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

// One scale for comparing two evaluations, with mates pushed to the extremes so
// that a faster mate still outranks a slower one. Always in White's terms.
static int ComparableScore(bool isMate, int mateInWhite, int cpWhite) {
    if (!isMate) return cpWhite;
    return (mateInWhite > 0) ? (100000 - mateInWhite * 100)
                             : (-100000 - mateInWhite * 100);
}

// The side to move named by a FEN.
static bool ScoreSideToMoveIsWhite(const std::string& fen) {
    const size_t space = fen.find(' ');
    if (space == std::string::npos || space + 1 >= fen.size()) return true;
    return fen[space + 1] != 'b';
}

bool BuildObservedBoard(const std::vector<std::string>& rows, int orientation, char out[64]) {
    if (rows.size() != 8) return false;

    // Re-order the detected grid into FEN order first: rank 8 at index 0 down to
    // rank 1 at index 7, files a to h. Everything downstream names actual
    // squares, which the raw grid cannot do because its orientation depends on
    // which way round the board is drawn.
    for (int fenRank = 0; fenRank < 8; ++fenRank) {
        const int rowIndex = (orientation == 0) ? fenRank : (7 - fenRank);
        for (int file = 0; file < 8; ++file) {
            const int colIndex = (orientation == 0) ? file : (7 - file);
            char piece = ' ';
            if (rowIndex >= 0 && rowIndex < (int)rows.size() &&
                colIndex >= 0 && colIndex < (int)rows[rowIndex].size()) {
                piece = rows[rowIndex][colIndex];
            }
            out[fenRank * 8 + file] = (piece == '\0') ? ' ' : piece;
        }
    }

    auto at = [&](int rank, int file) { return out[rank * 8 + file]; };

    const int kRank8 = 0;
    const int kRank1 = 7;

    int whiteKings = 0, blackKings = 0;
    int whitePieces = 0, blackPieces = 0;
    int whitePawns = 0, blackPawns = 0;
    int whiteKingRank = -1, whiteKingFile = -1;
    int blackKingRank = -1, blackKingFile = -1;

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            const char piece = at(rank, file);
            if (piece == ' ') continue;

            // A pawn cannot stand on the first or last rank; it would have
            // promoted. Seeing one means a square was misread.
            if ((piece == 'P' || piece == 'p') && (rank == kRank8 || rank == kRank1)) return false;

            if (piece >= 'A' && piece <= 'Z') {
                ++whitePieces;
                if (piece == 'P') ++whitePawns;
                if (piece == 'K') { ++whiteKings; whiteKingRank = rank; whiteKingFile = file; }
            }
            else {
                ++blackPieces;
                if (piece == 'p') ++blackPawns;
                if (piece == 'k') { ++blackKings; blackKingRank = rank; blackKingFile = file; }
            }
        }
    }

    if (whiteKings != 1 || blackKings != 1) return false;
    if (whitePieces > 16 || blackPieces > 16) return false;
    if (whitePawns > 8 || blackPawns > 8) return false;

    // Kings cannot stand next to each other. Cheap, and a reliable sign that a
    // square was read wrong.
    if (std::abs(whiteKingRank - blackKingRank) <= 1 &&
        std::abs(whiteKingFile - blackKingFile) <= 1) {
        return false;
    }

    return true;
}

