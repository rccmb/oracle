#include "ChessboardDetection.h"

// TODO: Document.
cv::Rect GetBoardROI(const cv::Mat& img, const CLICK& firstClick, const CLICK& secondClick) {
    int width = std::abs(secondClick.x - firstClick.x);
    int height = width;

    int roi_x = firstClick.x;
    int roi_y = firstClick.y;

    return cv::Rect(roi_x, roi_y, width, height);
}

// TODO: Document.
double SampleCellCenter(const cv::Mat& gray, int x, int y) {
    // Always use the configured values from sliders
    int patch = g_debugPatchSize;
    int offset = g_debugOffset;

    int half = patch / 2;

    int startingX = x - half;
    int startingY = y - half + offset;

    cv::Rect roi(startingX, startingY, patch, patch);
    roi &= cv::Rect(0, 0, gray.cols, gray.rows);

    SAMPLE sample;
    sample.x = startingX;
    sample.y = startingY;
    sample.width = patch;
    sample.height = patch;
    g_debugSamples.push_back(sample);

    std::cout << "[DEBUG] Sampling cell center at (" << x << ", " << y << ") with patch size " << patch << " and offset " << offset << " resulting in ROI (" << roi.x << ", " << roi.y << ", " << roi.width << ", " << roi.height << ")\n";

    return cv::mean(gray(roi))[0];
}

// TODO: Document.
double CompareEdges(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size()) return 1e9;
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    return cv::sum(diff)[0];
}

// TODO: Document.
std::map<std::string, cv::Mat> LoadReferencePieces(LPCWSTR tempDir) {
    std::map<std::string, cv::Mat> refs;
    for (const auto& entry : std::filesystem::directory_iterator(tempDir)) {
        if (entry.path().extension() == ".png") {
            refs[entry.path().stem().string()] = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
        }
    }
    return refs;
}

// TODO: Document.
int DetectBoardDimensions(cv::Mat screenshot) {
    if (screenshot.empty()) return 0;

    cv::Rect g_boardRectCV;

    // Validate the chessboard in the click-defined region.
    cv::Rect boardROI = GetBoardROI(screenshot, g_clicks.first, g_clicks.second);
    std::cout << "[DEBUG] Board ROI: (" << boardROI.x << ", " << boardROI.y << ", " << boardROI.width << ", " << boardROI.height << ")" << std::endl;

    auto result = ValidateChessboard(screenshot, boardROI, screenshot);
    if (result) {
        // Drawing the board on the overlay.
        const cv::Rect& fixedRect = *result;
        g_boardRect = { fixedRect.x, fixedRect.y, fixedRect.x + fixedRect.width, fixedRect.y + fixedRect.height };
        g_boardRectCV = fixedRect;

        // Automatically place sample points in the center of each cell
        int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
        int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

        g_userSamplePoints.clear(); // Clear any existing points

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                SAMPLE sample;
                sample.x = g_boardRect.left + (col * cellWidth) + (cellWidth / 2) - 3; // Center with 6x6 size
                sample.y = g_boardRect.top + (row * cellHeight) + (cellHeight / 2) - 3;
                sample.width = 6;
                sample.height = 6;
                g_userSamplePoints.push_back(sample);
            }
        }

        std::cout << "[INFO] Placed " << g_userSamplePoints.size() << " sample points in cell centers" << std::endl;

        // Update debug samples with current configuration
        UpdateDebugSamples();
    }
    else {
        std::cout << "[ERROR] Failed to detect chessboard pattern in the specified region" << std::endl;
        std::cout << "[ERROR] Please try clicking on different corners of the chessboard" << std::endl;
        return 0;
    }

    return 1;
}

// TODO: Document.
int DetectPieceColorCoding(cv::Mat screenshot, int cellWidth, int cellHeight) {
    // Sample each cell using the current slider values
    std::vector<double> sampleValues;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);

            // Use SampleCellCenter with current slider values
            double value = SampleCellCenter(screenshot, cellCenterX, cellCenterY);
            sampleValues.push_back(value);

            std::cout << "[DEBUG] Cell (" << row << ", " << col << ") brightness: " << value << std::endl;
        }
    }

    if (sampleValues.size() < 2) {
        std::cout << "[ERROR] Need at least 2 sample points for color analysis!" << std::endl;
        return 0;
    }

    // Find min and max values from samples
    double refBlack = *std::min_element(sampleValues.begin(), sampleValues.end());
    double refWhite = *std::max_element(sampleValues.begin(), sampleValues.end());

    std::cout << "[DEBUG] Black Brightness: " << refBlack << std::endl;
    std::cout << "[DEBUG] White Brightness: " << refWhite << std::endl;
    std::cout << "[INFO] Analysis started with " << sampleValues.size() << " sample points" << std::endl;

    return 1;
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    HWND hwndOverlay = (HWND)param;
    HWND hwndDesktop = GetDesktopWindow();
    
    // Wait for analysis to start
    while (!g_hasAnalysisStarted) {
        Sleep(100);
    }
    
    std::cout << "[INFO] ChessboardDetectionThread started analysis" << std::endl;
    
    // Board dimensions
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
    
    // Get reference values for piece color detection
    cv::Mat screenshot = HWND2MAT(hwndDesktop);
    DetectPieceColorCoding(screenshot, cellWidth, cellHeight);
    
    // Extract reference values from the analysis
    std::vector<double> sampleValues;
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
    
    std::cout << "[INFO] Reference values - Black: " << refBlack << ", White: " << refWhite << std::endl;
    
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
                bool isBlack = std::abs(val - refBlack) < tolerance;
                bool isWhite = std::abs(val - refWhite) < tolerance;

                std::cout << "[DEBUG] Cell (" << row << ", " << col << ") brightness: " << val 
                    << (isBlack ? " [Black]" : isWhite ? " [White]" : " [Empty]") << std::endl;

                // Count pieces for statistics
                if (isBlack) {
                    globalBlackCount++;
                } else if (isWhite) {
                    globalWhiteCount++;
                } else {
                    globalEmptyCount++;
                }

                // Build FEN notation
                if (!isBlack && !isWhite) {
                    ++emptyCount;
                }
                else {
                    if (emptyCount > 0) {
                        fen += std::to_string(emptyCount);
                        emptyCount = 0;
                    }
                    fen += (isBlack ? "b" : "w"); // Simple piece representation
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

// TODO: Document.
void UpdateDebugSamples() {
    // Clear existing debug samples
    g_debugSamples.clear();
    
    if (g_boardRect.right - g_boardRect.left <= 0 || g_boardRect.bottom - g_boardRect.top <= 0) {
        return; // Board not detected yet
    }
    
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
    
    // Generate debug samples for all cells using current configuration
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
            
            // Apply current slider values
            int patchSize = g_debugPatchSize;
            int offset = g_debugOffset;
            int half = patchSize / 2;
            
            SAMPLE sample;
            sample.x = cellCenterX - half;
            sample.y = cellCenterY - half + offset;
            sample.width = patchSize;
            sample.height = patchSize;
            
            g_debugSamples.push_back(sample);
        }
    }
}

