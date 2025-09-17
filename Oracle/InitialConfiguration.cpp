#include "InitialConfiguration.h"

void SetBoardClicks(HWND hwndDesktop) {
    const cv::Mat& screenshot = g_userScreenshotGray;

    POINT p;
    GetCursorPos(&p);

    if (g_clickStage == 0) {
        g_viewFirstClick.x = p.x;
        g_viewFirstClick.y = p.y;
        if (g_viewFirstClick.y >= 0 && g_viewFirstClick.y < screenshot.rows && g_viewFirstClick.x >= 0 && g_viewFirstClick.x < screenshot.cols)
            g_viewFirstClick.grayscaleValue = screenshot.at<uchar>(g_viewFirstClick.y, g_viewFirstClick.x);
        g_clickStage = 1;
    }
    else if (g_clickStage == 1) {
        g_viewSecondClick.x = p.x;
        g_viewSecondClick.y = p.y;
        if (g_viewSecondClick.y >= 0 && g_viewSecondClick.y < screenshot.rows && g_viewSecondClick.x >= 0 && g_viewSecondClick.x < screenshot.cols)
            g_viewSecondClick.grayscaleValue = screenshot.at<uchar>(g_viewSecondClick.y, g_viewSecondClick.x);
        g_clickStage = 2;
    }

    Sleep(200);
}

void UpdateDebugSamples() {
    g_debugSamples.clear();

    if (g_boardRect.right - g_boardRect.left <= 0 || g_boardRect.bottom - g_boardRect.top <= 0) {
        return;
    }

    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    // Generate debug samples for all cells using current configuration.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            SAMPLE sample;
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
            sample.x = cellCenterX - (g_debugPatchSize / 2) + g_debugOffsetX;
            sample.y = cellCenterY - (g_debugPatchSize / 2) + g_debugOffsetY;
            sample.width = g_debugPatchSize;
            sample.height = g_debugPatchSize;
            g_debugSamples.push_back(sample);
        }
    }
}

int DetectPieceColorCoding(int cellWidth, int cellHeight) {
	// Hide debug samples during analysis so they don't interfere with detection visuals.
	g_isConfiguringSamplePoints = false;

	// Coordinates for the centers of top-left and bottom-left cells.
	int topLeftCenterX = g_boardRect.left + (cellWidth / 2);
	int topLeftCenterY = g_boardRect.top + (cellHeight / 2);
	int bottomLeftCenterX = g_boardRect.left + (cellWidth / 2);
	int bottomLeftCenterY = g_boardRect.top + (7 * cellHeight) + (cellHeight / 2);

	int patch = g_debugPatchSize;
	int offsetX = g_debugOffsetX;
	int offsetY = g_debugOffsetY;

	int tlX = topLeftCenterX - (patch / 2) + offsetX;
	int tlY = topLeftCenterY - (patch / 2) + offsetY;
	int blX = bottomLeftCenterX - (patch / 2) + offsetX;
	int blY = bottomLeftCenterY - (patch / 2) + offsetY;

	cv::Rect tlRoi(tlX, tlY, patch, patch);
	cv::Rect blRoi(blX, blY, patch, patch);
	tlRoi &= cv::Rect(0, 0, g_userScreenshotGray.cols, g_userScreenshotGray.rows);
	blRoi &= cv::Rect(0, 0, g_userScreenshotGray.cols, g_userScreenshotGray.rows);

	double tlVal = cv::mean(g_userScreenshotGray(tlRoi))[0];
	double blVal = cv::mean(g_userScreenshotGray(blRoi))[0];

	// Darker is the black pieces.
	if (tlVal < blVal) {
		g_refBlackPiece = (int)std::round(tlVal);
		g_refWhitePiece = (int)std::round(blVal);
		g_orientation = 0; // Standard orientation. White at bottom.
	} else {
		g_refBlackPiece = (int)std::round(blVal);
		g_refWhitePiece = (int)std::round(tlVal);
		g_orientation = 1; // Flipped orientation. White at top.
	}

	return 1;
}

