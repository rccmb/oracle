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

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
	std::cout << "[INFO] Chessboard detection thread started.\n";
    HWND hwndOverlay = (HWND)param;
    HWND hwndDesktop = GetDesktopWindow();
    
    // Board dimensions.
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    std::map<std::string, cv::Mat> refs = LoadReferencePieces(g_tempDir);

    // Continuous analysis loop.
    while(true) {
        while (g_hasAnalysisStarted) {
            cv::Mat frame = HWND2MAT(hwndDesktop);
            std::fill(g_detectedLetters.begin(), g_detectedLetters.end(), ' ');
            std::fill(g_boardGridRows.begin(), g_boardGridRows.end(), std::string(8, ' '));

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

                    // Build crop ROI.
                    int x = cx - (g_cropPatchSize / 2) + g_cropOffsetX;
                    int y = cy - (g_cropPatchSize / 2) + g_cropOffsetY;
                    cv::Rect roi(x, y, g_cropPatchSize, g_cropPatchSize);
                    roi &= cv::Rect(0, 0, frame.cols, frame.rows);
                    if (roi.width <= 0 || roi.height <= 0) continue;

                    // Prepare cell edges.
                    cv::Mat cell = frame(roi).clone();
                    cv::Mat cellEdges;
                    cv::Canny(cell, cellEdges, 50, 150);

                    // Compare against references.
                    double bestScore = 1e18;
                    char bestLetter = looksBlack ? 'p' : 'P';
                    for (const auto& kv : refs) {
                        const std::string& name = kv.first;
                        const cv::Mat& ref = kv.second;
                        if (ref.empty()) continue;

                        bool hasWhitePrefix = name.rfind("white_", 0) == 0;
                        bool hasBlackPrefix = name.rfind("black_", 0) == 0;
                        if (looksBlack && hasWhitePrefix) continue;
                        if (looksWhite && hasBlackPrefix) continue;

                        double score = CompareEdges(cellEdges, ref);
                        if (score < bestScore) {
                            bestScore = score;
                            if (name.find(looksBlack ? '_p' : '_P') != std::string::npos) bestLetter = looksBlack ? 'p' : 'P';
                            else if (name.find(looksBlack ? '_n' : '_N') != std::string::npos) bestLetter = looksBlack ? 'n' : 'N';
                            else if (name.find(looksBlack ? '_b' : '_B') != std::string::npos) bestLetter = looksBlack ? 'b' : 'B';
                            else if (name.find(looksBlack ? '_r' : '_R') != std::string::npos) bestLetter = looksBlack ? 'r' : 'R';
                            else if (name.find(looksBlack ? '_q' : '_Q') != std::string::npos) bestLetter = looksBlack ? 'q' : 'Q';
                            else if (name.find(looksBlack ? '_k' : '_K') != std::string::npos) bestLetter = looksBlack ? 'k' : 'K';
                        }
                    }

                    double maxPossible = 255.0 * roi.width * roi.height;
                    double similarity = 1.0 - std::min(bestScore / maxPossible, 1.0);
                    if (similarity >= 0.90) {
                        g_detectedLetters[idx] = bestLetter;
                    }

                }
            }

            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    g_boardGridRows[r][c] = g_detectedLetters[r * 8 + c];
                }
            }

            std::cout << "Frame processed.\n";

            Sleep(10);
        }

        Sleep(10);
    }

    return 0;
}

