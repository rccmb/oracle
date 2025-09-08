#include "Vision.h"

/**
 * @brief Validates if a given rectangle contains a chessboard pattern by checking intensity similarities at grid intersections.
 * 
 * @param gray Grayscale image of the screenshot.
 * @param rect Bounding rectangle of the candidate chessboard.
 * @param debugImg Output image for debug visualizations.
 * 
 * @return True if at least 50% of the checked intersections show valid matches, indicating a chessboard.
 */
bool validateChessboard(const cv::Mat& gray, const cv::Rect& rect, cv::Mat& debugImg) {
    int cellW = rect.width / 8;
    int cellH = rect.height / 8;

    int correctCount = 0;
    int totalChecks = 0;

    const int patch_size = 4;
    const int half = patch_size / 2;
    const int offset = 4; // Offset from junction. 2 PIXEL DIFFERENCE FROM THE BORDER.

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 7; col++) {
            // Cell intersection coordinates.
            int cornerX = rect.x + (col + 1) * cellW;
            int cornerY = rect.y + (row + 1) * cellH;

            cornerX = std::clamp(cornerX, offset + half, gray.cols - offset - half - 1);
            cornerY = std::clamp(cornerY, offset + half, gray.rows - offset - half - 1);

            cv::Rect tl_patch(cornerX - offset - half, cornerY - offset - half, patch_size, patch_size);
            cv::Rect tr_patch(cornerX + offset - half, cornerY - offset - half, patch_size, patch_size);
            cv::Rect bl_patch(cornerX - offset - half, cornerY + offset - half, patch_size, patch_size);
            cv::Rect br_patch(cornerX + offset - half, cornerY + offset - half, patch_size, patch_size);

            // Adjust patches to stay within bounds.
            tl_patch.width = std::min(tl_patch.width, gray.cols - tl_patch.x);
            tl_patch.height = std::min(tl_patch.height, gray.rows - tl_patch.y);
            tr_patch.width = std::min(tr_patch.width, gray.cols - tr_patch.x);
            tr_patch.height = std::min(tr_patch.height, gray.rows - tr_patch.y);
            bl_patch.width = std::min(bl_patch.width, gray.cols - bl_patch.x);
            bl_patch.height = std::min(bl_patch.height, gray.rows - bl_patch.y);
            br_patch.width = std::min(br_patch.width, gray.cols - br_patch.x);
            br_patch.height = std::min(br_patch.height, gray.rows - br_patch.y);

            // Skip small patches.
            if (tl_patch.area() < patch_size * patch_size / 2 ||
                tr_patch.area() < patch_size * patch_size / 2 ||
                bl_patch.area() < patch_size * patch_size / 2 ||
                br_patch.area() < patch_size * patch_size / 2) {
                continue;
            }

            // Compute mean intensity of each patch
            double avg_tl = cv::mean(gray(tl_patch))[0];
            double avg_tr = cv::mean(gray(tr_patch))[0];
            double avg_bl = cv::mean(gray(bl_patch))[0];
            double avg_br = cv::mean(gray(br_patch))[0];

            // Check if top-right matches bottom-left and top-left matches bottom-right.
            if (std::abs(avg_tr - avg_bl) <= 30 && std::abs(avg_tl - avg_br) <= 30) {
                correctCount++;
            }

            totalChecks++;

            // Debug: Draw intersection point and patches
            /*
            cv::circle(debugImg, cv::Point(cornerX, cornerY), 2, cv::Scalar(255, 0, 255), -1);
            cv::rectangle(debugImg, tl_patch, cv::Scalar(255, 0, 0), 1); 
            cv::rectangle(debugImg, tr_patch, cv::Scalar(0, 255, 0), 1);
            cv::rectangle(debugImg, bl_patch, cv::Scalar(0, 0, 255), 1);
            cv::rectangle(debugImg, br_patch, cv::Scalar(0, 255, 255), 1);
            */
        }
    }

    double ratio = totalChecks > 0 ? (double)correctCount / totalChecks : 0.0;

    // Debug: Save debug image if chessboard is detected
    /*
    if (ratio >= 0.5) {
        cv::imwrite("debug_chessboard_" + std::to_string(std::time(nullptr)) + ".png", debugImg);
    }
    */

	return (ratio >= 0.5); // Need at least 50% correct intersections.
}

DWORD WINAPI ChessboardDetectionThread(LPVOID param) {
    HWND hwndOverlay = (HWND)param;
    HWND hwndDesktop = GetDesktopWindow();
    int frameId = 0;

    while (true) {
        ClearCandidateRectangles();
		ClearBestRectangle();

        cv::Mat screenshot = hwnd2mat(hwndDesktop);
        if (screenshot.empty()) continue;

        cv::Mat gray, blur, edges;
        cv::cvtColor(screenshot, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, blur, cv::Size(5, 5), 1.0);

        double median = cv::mean(gray)[0]; 
        double lowThreshold = std::max(20.0, median * 0.5);
        double highThreshold = std::min(255.0, median * 1.5); 
        cv::Canny(blur, edges, lowThreshold, highThreshold);

        // Find the contours and order them by area. LARGEST TO SMALLEST.
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::sort(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return cv::contourArea(a) > cv::contourArea(b);
            });

        for (auto& contour : contours) {
            cv::Rect rect = cv::boundingRect(contour);
            double area = rect.area();
            double aspectRatio = (double)rect.width / rect.height;

            if (area < 8392) continue;
            double squareness = std::min(aspectRatio, 1.0 / aspectRatio);
            if (squareness < 0.9) continue;

            // Validate checkerboard.
            if (validateChessboard(gray, rect, screenshot)) { 
                RECT r = { rect.x, rect.y, rect.x + rect.width, rect.y + rect.height };
				SetBestRectangle(r);
                break;
            }

            RECT r = { rect.x, rect.y, rect.x + rect.width, rect.y + rect.height };
            AddCandidateRectangle(r);
        }

        PostMessage(hwndOverlay, WM_CHESSBOARD_CANDIDATES, NULL, NULL);

        std::cout << "Processed frame." << std::endl;

        Sleep(1);
    }

    return 0;
}