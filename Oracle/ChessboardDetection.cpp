#include "ChessboardDetection.h"

std::pair<CLICK, CLICK> CLICKS;

cv::Rect GetBoardROI(const cv::Mat& img, const CLICK& firstClick, const CLICK& secondClick) {
    int width = std::abs(secondClick.x - firstClick.x);
    int height = width;

    int roi_x = firstClick.x;
    int roi_y = firstClick.y;

    return cv::Rect(roi_x, roi_y, width, height);
}

/**
 * @brief Validates if a given rectangle contains a chessboard pattern by checking intensity similarities at grid intersections.
 * 
 * @param gray Grayscale image of the screenshot.
 * @param rect Bounding rectangle of the candidate chessboard.
 * @param debugImg Output image for debug visualizations.
 * 
 * @return A Rect containing the detected chessboard area if valid, or std::nullopt if not valid.
 */
std::optional<cv::Rect> ValidateChessboard(const cv::Mat& gray, const cv::Rect& roi, cv::Mat& debugImg) {
    const int patchSize = 1;
    const int stride = 2;

    uchar colorA = CLICKS.first.grayscaleValue;
    uchar colorB = CLICKS.second.grayscaleValue;
    const int colorThreshold = 5; // Acceptable difference for color match.

    std::vector<cv::Point> junctions;
    int totalChecks = 0;
    int junctionsFound = 0;

    for (int y = roi.y; y < roi.y + roi.height; y += stride) {
        if (junctionsFound == 7) break;

        for (int x = roi.x; x < roi.x + roi.width; x += stride) {
            if (junctionsFound == 7) break;

            int offset = 5;
            std::vector<cv::Point> cell_centers = {
                {x - offset, y - offset}, // TL.
                {x + offset, y - offset}, // TR.
                {x - offset, y + offset}, // BL.
                {x + offset, y + offset}  // BR.
            };

            bool valid = true;
            std::vector<double> avgs;
            for (auto& pt : cell_centers) {
                if (pt.x < 0 || pt.x >= gray.cols ||
                    pt.y < 0 || pt.y >= gray.rows) {
                    valid = false;
                    break;
                }
                cv::Rect patch(pt.x, pt.y, patchSize, patchSize);
                avgs.push_back(cv::mean(gray(patch))[0]);
            }
            if (!valid) continue;

            // Pattern 1: [A B; B A].
            bool pattern1 =
                std::abs(avgs[0] - colorA) < colorThreshold &&
                std::abs(avgs[1] - colorB) < colorThreshold &&
                std::abs(avgs[2] - colorB) < colorThreshold &&
                std::abs(avgs[3] - colorA) < colorThreshold;

            // Pattern 2: [B A; A B].
            bool pattern2 =
                std::abs(avgs[0] - colorB) < colorThreshold &&
                std::abs(avgs[1] - colorA) < colorThreshold &&
                std::abs(avgs[2] - colorA) < colorThreshold &&
                std::abs(avgs[3] - colorB) < colorThreshold;

            if (pattern1 || pattern2) {
                junctions.emplace_back(x + offset - patchSize, y + offset - patchSize);
				x += offset * 2; // Skip ahead to avoid overlapping checks.
                junctionsFound += 1;
            }
            totalChecks++;
        }
    }

    std::sort(junctions.begin(), junctions.end(), [](const cv::Point& a, const cv::Point& b) {
        return (a.y < b.y) || (a.y == b.y && a.x < b.x);
        });

    cv::Point topLeft = junctions.front();

    // Estimate cell size by averaging distances between adjacent junctions in x and y.
    std::vector<int> dx, dy;
    for (size_t i = 1; i < junctions.size(); ++i) {
        if (junctions[i].y == junctions[i - 1].y)
            dx.push_back(junctions[i].x - junctions[i - 1].x);
        if (junctions[i].x == junctions[i - 1].x)
            dy.push_back(junctions[i].y - junctions[i - 1].y);
    }

    // A chessboard is a square.
    int cellW = dx.empty() ? patchSize * 4 : std::accumulate(dx.begin(), dx.end(), 0) / (int)dx.size();

    int board_x = topLeft.x - cellW;
    int board_y = topLeft.y - cellW;
    int board_w = cellW * 8;
    int board_h = cellW * 8;

    board_x = std::clamp(board_x, 0, gray.cols - board_w);
    board_y = std::clamp(board_y, 0, gray.rows - board_h);

    cv::Rect boardRect(board_x, board_y, board_w, board_h);
    std::cout << "[INFO] Detected board at (" << boardRect.x << ", " << boardRect.y << ") size (" << boardRect.width << "x" << boardRect.height << ")\n";
    return boardRect;
}