std::optional<cv::Rect> ValidateChessboard(const cv::Mat& gray, const cv::Rect& roi, cv::Mat& debugImg) {
    const int patchSize = 1;
    const int stride = 2;

    uchar colorA = g_clicks.first.grayscaleValue;
    uchar colorB = g_clicks.second.grayscaleValue;
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

    // Check if we found enough junctions to form a chessboard.
    if (junctions.size() < 4) {
        return std::nullopt;
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

    cv::Rect g_boardRect(board_x, board_y, board_w, board_h);
    return g_boardRect;
}

int DetectBoardDimensions() {
	cv::Mat screenshot;
	if (g_userScreenshotReady && !g_userScreenshotGray.empty()) {
		screenshot = g_userScreenshotGray;
	} else {
		screenshot = HWND2MAT(GetDesktopWindow());
	}

    if (screenshot.empty()) return 0;

    // Calculate the board ROI.
    int width = std::abs(g_clicks.second.x - g_clicks.first.x);
    int height = width;

    int roi_x = g_clicks.first.x;
    int roi_y = g_clicks.first.y;

    cv::Rect boardROI = cv::Rect(roi_x, roi_y, width, height);

    auto result = ValidateChessboard(screenshot, boardROI, screenshot);
    if (result) {
        // Drawing the board on the overlay.
        const cv::Rect& fixedRect = *result;
        g_boardRect = { fixedRect.x, fixedRect.y, fixedRect.x + fixedRect.width, fixedRect.y + fixedRect.height };

        // Generate debug samples now that board is known.
        UpdateDebugSamples();
    }

    return 1;
}

void UpdateCropRects() {
    g_cropRects.clear();

    if (g_boardRect.right - g_boardRect.left <= 0 || g_boardRect.bottom - g_boardRect.top <= 0) {
        return;
    }

    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            SAMPLE rect;
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
            rect.x = cellCenterX - (g_cropPatchSize / 2) + g_cropOffsetX;
            rect.y = cellCenterY - (g_cropPatchSize / 2) + g_cropOffsetY;
            rect.width = g_cropPatchSize;
            rect.height = g_cropPatchSize;
            g_cropRects.push_back(rect);
        }
    }
}

void GenerateReferencePieceCrops(const cv::Mat& gray, int cellWidth, int cellHeight) {
    if (std::filesystem::exists(g_tempDir)) {
        std::filesystem::remove_all(g_tempDir);
    }
    std::filesystem::create_directories(g_tempDir);

    auto rectFor = [&](int row, int col) -> cv::Rect {
        int centerX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
        int centerY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
        int x = centerX - (g_cropPatchSize / 2) + g_cropOffsetX;
        int y = centerY - (g_cropPatchSize / 2) + g_cropOffsetY;
        return cv::Rect(x, y, g_cropPatchSize, g_cropPatchSize);
        };

	// Depends on orientation.
    std::vector<uchar> pieceNamesBlack;
    std::vector<uchar> pieceNamesWhite;
    if (g_orientation == 0) { // Black up.
        pieceNamesBlack = { 'r', 'n', 'b', 'q', 'k', 'p' }; 
        pieceNamesWhite = { 'R', 'N', 'B', 'Q', 'K', 'P' }; 
    }
	else { // White up.
        pieceNamesBlack = { 'r', 'n', 'b', 'k', 'q', 'p' };
        pieceNamesWhite = { 'R', 'N', 'B', 'K', 'Q', 'P' };
    }
    

    for (int col = 0; col < 5; ++col) {
        g_orientation == 0 
            ? SaveReferencePiece(gray, rectFor(0, col), std::string("black_") + (char)pieceNamesBlack[col] + ".png")
			: SaveReferencePiece(gray, rectFor(0, col), std::string("white_") + (char)pieceNamesWhite[col] + ".png");
    }
    g_orientation == 0
        ? SaveReferencePiece(gray, rectFor(1, 0), "black_p.png")
        : SaveReferencePiece(gray, rectFor(1, 0), "white_P.png");
    
    for (int col = 0; col < 5; ++col) {
        g_orientation == 0
            ? SaveReferencePiece(gray, rectFor(7, col), std::string("white_") + (char)pieceNamesWhite[col] + ".png")
            : SaveReferencePiece(gray, rectFor(7, col), std::string("black_") + (char)pieceNamesBlack[col] + ".png");
    }
    g_orientation == 0
        ? SaveReferencePiece(gray, rectFor(6, 0), "white_P.png")
        : SaveReferencePiece(gray, rectFor(6, 0), "black_p.png");

	// Give time for files to flush.
    Sleep(300);
}

