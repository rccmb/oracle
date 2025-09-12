#include "ChessboardDetection.h"

std::pair<CLICK, CLICK> CLICKS;

RECT g_boardRect = { 0, 0, 0, 0 };
std::vector<SAMPLE> g_debugSamples;

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

    cv::Rect g_boardRect(board_x, board_y, board_w, board_h);
    std::cout << "[INFO] Detected board at (" << g_boardRect.x << ", " << g_boardRect.y << ") size (" << g_boardRect.width << "x" << g_boardRect.height << ")\n";
    return g_boardRect;
}

// HELPERS.

// TODO: Document.
cv::Rect GetBoardROI(const cv::Mat& img, const CLICK& firstClick, const CLICK& secondClick) {
    int width = std::abs(secondClick.x - firstClick.x);
    int height = width;

    int roi_x = firstClick.x;
    int roi_y = firstClick.y;

    return cv::Rect(roi_x, roi_y, width, height);
}

// TODO: Document.
double SampleCellCenter(const cv::Mat& gray, int x, int y, int patch = 6, int offset = 0) {
    patch = 6;

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

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    HWND hwndOverlay = (HWND)param;
	HWND hwndDesktop = GetDesktopWindow();
    int drawn = 0;

    std::cout << "[INFO] Processing chessboard." << std::endl;

    // Screenshot timing.
    cv::Mat screenshot = HWND2MAT(hwndDesktop);

    if (screenshot.empty()) return 0;

	cv::Rect g_boardRectCV;

    // Validate the chessboard in the click-defined region.
    cv::Rect boardROI = GetBoardROI(screenshot, CLICKS.first, CLICKS.second);

    auto result = ValidateChessboard(screenshot, boardROI, screenshot);
    if (result) {
        // Drawing the board on the overlay.
        const cv::Rect& fixedRect = *result;
        g_boardRect = { fixedRect.x, fixedRect.y, fixedRect.x + fixedRect.width, fixedRect.y + fixedRect.height };
		g_boardRectCV = fixedRect;
    }
    else {
        return 0;
    }

    // Board dimensions.
	int boardWidth = g_boardRect.right - g_boardRect.left;
	int boardHeight = g_boardRect.bottom - g_boardRect.top;
    int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    // Determining piece colors by sampling rooks.
    double refTL = SampleCellCenter(screenshot, 
        g_boardRect.left + (cellWidth / 2), 
        g_boardRect.top + cellHeight / 2,
        2,
        10); // TL.
    double refBL = SampleCellCenter(screenshot, 
        g_boardRect.left + (cellWidth / 2), 
        g_boardRect.top + (7 * cellHeight) + (cellHeight / 2),
        2,
        10); // BL.

    double tempMin = std::min(refTL, refBL);
	double tempMax = std::max(refTL, refBL);
    double refBlack = tempMin;
    double refWhite = tempMax;

    std::cout << "[DEBUG] Black Brightness: " << refBlack << std::endl;
    std::cout << "[DEBUG] White Brightness: " << refWhite << std::endl;

    if (refTL > refBL) {
        std::cout << "[INFO] White pieces are at the top." << std::endl;
    }
    else {
        std::cout << "[INFO] Black pieces are at the top." << std::endl;
    }

    // Creating temporary directory for piece images.
    LPCWSTR tempDir = L"temp"; // Pointer.
    std::string tempDirStr = "temp"; // String.
    RemoveDirectory(tempDir);
    CreateDirectory(tempDir, NULL);

    for (int i = 0; i < 5; ++i) {
        int x = g_boardRect.left + i * cellWidth;
        int y = g_boardRect.top;

		std::cout << "[DEBUG] Capturing piece at (" << x << ", " << y << " with width " << cellWidth << " and height " << cellHeight << ")\n";
        cv::Mat piece = CropHWND2MAT(hwndDesktop, x, y, cellWidth, cellHeight);

        cv::Mat edges;
        cv::Canny(piece, edges, 50, 150);

        std::string filename = tempDirStr + "/piece_edges_row0_col" + std::to_string(i) + ".png";
        cv::imwrite(filename, edges);

        if (i == 4) {
            // First cell of second row.
            int x = g_boardRect.left;
            int y = g_boardRect.top + cellHeight;
            cv::Mat piece = CropHWND2MAT(hwndDesktop, x, y, cellWidth, cellHeight);

            cv::Mat edges;
            cv::Canny(piece, edges, 50, 150);

            std::string filename = tempDirStr + "/piece_edges_row1_col0.png";
            cv::imwrite(filename, edges);
        }
    }

	// Analysing the board continuously.
    int cellCount = 8 * 8;

    // While true:
    //  Take a print of the screen.
	//  For each cell, check if it is occupied like this: if the center is black or white, it is occupied.
	//  If occupied, save the COLOR of the piece and the position.
	//  For each occupied cell, crop the occupied cell and compare it to the pieces in the temp folder.
    //  According to the best match, assign a piece type, and draw text with the piece type according to FEN notation.
	//  While keeping track of the pieces, also turn the board into FEN notation so that we can later feed it into stockfish.

    auto refPieces = LoadReferencePieces(tempDir);

	int tolerance = 15;
    int globalEmptyCount = 0;
	int globalBlackCount = 0;
	int globalWhiteCount = 0;

    while (true) {
        cv::Mat frame = HWND2MAT(hwndDesktop);
        std::string fen;
        for (int row = 0; row < 8; ++row) {
            int emptyCount = 0;
            for (int col = 0; col < 8; ++col) {
                int cx = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
                int cy = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);

                double val = SampleCellCenter(frame, cx, cy, 2, 10);

                // Piece color detection.
                bool isBlack = std::abs(val - refBlack) < tolerance;
                bool isWhite = std::abs(val - refWhite) < tolerance;

                std::cout << "[DEBUG] Cell (" << row << ", " << col << ") brightness: " << val 
					<< (isBlack ? " [Black]" : isWhite ? " [White]" : " [Empty]") << std::endl;

                if (isBlack) globalBlackCount++;
                if (isWhite) globalWhiteCount++;

                if (!isBlack && !isWhite) {
                    ++emptyCount;
                    globalEmptyCount++;
                }
                else {
                    if (emptyCount > 0) {
                        fen += std::to_string(emptyCount);
                        emptyCount = 0;
                    }

                    // Crop, edge, and match.
                    cv::Mat crop = CropHWND2MAT(hwndDesktop, 
                        g_boardRect.left + col * cellWidth, 
                        g_boardRect.top + row * cellHeight, 
                        cellWidth, 
                        cellHeight);
                    cv::Mat edges;
                    cv::Canny(crop, edges, 50, 150);

                    // Find best match.
                    std::string bestName;
                    double bestScore = 1e9;
                    for (const auto& [name, ref] : refPieces) {
                        double score = CompareEdges(edges, ref);
                        if (score < bestScore) {
                            bestScore = score;
                            bestName = name;
                        }
                    }

                    // TODO, assign FEN name of the piece.
                }
            }

            if (emptyCount > 0) fen += std::to_string(emptyCount);
            if (row < 7) fen += '/';
        }

        std::cout << "[FEN] " << fen << std::endl;

        // TODO: Draw FEN or piece type on overlay if needed.

        break;
    }

    PostMessage(hwndOverlay, WM_CHESSBOARD_DETECTED, NULL, NULL);

	std::cout << "Pieces detected: Black = " << globalBlackCount << ", White = " << globalWhiteCount << ", Empty = " << globalEmptyCount << std::endl;

    Sleep(500000000);

    return 0;
}

void SetChessboardClicks(std::pair<CLICK, CLICK> clicks) {
	CLICKS = clicks;
}