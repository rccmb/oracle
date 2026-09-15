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

    int offsetsX[3] = { g_debugOffsetX, g_debugOffsetX2, g_debugOffsetX3 };
    int offsetsY[3] = { g_debugOffsetY, g_debugOffsetY2, g_debugOffsetY3 };

    // Generate debug samples for all cells using current configuration.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
            int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
            
            for (int s = 0; s < 3; ++s) {
                SAMPLE sample;
                sample.x = cellCenterX - (g_debugPatchSize / 2) + offsetsX[s];
                sample.y = cellCenterY - (g_debugPatchSize / 2) + offsetsY[s];
                sample.width = g_debugPatchSize;
                sample.height = g_debugPatchSize;
                g_debugSamples.push_back(sample);
            }
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

    // Capture the actual colors.
    if (!g_userScreenshotColor.empty()) {
        cv::Scalar tlColorMean = cv::mean(g_userScreenshotColor(tlRoi));
        cv::Scalar blColorMean = cv::mean(g_userScreenshotColor(blRoi));

        auto toVec3b = [](const cv::Scalar& s) {
            return cv::Vec3b(
                (uchar)std::clamp((int)std::round(s[0]), 0, 255),
                (uchar)std::clamp((int)std::round(s[1]), 0, 255),
                (uchar)std::clamp((int)std::round(s[2]), 0, 255)
            );
        };

        cv::Vec3b tlVec = toVec3b(tlColorMean);
        cv::Vec3b blVec = toVec3b(blColorMean);

        if (tlVal < blVal) {
            g_refBlackPieceColor = tlVec;
            g_refWhitePieceColor = blVec;
        } else {
            g_refBlackPieceColor = blVec;
            g_refWhitePieceColor = tlVec;
        }
    }

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

std::optional<cv::Rect> ValidateChessboard(const cv::Mat& gray, cv::Mat& debugImg) {
    uchar colorA = g_clicks.first.grayscaleValue;
    uchar colorB = g_clicks.second.grayscaleValue;
    const int colorThreshold = 35; // Increased threshold for robustness against textured boards 

    // The user clicked a8 and b8. The vertical boundary between them is halfway between their x-coordinates.
    int junction_x = (g_clicks.first.x + g_clicks.second.x) / 2;
    int start_y = std::min(g_clicks.first.y, g_clicks.second.y);
    
    // We sample colors to the left and right of the boundary. 
    // Since the distance between clicks is roughly cellW, offset = cellW / 4 is safely inside the squares.
    int offset = std::abs(g_clicks.second.x - g_clicks.first.x) / 4;
    if (offset < 5) offset = 5;

    std::vector<int> junction_ys;
    
    // The top row is A (left) and B (right). We traverse downwards looking for the first flip to B (left) and A (right).
    bool lookingForFlip = true;
    
    for (int y = start_y; y < gray.rows; y += 1) {
        int left_x = junction_x - offset;
        int right_x = junction_x + offset;
        
        if (left_x < 0 || right_x >= gray.cols) break;
        
        uchar left_val = gray.at<uchar>(y, left_x);
        uchar right_val = gray.at<uchar>(y, right_x);
        
        // Are we currently matching the inverted state (B left, A right)?
        bool match_flipped = 
            std::abs(left_val - colorB) < colorThreshold && 
            std::abs(right_val - colorA) < colorThreshold;
            
        // Are we matching the normal state (A left, B right)?
        bool match_normal = 
            std::abs(left_val - colorA) < colorThreshold && 
            std::abs(right_val - colorB) < colorThreshold;
            
        if (lookingForFlip && match_flipped) {
            junction_ys.push_back(y);
            lookingForFlip = false; // Now we look for a flip back to normal
            if (junction_ys.size() == 7) break; // Found all 7 vertical junctions
            y += offset * 2; // Skip ahead safely
        } else if (!lookingForFlip && match_normal) {
            junction_ys.push_back(y);
            lookingForFlip = true; // Now we look for a flip to flipped
            if (junction_ys.size() == 7) break;
            y += offset * 2; // Skip ahead safely
        }
    }

    if (junction_ys.size() < 4) {
        return std::nullopt; // Failed to find enough vertical junctions
    }

    std::vector<int> dy;
    for (size_t i = 1; i < junction_ys.size(); ++i) {
        dy.push_back(junction_ys[i] - junction_ys[i - 1]);
    }
    
    std::sort(dy.begin(), dy.end());
    int cellH = dy[dy.size() / 2];
    
    // Chessboard cells are square
    int cellW = cellH; 

    // junction_ys[0] is the top-most junction line (between rank 8 and 7).
    int first_junction_y = junction_ys[0];
    
    // We know junction_x is the line between A-file and B-file (k=1).
    int board_x = junction_x - cellW;
    
    // We know first_junction_y is the line between rank 8 and rank 7 (m=1).
    int board_y = first_junction_y - cellH;
    
    int board_w = cellW * 8;
    int board_h = cellH * 8;

    if (board_w > gray.cols || board_h > gray.rows) {
        return std::nullopt;
    }

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
        cv::Mat gray;
        cv::cvtColor(screenshot, gray, cv::COLOR_BGRA2GRAY);
        screenshot = gray.clone();
	}

    if (screenshot.empty()) return 0;

    auto result = ValidateChessboard(screenshot, screenshot);
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

void GenerateReferencePieceCrops(const cv::Mat& src, int cellWidth, int cellHeight) {
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
            ? SaveReferencePiece(src, rectFor(0, col), std::string("black_") + (char)pieceNamesBlack[col] + ".png")
            : SaveReferencePiece(src, rectFor(0, col), std::string("white_") + (char)pieceNamesWhite[col] + ".png");
    }
    g_orientation == 0
        ? SaveReferencePiece(src, rectFor(1, 0), "black_p.png")
        : SaveReferencePiece(src, rectFor(1, 0), "white_P.png");
    
    for (int col = 0; col < 5; ++col) {
        g_orientation == 0
            ? SaveReferencePiece(src, rectFor(7, col), std::string("white_") + (char)pieceNamesWhite[col] + ".png")
            : SaveReferencePiece(src, rectFor(7, col), std::string("black_") + (char)pieceNamesBlack[col] + ".png");
    }
    g_orientation == 0
        ? SaveReferencePiece(src, rectFor(6, 0), "white_P.png")
        : SaveReferencePiece(src, rectFor(6, 0), "black_p.png");

	// Give time for files to flush.
    Sleep(300);
}

