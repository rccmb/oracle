#include "ChessboardDetection.h"

double SampleCellCenter(const cv::Mat& gray, int x, int y) {
    int patch = g_debugPatchSize;
    int offsetX = g_debugOffsetX;
    int offsetY = g_debugOffsetY;

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

    // Determine row iteration based on orientation.
    int start = (g_orientation == 0) ? 0 : 7;
    int end = (g_orientation == 0) ? 8 : -1;
    int step = (g_orientation == 0) ? 1 : -1;

    for (int row = start; row != end; row += step) {
        int emptyCount = 0;
        for (int col = 0; col < 8; ++col) {
            char piece = g_boardGridRows[row][col];
            if (piece == ' ' || piece == '\0') {
                emptyCount++;
            }
            else {
                if (emptyCount > 0) {
                    fen << emptyCount;
                    emptyCount = 0;
                }
                fen << piece;

                if (piece == 'K') hasWhiteKing = true;
                if (piece == 'k') hasBlackKing = true;
            }
        }
        if (emptyCount > 0) fen << emptyCount;
        if (row != (step > 0 ? end - 1 : end + 1)) fen << '/';
    }

    if (!hasWhiteKing || !hasBlackKing) {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }

    fen << ' ' << (g_sfPlayWhite ? 'w' : 'b');

    fen << " KQkq";

    fen << " -";

    fen << " 0 1";

    std::string result = fen.str();

    int slashCount = (int)std::count(result.begin(), result.end(), '/');
    if (slashCount != 7) {
        return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    }

    return result;
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    HWND hwndOverlay = (HWND)param;
    HWND hwndDesktop = GetDesktopWindow();
    
    // Board dimensions.
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    std::map<std::string, cv::Mat> refs = LoadReferencePieces(g_tempDir);

    // CHAMFER PREPROCESSING OF REFERENCES. DISTANCE TRANSFORMS.
    struct RefChamferData {
        std::string name;
        bool isWhite;
        cv::Mat dt;
        cv::Size size;
    };

    std::vector<RefChamferData> refChamfers;
    refChamfers.reserve(refs.size());

    const int chamferDilate = 1;
    cv::Mat dilateKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(chamferDilate * 2 + 1, chamferDilate * 2 + 1));

    for (const auto& kv : refs) {
        const std::string& name = kv.first;
        const cv::Mat& refImg = kv.second;
        if (refImg.empty()) continue;

        cv::Mat refEdges;
        cv::threshold(refImg, refEdges, 0, 255, cv::THRESH_BINARY);
        if (chamferDilate > 0) {
            cv::dilate(refEdges, refEdges, dilateKernel);
        }

        // Distance to nearest edge pixel.
        cv::Mat refEdgesInv;
        cv::bitwise_not(refEdges, refEdgesInv);
        cv::Mat dt;
        cv::distanceTransform(refEdgesInv, dt, cv::DIST_L2, 3);

        RefChamferData data;
        data.name = name;
        data.isWhite = (name.rfind("white_", 0) == 0);
        data.dt = dt;
        data.size = dt.size();
        refChamfers.push_back(std::move(data));
    }

    std::fill(g_boardGridRows.begin(), g_boardGridRows.end(), std::string(8, ' '));

    // Continuous analysis loop.
    while(true) {
        while (g_hasAnalysisStarted) {
            cv::Mat frame = HWND2MAT(hwndDesktop);
            std::fill(g_detectedLetters.begin(), g_detectedLetters.end(), ' ');

            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int idx = row * 8 + col;
                    int cx = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
                    int cy = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);

                    // Determine occupancy by brightness proximity.
                    double val = SampleCellCenter(frame, cx, cy);
                    bool looksBlack = (g_refBlackPiece >= 0) && (std::abs(val - g_refBlackPiece) <= g_analysisTolerance);
                    bool looksWhite = (g_refWhitePiece >= 0) && (std::abs(val - g_refWhitePiece) <= g_analysisTolerance);
                    if (!looksBlack && !looksWhite) {
                        continue;
                    }

                    cv::Rect roi;
                    if (g_cropRects.size() == 64) {
                        const SAMPLE& s = g_cropRects[idx];
                        roi = cv::Rect(s.x, s.y, s.width, s.height);
                    } else {
                        int x = cx - (g_cropPatchSize / 2) + g_cropOffsetX;
                        int y = cy - (g_cropPatchSize / 2) + g_cropOffsetY;
                        roi = cv::Rect(x, y, g_cropPatchSize, g_cropPatchSize);
                    }
                    roi &= cv::Rect(0, 0, frame.cols, frame.rows);
                    if (roi.width <= 0 || roi.height <= 0) continue;

                    cv::Mat cellGray = frame(roi).clone();
                    cv::Mat cellEdges;
                    cv::Canny(cellGray, cellEdges, 50, 150);
                    cv::threshold(cellEdges, cellEdges, 0, 255, cv::THRESH_BINARY);

                    double bestChamfer = 1e18;
                    char bestLetter = looksBlack ? 'p' : 'P';
                    std::string bestName;

                    cv::Mat candidateResized;
                    std::vector<cv::Point> edgePoints;

                    for (const auto& rd : refChamfers) {
                        if (rd.isWhite && looksBlack) continue;
                        if (!rd.isWhite && looksWhite) continue;

                        if (candidateResized.size() != rd.size) {
                            cv::resize(cellEdges, candidateResized, rd.size, 0, 0, cv::INTER_NEAREST);
                        }

                        edgePoints.clear();
                        cv::findNonZero(candidateResized, edgePoints);
                        if (edgePoints.empty()) continue;

                        const cv::Mat& dt = rd.dt;
                        double sumDist = 0.0;
                        for (const cv::Point& p : edgePoints) {
                            int px = std::clamp(p.x, 0, dt.cols - 1);
                            int py = std::clamp(p.y, 0, dt.rows - 1);
                            sumDist += dt.at<float>(py, px);
                        }
                        double meanDist = sumDist / (double)edgePoints.size();

                        if (meanDist < bestChamfer) {
                            bestChamfer = meanDist;
                            bestName = rd.name;

                            char pieceChar = bestName.back(); 
                            bestLetter = pieceChar;
                        }
                    }

                    const double chamferScale = 3.0;
                    double similarity = std::exp(-bestChamfer / chamferScale);
                    if (similarity >= 0.90) {
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
                for (size_t i = 0; i < g_detectedLetters.size(); ++i) {
                    if (g_detectedLetters[i] != g_prevLetterDrawQueue[i]) {
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

                g_boardGridRows = std::move(newRows);
                g_noBoard = false;
            }
        }

        Sleep(10);
    }

    return 0;
}