std::string BoardToFEN() {
    char board[64];
    if (!BuildObservedBoard(g_boardGridRows, g_orientation, board)) return std::string();

    char squares[8][8];
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) squares[rank][file] = board[rank * 8 + file];
    }

    auto at = [&](int rank, int file) { return squares[rank][file]; };

    // Rank 8 is index 0, rank 1 is index 7.
    const int kRank8 = 0;
    const int kRank1 = 7;

    std::ostringstream fen;
    for (int rank = 0; rank < 8; ++rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            const char piece = at(rank, file);
            if (piece == ' ') {
                ++emptyCount;
                continue;
            }
            if (emptyCount > 0) { fen << emptyCount; emptyCount = 0; }
            fen << piece;
        }
        if (emptyCount > 0) fen << emptyCount;
        if (rank < 7) fen << '/';
    }

    fen << ' ' << g_sideToMove << ' ';

    // Castling rights are read off the board rather than assumed. Oracle used to
    // write "KQkq" unconditionally, which crashes Stockfish outright: claiming a
    // right for a side that has no rook to castle with segfaults the engine,
    // because its setup scans the back rank for that rook and runs off the board.
    // Rooks are gone in most endgames, which is why the engine tended to die
    // around the end of a game rather than the middle of one.
    //
    // This over-grants in one case, a king that moved and came back, and that is
    // the safe direction: Stockfish discards a right it cannot use.
    std::string castling;
    if (at(kRank1, 4) == 'K') {
        if (at(kRank1, 7) == 'R') castling += 'K';
        if (at(kRank1, 0) == 'R') castling += 'Q';
    }
    if (at(kRank8, 4) == 'k') {
        if (at(kRank8, 7) == 'r') castling += 'k';
        if (at(kRank8, 0) == 'r') castling += 'q';
    }
    fen << (castling.empty() ? "-" : castling);

    // En passant and the halfmove clock are not recoverable from one frame.
    fen << " - 0 1";

    std::string result = fen.str();
    if ((int)std::count(result.begin(), result.end(), '/') != 7) return std::string();

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
        bool isWhite = false;
        bool isLight = false;   // Shade of square the reference was captured on.
        bool hasShade = false;  // False for references saved before shades existed.
        cv::Mat base;           // As captured.
        cv::Mat scaled;         // Resized to the board's current squares.
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

        // Named "<colour>_<L|D>_<piece>", so the shade sits right after the
        // colour and the piece letter stays last.
        if (name.size() > 6 && (name[6] == 'L' || name[6] == 'D')) {
            data.hasShade = true;
            data.isLight = (name[6] == 'L');
        }

        data.base = refImg;
        data.scaled = refImg;
        refTemplates.push_back(std::move(data));
    }

    // Size the references were last resized to, so the work happens on a change
    // rather than every frame.
    int templatesScaledFor = -1;

    std::fill(g_boardGridRows.begin(), g_boardGridRows.end(), std::string(8, ' '));

    // Room around the board so a crop widened by searchPadding still falls
    // inside the captured region rather than being clipped at its edge.
    const int searchPadding = 5;

    cv::Mat previousFrameColor;

    // The game itself, followed across frames.
    GameTracker tracker;

    // What the position was worth the last time a search settled, and who was to
    // move in it. Comparing that against the next settled score is what turns an
    // evaluation into a judgement about the move that was played.
    int previousEvaluatedPly = -1;
    int previousScore = 0;
    bool previousWhiteToMove = true;
    int previousVerdictPly = -1;

    // Continuous analysis loop.
    while(true) {
        while (g_hasAnalysisStarted) {
            // A fresh detection means a different game, or at least a board the
            // tracker has no business reconciling against the old one.
            if (g_trackerResetRequested) {
                g_trackerResetRequested = false;
                tracker.Reset();
                previousFrameColor.release();
                g_liveEvalValid.store(false);
                previousEvaluatedPly = -1;
                previousVerdictPly = -1;

                std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                g_trackedFen = tracker.Position().ToFen();
                g_trackerPly = 0;
                g_trackerInSync = true;
                g_lastMoveUci.clear();
                g_sfBestMoves.clear();
            }

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

            // References are captured at whatever the squares measured when the
            // board was detected. Rescaling them to the squares as they are now
            // means a browser zoom or a resized window keeps working instead of
            // silently matching nothing until the board is detected again.
            const int desiredCrop = std::max(8, (std::min(cellWidth, cellHeight) * 78) / 100);
            if (desiredCrop != templatesScaledFor) {
                for (RefTemplateData& reference : refTemplates) {
                    if (reference.base.empty()) continue;
                    if (reference.base.cols == desiredCrop) {
                        reference.scaled = reference.base;
                    }
                    else {
                        cv::resize(reference.base, reference.scaled,
                                   cv::Size(desiredCrop, desiredCrop), 0, 0, cv::INTER_AREA);
                    }
                }
                templatesScaledFor = desiredCrop;
            }

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
                    if (g_cropRects.size() == 64 && g_cropRects[idx].width == desiredCrop) {
                        // Stored in screen coordinates for the overlay to draw,
                        // so shift them into the captured region.
                        const SAMPLE& s = g_cropRects[idx];
                        roi = cv::Rect(s.x - captureLeft, s.y - captureTop, s.width, s.height);
                    } else {
                        // The board has changed size since those were built, so
                        // derive the crop from the squares as they are now. Any
                        // offsets the user set by hand still apply.
                        const int x = cx - (desiredCrop / 2) + g_cropOffsetX;
                        const int y = cy - (desiredCrop / 2) + g_cropOffsetY;
                        roi = cv::Rect(x, y, desiredCrop, desiredCrop);
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

                    // Which shade this square is. Turning a board around maps a8
                    // onto h1 and both are light, so screen parity gives the
                    // answer whichever way the board is drawn.
                    const bool squareIsLight = ((row + col) % 2 == 0);

                    // Two passes: references captured on this shade of square
                    // first, and only if none of them matched, the ones captured
                    // on the other. The king and queen only ever stand on one
                    // shade at the start, so for those the second pass is the
                    // only pass there is.
                    for (int pass = 0; pass < 2 && bestMatch < g_matchThreshold; ++pass) {
                        for (const auto& rd : refTemplates) {
                            if (rd.isWhite && looksBlack) continue;
                            if (!rd.isWhite && looksWhite) continue;

                            if (rd.hasShade) {
                                const bool wanted = (pass == 0) ? squareIsLight : !squareIsLight;
                                if (rd.isLight != wanted) continue;
                            }
                            else if (pass == 1) {
                                continue; // Shadeless references are tried once.
                            }

                            if (cellGray.cols < rd.scaled.cols || cellGray.rows < rd.scaled.rows) {
                                // Failsafe in case the screen bounds clip the ROI.
                                continue;
                            }

                            cv::Mat result;
                            cv::matchTemplate(cellGray, rd.scaled, result, cv::TM_CCOEFF_NORMED);

                            double minVal, maxVal;
                            cv::minMaxLoc(result, &minVal, &maxVal);

                            if (maxVal > bestMatch) {
                                bestMatch = maxVal;
                                bestName = rd.name;
                                bestLetter = bestName.back();
                            }
                        }
                    }

                    // TODO: Make similarity editable with ImGui.
                    if (bestMatch >= g_matchThreshold) {
                        g_detectedLetters[idx] = bestLetter;
                    }
                }
            }


            // Reconcile what was seen against the game so far. The frame is an
            // observation, not an answer: the tracker decides which legal move,
            // if any, explains it, and rejects the frame outright when none
            // does. Animations and covered squares therefore cost nothing.
            std::vector<std::string> observedRows(8);
            for (int row = 0; row < 8; ++row) {
                std::string rowStr;
                rowStr.reserve(8);
                for (int col = 0; col < 8; ++col) rowStr.push_back(g_detectedLetters[row * 8 + col]);
                observedRows[row] = rowStr;
            }

            // A board has been scanned, whatever it turned out to say. The
            // preview is gated on this, and gating it on the tracker accepting
            // the frame instead meant one unreadable frame blanked the whole
            // analysis window until the tracker happened to agree again.
            g_noBoard = false;

            char observed[64];
            const bool observationUsable = BuildObservedBoard(observedRows, g_orientation, observed);
            const TrackerOutcome outcome = observationUsable
                ? tracker.Observe(observed)
                : TrackerOutcome::Unreadable;

            if (outcome == TrackerOutcome::Unreadable) {
                // Show what the detector is actually reading rather than freezing
                // on the last agreed position. When the two disagree, seeing the
                // raw read is what tells you which square is being misread.
                std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                g_boardGridRows = observedRows;
                if (tracker.UnreadableStreak() > 30) g_trackerInSync = false;
            }
            else {
                // Once the tracker agrees, the preview shows the tracked
                // position rather than the raw read, so a square misread for a
                // single frame does not flicker in it.
                std::vector<std::string> trackedRows(8, std::string(8, ' '));
                for (int fenRank = 0; fenRank < 8; ++fenRank) {
                    for (int file = 0; file < 8; ++file) {
                        const int rowIndex = (g_orientation == 0) ? fenRank : (7 - fenRank);
                        const int colIndex = (g_orientation == 0) ? file : (7 - file);
                        trackedRows[rowIndex][colIndex] = tracker.Position().PieceAt(fenRank * 8 + file);
                    }
                }

                const std::string trackedFen = tracker.Position().ToFen();
                const bool ours = (tracker.Position().SideToMove() == Color::White) == g_sfPlayWhite;

                std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                g_boardGridRows = std::move(trackedRows);
                g_trackedFen = trackedFen;
                g_trackerPly = tracker.Ply();
                g_trackerInSync = true;
                g_sfMovesAreOurs = ours;
                g_sideToMove = (tracker.Position().SideToMove() == Color::White) ? 'w' : 'b';
                g_lastMoveUci = tracker.History().empty() ? std::string()
                                                          : tracker.History().back().uci;
            }

            if (StockfishIsAlive()) {
                std::string fen;
                std::string movePlayed;
                int plyNow = 0;
                {
                    std::lock_guard<std::mutex> snapshot(g_analysisStateMutex);
                    fen = g_trackedFen;
                    plyNow = g_trackerPly;
                    movePlayed = g_lastMoveUci;
                }

                static std::string lastQueriedFen;
                if (!fen.empty() && fen != lastQueriedFen) {
                    const bool whiteToMoveHere = ScoreSideToMoveIsWhite(fen);

                    // The engine call blocks, so it runs outside the lock and only
                    // its result is published.
                    std::vector<StockfishMove> moves =
                        GetBestMoves(fen, g_sfElo, g_sfNumberMoves, g_sfMoveDepth);

                    // What the position is worth now that the search has settled,
                    // on one scale that puts mates at the extremes so a move can
                    // be compared against the one before it.
                    const int scoreNow = ComparableScore(
                        g_liveEvalIsMate.load(), g_liveEvalMateInWhite.load(), g_liveEvalCpWhite.load());

                    // Attribute the change to whoever moved. Analysing both sides'
                    // turns is what makes this possible: the position before their
                    // move and the position after it have both been evaluated.
                    std::string verdict;
                    int lossCp = 0;
                    bool byUs = false;

                    if (previousEvaluatedPly >= 0 && plyNow == previousEvaluatedPly + 1 &&
                        g_liveEvalValid.load()) {
                        // Positive means the side that moved is worse off than
                        // before. Scores are in White's terms, so Black's loss is
                        // the same difference the other way round.
                        lossCp = previousWhiteToMove
                            ? (previousScore - scoreNow)
                            : (scoreNow - previousScore);

                        // A game already decided does not get worse in a way worth
                        // calling out: going from mate in two to mate in five is
                        // not a blunder.
                        const bool alreadyDecided =
                            (previousScore > 1500 && scoreNow > 1500) ||
                            (previousScore < -1500 && scoreNow < -1500);

                        if (!alreadyDecided) {
                            if (lossCp >= 300) verdict = "Blunder";
                            else if (lossCp >= 150) verdict = "Mistake";
                            else if (lossCp >= 80) verdict = "Inaccuracy";
                        }

                        byUs = (previousWhiteToMove == g_sfPlayWhite);
                    }

                    previousEvaluatedPly = plyNow;
                    previousScore = scoreNow;
                    previousWhiteToMove = whiteToMoveHere;

                    std::lock_guard<std::mutex> publish(g_analysisStateMutex);
                    g_sfBestMoves = std::move(moves);
                    lastQueriedFen = fen;

                    if (!verdict.empty()) {
                        g_lastMoveVerdict = verdict;
                        g_lastMoveLossCp = lossCp;
                        g_lastMoveVerdictByUs = byUs;
                        g_lastMoveVerdictUci = movePlayed;
                    }
                    else if (plyNow != previousVerdictPly) {
                        // A sound move clears the previous callout rather than
                        // leaving it on screen for the rest of the game.
                        g_lastMoveVerdict.clear();
                        g_lastMoveLossCp = 0;
                        g_lastMoveVerdictUci.clear();
                    }
                    previousVerdictPly = plyNow;
                }
            }
        }

        Sleep(10);
    }

    return 0;
}