double SampleCellBrightness(const cv::Mat& gray, const cv::Rect& board, int row, int col) {
    int cellW = board.width / 8;
    int cellH = board.height / 8;

    cv::Point center(
        board.x + col * cellW + cellW / 2,
        board.y + row * cellH + cellH / 2
    );

    int radius = std::min(cellW, cellH) / 4; 
    cv::Rect roi(center.x - radius, center.y - radius, radius * 2, radius * 2);
    roi &= cv::Rect(0, 0, gray.cols, gray.rows);

    return cv::mean(gray(roi))[0];
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    HWND hwndOverlay = (HWND)param;
	HWND hwndDesktop = GetDesktopWindow();
    int drawn = 0;

    std::cout << "[INFO] Processing chessboard." << std::endl;

    // Screenshot timing.
    start = std::chrono::high_resolution_clock::now();
    cv::Mat screenshot = HWND2MAT(hwndDesktop);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (screenshot.empty()) return 0;

    RECT bestRect;

    // Validate the chessboard in the click-defined region.
    cv::Rect boardROI = GetBoardROI(screenshot, CLICKS.first, CLICKS.second);

    auto result = ValidateChessboard(screenshot, boardROI, screenshot);
    if (result) {
        // Drawing the board on the overlay.
        const cv::Rect& fixedRect = *result;
        bestRect = { fixedRect.x, fixedRect.y, fixedRect.x + fixedRect.width, fixedRect.y + fixedRect.height };
        SetBestRectangle(bestRect);
        PostMessage(hwndOverlay, WM_CHESSBOARD_DETECTED, NULL, NULL);

        // Determining piece colors by sampling rooks.
        double topLeftRook = SampleCellBrightness(screenshot, fixedRect, 0, 0); // TL.
        double bottomLeftRook = SampleCellBrightness(screenshot, fixedRect, 7, 0); // BL.

        std::cout << "[DEBUG] Top-left rook brightness: " << topLeftRook << std::endl;
        std::cout << "[DEBUG] Bottom-left rook brightness: " << bottomLeftRook << std::endl;

        if (topLeftRook > bottomLeftRook) {
            std::cout << "[INFO] White pieces are at the top." << std::endl;
        }
        else {
            std::cout << "[INFO] Black pieces are at the top." << std::endl;
        }
    }

    std::cout << "[INFO] Processed chessboard." << std::endl;

    // Retrieving piece images.
    int cellWidth = (bestRect.right - bestRect.left) / 8;
    int cellHeight = (bestRect.bottom - bestRect.top) / 8;

    // Creating temporary directory for piece images.
    LPCWSTR tempDir = L"temp"; // Pointer.
    std::string tempDirStr = "temp"; // String.
    RemoveDirectory(tempDir);
    CreateDirectory(tempDir, NULL);

    for (int i = 0; i < 5; ++i) {
        int x = bestRect.left + i * cellWidth;
        int y = bestRect.top;

		std::cout << "[DEBUG] Capturing piece at (" << x << ", " << y << " with width " << cellWidth << " and height " << cellHeight << ")\n";
        cv::Mat piece = CropHWND2MAT(hwndDesktop, x, y, cellWidth, cellHeight);

        cv::Mat edges;
        cv::Canny(piece, edges, 50, 150);

        std::string filename = tempDirStr + "/piece_edges_row0_col" + std::to_string(i) + ".png";
        cv::imwrite(filename, edges);

        if (i == 4) {
            // First cell of second row.
            int x = bestRect.left;
            int y = bestRect.top + cellHeight;
            cv::Mat piece = CropHWND2MAT(hwndDesktop, x, y, cellWidth, cellHeight);

            cv::Mat edges;
            cv::Canny(piece, edges, 50, 150);

            std::string filename = tempDirStr + "/piece_edges_row1_col0.png";
            cv::imwrite(filename, edges);
        }
    }

    Sleep(500000000);

    return 0;
}

void SetChessboardClicks(std::pair<CLICK, CLICK> clicks) {
	CLICKS = clicks;
}