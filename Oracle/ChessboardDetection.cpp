#include "ChessboardDetection.h"

#include <cstring>

double SampleCellCenter(const cv::Mat& gray, int x, int y, int offsetX, int offsetY) {
    int patch = g_debugPatchSize;

    int startingX = x - (patch / 2) + offsetX;
    int startingY = y - (patch / 2) + offsetY;

    cv::Rect roi(startingX, startingY, patch, patch);
    roi &= cv::Rect(0, 0, gray.cols, gray.rows);

    return cv::mean(gray(roi))[0];
}

double CompareEdges(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size()) return 1e9;
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    return cv::sum(diff)[0];
}

std::string BoardToFEN() {
    std::ostringstream fen;

    if (g_boardGridRows.size() != 8) {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }

    bool hasWhiteKing = false;
    bool hasBlackKing = false;

    for (int fenRank = 0; fenRank < 8; ++fenRank) {
        int rowIndex = (g_orientation == 0) ? fenRank : (7 - fenRank);
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            int colIndex = (g_orientation == 0) ? file : (7 - file);
            char piece = ' ';
            if (rowIndex >= 0 && rowIndex < (int)g_boardGridRows.size() &&
                colIndex >= 0 && colIndex < (int)g_boardGridRows[rowIndex].size()) {
                piece = g_boardGridRows[rowIndex][colIndex];
            }
            if (piece == ' ' || piece == '\0') {
                ++emptyCount;
            }
            else {
                if (emptyCount > 0) { fen << emptyCount; emptyCount = 0; }
                fen << piece;

                if (piece == 'K') hasWhiteKing = true;
                else if (piece == 'k') hasBlackKing = true;
            }
        }

        if (emptyCount > 0) fen << emptyCount;
        if (fenRank < 7) fen << '/';
    }

    if (!hasWhiteKing || !hasBlackKing) {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }

    fen << ' ' << g_sideToMove;

    fen << " KQkq";

    fen << " -";

    fen << " 0 1";

    std::string result = fen.str();

    int slashCount = (int)std::count(result.begin(), result.end(), '/');
    if (slashCount != 7) {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }

	// All validations passes, update last valid FEN.
    {
        std::lock_guard<std::mutex> publish(g_analysisStateMutex);
        g_lastValidFEN = result;
    }
    return result;
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    HWND hwndOverlay = (HWND)param;

    std::map<std::string, cv::Mat> refs = LoadReferencePieces(g_tempDir);

    // TEMPLATE MATCHING PREPROCESSING OF REFERENCES.
    struct RefTemplateData {
        std::string name;
        bool isWhite;
        cv::Mat tpl;
    };

    std::vector<RefTemplateData> refTemplates;
    refTemplates.reserve(refs.size());

    for (const auto& kv : refs) {
        const std::string& name = kv.first;
        const cv::Mat& refImg = kv.second;
        if (refImg.empty()) continue;

        RefTemplateData data;
        data.name = name;
        data.isWhite = (name.rfind("white_", 0) == 0);
        data.tpl = refImg;
        refTemplates.push_back(std::move(data));
    }

    std::fill(g_boardGridRows.begin(), g_boardGridRows.end(), std::string(8, ' '));

    // Room around the board so a crop widened by searchPadding still falls
    // inside the captured region rather than being clipped at its edge.
    const int searchPadding = 5;

    cv::Mat previousFrameColor;

    // Continuous analysis loop.
    while(true) {
        while (g_hasAnalysisStarted) {
            // Re-read the board every pass. These used to be computed once when
            // the thread started, so re-detecting a board of a different size, or
            // at a different place, had no effect until Oracle was restarted.
            const int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
            const int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
            if (cellWidth <= 0 || cellHeight <= 0) {
                Sleep(50);
                continue;
            }

            // Only the board is captured, not the whole desktop. The old full
            // screen grab copied tens of megabytes per pass on a large display,
            // in a loop that had no sleep in it, to read sixty-four squares.
            const int captureLeft = g_boardRect.left - searchPadding;
            const int captureTop = g_boardRect.top - searchPadding;
            const int captureWidth = (g_boardRect.right - g_boardRect.left) + 2 * searchPadding;
            const int captureHeight = (g_boardRect.bottom - g_boardRect.top) + 2 * searchPadding;

            cv::Mat frameColor = CaptureScreenRegion(captureLeft, captureTop, captureWidth, captureHeight);
            if (frameColor.empty()) {
                Sleep(50);
                continue;
            }

            // Nothing on the board changed, so neither would the result. Pieces
            // only move when pixels do, and between moves that is every frame.
            if (!previousFrameColor.empty() &&
                previousFrameColor.size() == frameColor.size() &&
                previousFrameColor.type() == frameColor.type() &&
                previousFrameColor.isContinuous() && frameColor.isContinuous() &&
                std::memcmp(previousFrameColor.data, frameColor.data, frameColor.total() * frameColor.elemSize()) == 0) {
                Sleep(10);
                continue;
            }
            previousFrameColor = frameColor.clone();

            cv::Mat frame;

            // Convert the frame to grayscale.
            cv::cvtColor(frameColor, frame, cv::COLOR_BGR2GRAY);

            std::fill(g_detectedLetters.begin(), g_detectedLetters.end(), ' ');

            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int idx = row * 8 + col;

                    // Cell centres in the captured region's own coordinates.
                    int cx = (g_boardRect.left - captureLeft) + (col * cellWidth) + (cellWidth / 2);
                    int cy = (g_boardRect.top - captureTop) + (row * cellHeight) + (cellHeight / 2);

                    // Determine occupancy by brightness proximity.
                    int offsetsX[3] = { g_debugOffsetX, g_debugOffsetX2, g_debugOffsetX3 };
                    int offsetsY[3] = { g_debugOffsetY, g_debugOffsetY2, g_debugOffsetY3 };

                    bool isPiece = false;
                    bool allLookLikeBoard = true;
                    bool looksBlack = false;
                    bool looksWhite = false;

                    for (int s = 0; s < 3; ++s) {
                        double val = SampleCellCenter(frame, cx, cy, offsetsX[s], offsetsY[s]);
                        bool localLooksBlack = (g_refBlackPiece >= 0) && (std::abs(val - g_refBlackPiece) <= g_analysisTolerance);
                        bool localLooksWhite = (g_refWhitePiece >= 0) && (std::abs(val - g_refWhitePiece) <= g_analysisTolerance);
                        
                        if (localLooksBlack || localLooksWhite) {
                            looksBlack = localLooksBlack;
                            looksWhite = localLooksWhite;
                            isPiece = true;
                            break;
                        }

                        uchar board1 = (uchar)g_refBoardColor1;
                        uchar board2 = (uchar)g_refBoardColor2;
                        bool looksBoard1 = (std::abs(val - board1) <= g_analysisTolerance);
                        bool looksBoard2 = (std::abs(val - board2) <= g_analysisTolerance);

                        if (!looksBoard1 && !looksBoard2) {
                            allLookLikeBoard = false;
                        }
                    }

                    if (!isPiece && allLookLikeBoard) {
                        continue;
                    }

                    cv::Rect roi;
                    if (g_cropRects.size() == 64) {
                        // Stored in screen coordinates for the overlay to draw,
                        // so shift them into the captured region.
                        const SAMPLE& s = g_cropRects[idx];
                        roi = cv::Rect(s.x - captureLeft, s.y - captureTop, s.width, s.height);
                    } else {
                        int x = cx - (g_cropPatchSize / 2) + g_cropOffsetX;
                        int y = cy - (g_cropPatchSize / 2) + g_cropOffsetY;
                        roi = cv::Rect(x, y, g_cropPatchSize, g_cropPatchSize);
                    }

                    // Widen the crop so template matching can find a piece that
                    // sits a little off centre in its square.
                    roi.x -= searchPadding;
                    roi.y -= searchPadding;
                    roi.width += 2 * searchPadding;
                    roi.height += 2 * searchPadding;

                    roi &= cv::Rect(0, 0, frame.cols, frame.rows);
                    if (roi.width <= 0 || roi.height <= 0) continue;

                    cv::Mat cellGray = frame(roi).clone();
                    
                    // Apply color palette masking.
                    if (!frameColor.empty()) {
                        cv::Mat cellColor = frameColor(roi).clone();
                        cellGray = ApplyPaletteMasking(cellColor);
                    }
                    
                    double bestMatch = -1.0;
                    char bestLetter = looksBlack ? 'p' : 'P';
                    std::string bestName;

                    for (const auto& rd : refTemplates) {
                        if (rd.isWhite && looksBlack) continue;
                        if (!rd.isWhite && looksWhite) continue;

                        if (cellGray.cols < rd.tpl.cols || cellGray.rows < rd.tpl.rows) {
                            // Failsafe in case the user shrinks the crop size in UI after saving templates, or screen bounds clip the ROI
                            continue;
                        }

                        cv::Mat result;
                        cv::matchTemplate(cellGray, rd.tpl, result, cv::TM_CCOEFF_NORMED);
                        
                        double minVal, maxVal;
                        cv::minMaxLoc(result, &minVal, &maxVal);

                        if (maxVal > bestMatch) {
                            bestMatch = maxVal;
                            bestName = rd.name;
                            bestLetter = bestName.back();
                        }
                    }

                    // TODO: Make similarity editable with ImGui.
                    if (bestMatch >= g_matchThreshold) {
                        g_detectedLetters[idx] = bestLetter;
                    }
                }
            }

            // Detect changes and update board rows only when state changes.
            bool changed = false;
            if (g_detectedLetters.size() != g_prevLetterDrawQueue.size()) {
                changed = true;
            }
            else {
                char detectedSide = '\0';
                int evidenceRank = 0;

                for (size_t i = 0; i < g_detectedLetters.size(); ++i) {
                    char curCh = g_detectedLetters[i];
                    char prevCh = (i < g_prevLetterDrawQueue.size()) ? g_prevLetterDrawQueue[i] : ' ';

                    if (curCh == prevCh)
                        continue;

                    // TODO: En passant detection. TWO PAWNS DISAPPEARED, ONE PAWN APPEARED.
                    // TODO: Add support for when the user reverts the game. As in, if there is one more piece detected, we should do nothing, just keep scanning the board.
                    // - We will have to keep the very last FEN, so that we know where to resume scanning the board for plays.
                    // - Also verify the side switching problem, sometimes it's erroneous.

					// Piece disappeared. Whoever was here, moved for this turn.
                    if (prevCh != ' ' && curCh == ' ') {
                        g_sideToMove = (prevCh >= 'A' && prevCh <= 'Z') ? 'b' : 'w';
                        changed = true;
                        break;
                    }
					// Piece appeared. Whoever is here, moved for this turn.
                    else if (prevCh == ' ' && curCh != ' ') {
                        g_sideToMove = (curCh >= 'A' && curCh <= 'Z') ? 'b' : 'w';
                        changed = true;
                        break;
                    }
                    // Piece changed. Promotion, capture or castling, whoever is here, moved for this turn.
                    else if (prevCh != curCh) {
                        g_sideToMove = (curCh >= 'A' && curCh <= 'Z') ? 'b' : 'w';
                        changed = true;
                        break;
                    }
                }
            }

            if (changed) {
                g_prevLetterDrawQueue = g_detectedLetters;

                // Build grid rows from detected letters.
                std::vector<std::string> newRows(8);
                for (int row = 0; row < 8; ++row) {
                    std::string rowStr;
                    rowStr.reserve(8);
                    for (int col = 0; col < 8; ++col) {
                        rowStr.push_back(g_detectedLetters[row * 8 + col]);
                    }
                    newRows[row] = rowStr;
                }

                {
                    std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                    g_boardGridRows = std::move(newRows);
                }
                g_noBoard = false;
            }

            if (StockfishIsAlive()) {
                std::string fen = BoardToFEN();
                static std::string lastQueriedFen;
                if (fen != lastQueriedFen) {
                    // The engine call blocks, so it runs outside the lock and only
                    // its result is published.
                    std::vector<StockfishMove> moves =
                        GetBestMoves(fen, g_sfElo, g_sfNumberMoves, g_sfMoveDepth);

                    std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                    g_sfBestMoves = std::move(moves);
                    lastQueriedFen = fen;
                }
            }
        }

        Sleep(10);
    }

    return 0;
}

