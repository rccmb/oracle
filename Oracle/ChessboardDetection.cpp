#include "ChessboardDetection.h"

double SampleCellCenter(const cv::Mat& gray, int x, int y) {
    int patch = g_debugPatchSize;
    int offsetX = g_debugOffsetX;
    int offsetY = g_debugOffsetY;

    int startingX = x - (patch / 2) + offsetX;
    int startingY = y - (patch / 2) + offsetY;

    cv::Rect roi(startingX, startingY, patch, patch);
    roi &= cv::Rect(0, 0, gray.cols, gray.rows);

    std::cout << "[DEBUG] Sampling cell at (" << x << ", " << y << ") with patch size " << patch << " and offsets X=" << offsetX << ", Y=" << offsetY << " resulting in ROI (" << roi.x << ", " << roi.y << ", " << roi.width << ", " << roi.height << ")\n";

    return cv::mean(gray(roi))[0];
}

double CompareEdges(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size()) return 1e9;
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    return cv::sum(diff)[0];
}

std::map<std::string, cv::Mat> LoadReferencePieces(LPCWSTR tempDir) {
    std::map<std::string, cv::Mat> refs;
    for (const auto& entry : std::filesystem::directory_iterator(tempDir)) {
        if (entry.path().extension() == ".png") {
            refs[entry.path().stem().string()] = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
        }
    }
    return refs;
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    HWND hwndOverlay = (HWND)param;
    HWND hwndDesktop = GetDesktopWindow();
    
    // Wait for analysis to start
    while (!g_hasAnalysisStarted) {
        Sleep(100);
    }
    
    std::cout << "[INFO] ChessboardDetectionThread started analysis" << std::endl;
    
    // Board dimensions.
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
    
    cv::Mat screenshot = HWND2MAT(hwndDesktop);
    
    // Extract reference values from the analysis
    /*std::vector<double> sampleValues;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
            double value = SampleCellCenter(screenshot, cellCenterX, cellCenterY);
            sampleValues.push_back(value);
        }
    }
    
    double refBlack = *std::min_element(sampleValues.begin(), sampleValues.end());
    double refWhite = *std::max_element(sampleValues.begin(), sampleValues.end());
    int tolerance = 15;
    
    std::cout << "[INFO] Reference values - Black: " << refBlack << ", White: " << refWhite << std::endl;*/
    
    // Continuous analysis loop
    while (g_hasAnalysisStarted) {
        cv::Mat frame = HWND2MAT(hwndDesktop);
        std::string fen;
        int globalEmptyCount = 0;
        int globalBlackCount = 0;
        int globalWhiteCount = 0;
        
        for (int row = 0; row < 8; ++row) {
            int emptyCount = 0;
            for (int col = 0; col < 8; ++col) {
                int cx = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
                int cy = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);

                // Use configured values for sampling
                double val = SampleCellCenter(frame, cx, cy);

                // Piece color detection
                /*bool isBlack = std::abs(val - refBlack) < tolerance;
                bool isWhite = std::abs(val - refWhite) < tolerance;*/

                std::cout << "[DEBUG] Cell (" << row << ", " << col << ") brightness: " << val 
                    << (g_refBlackPiece ? " [Black]" : g_refWhitePiece ? " [White]" : " [Empty]") << std::endl;

                // Count pieces for statistics
                if (g_refBlackPiece) {
                    globalBlackCount++;
                } else if (g_refWhitePiece) {
                    globalWhiteCount++;
                } else {
                    globalEmptyCount++;
                }

                // Build FEN notation
                if (!g_refBlackPiece && !g_refWhitePiece) {
                    ++emptyCount;
                }
                else {
                    if (emptyCount > 0) {
                        fen += std::to_string(emptyCount);
                        emptyCount = 0;
                    }
                    fen += (g_refBlackPiece ? "b" : "w"); // Simple piece representation
                }
            }

            if (emptyCount > 0) fen += std::to_string(emptyCount);
            if (row < 7) fen += '/';
        }

        std::cout << "[FEN] " << fen << std::endl;
        std::cout << "[STATS] Pieces detected: Black = " << globalBlackCount << ", White = " << globalWhiteCount << ", Empty = " << globalEmptyCount << std::endl;

        // Send update message to overlay
        PostMessage(hwndOverlay, WM_CHESSBOARD_DETECTED, NULL, NULL);

        Sleep(1000); // Analyze every second
    }

    std::cout << "[INFO] ChessboardDetectionThread stopped" << std::endl;
    return 0;
}

