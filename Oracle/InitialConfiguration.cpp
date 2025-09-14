#include "InitialConfiguration.h"

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

    // Check if we found enough junctions to form a chessboard
    if (junctions.size() < 4) {
        std::cout << "[ERROR] Not enough chessboard junctions found (" << junctions.size() << " found, need at least 4)" << std::endl;
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
    std::cout << "[INFO] Detected board at (" << g_boardRect.x << ", " << g_boardRect.y << ") size (" << g_boardRect.width << "x" << g_boardRect.height << ")\n";
    return g_boardRect;
}