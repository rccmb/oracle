// Runs the board detector over a screenshot and reports what it found.
//
// Detection is the one part of Oracle that can be checked without a live board
// on screen: give it a PNG of a desktop and it either finds the board or does
// not. Point it at screenshots from whichever sites and themes matter, pass the
// expected origin and cell size, and a change to the detector that breaks one of
// them says so instead of silently degrading in real games.
//
// Build with build.bat. See README.md.
#include "vision/BoardDetection.h"

#include <cstdio>
#include <cstdlib>
#include <opencv2/imgcodecs.hpp>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: BoardDetectionCheck <image> [expected_x expected_y expected_cell]\n");
        std::printf("       with no expected values it reports what it found and exits 0.\n");
        return 2;
    }

    cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (image.empty()) {
        std::printf("FAIL: could not read %s\n", argv[1]);
        return 2;
    }
    std::printf("image        : %s, %dx%d\n", argv[1], image.cols, image.rows);

    g_boardDetectionTrace = true;

    const int64 start = cv::getTickCount();
    std::optional<BoardCandidate> board = DetectChessboard(image);
    const double elapsedMs = (cv::getTickCount() - start) / cv::getTickFrequency() * 1000.0;
    std::printf("detect took  : %.1f ms\n", elapsedMs);

    if (!board) {
        // Correct for an image with no board on it, so the caller decides.
        std::printf("RESULT: no board detected\n");
        return 1;
    }

    std::printf("board rect   : x=%d y=%d w=%d h=%d\n",
        board->rect.x, board->rect.y, board->rect.width, board->rect.height);
    std::printf("cell size    : %d\n", board->cellSize);
    std::printf("confidence   : %.3f over %d readable squares\n",
        board->confidence, board->uniformCells);
    std::printf("light square : B=%d G=%d R=%d\n",
        board->lightSquare[0], board->lightSquare[1], board->lightSquare[2]);
    std::printf("dark square  : B=%d G=%d R=%d\n",
        board->darkSquare[0], board->darkSquare[1], board->darkSquare[2]);

    cv::Vec3b darkPiece, lightPiece;
    int orientation = -1;
    if (EstimatePieceColors(image, *board, darkPiece, lightPiece, orientation)) {
        std::printf("dark piece   : B=%d G=%d R=%d\n", darkPiece[0], darkPiece[1], darkPiece[2]);
        std::printf("light piece  : B=%d G=%d R=%d\n", lightPiece[0], lightPiece[1], lightPiece[2]);
        std::printf("orientation  : %d (%s at the bottom)\n", orientation,
            orientation == 0 ? "white" : "black");
    }
    else {
        std::printf("WARN: piece colours could not be read (not a starting position?)\n");
    }

    ApplyDerivedSampleGeometry(*board);
    std::printf("derived      : samplePatch=%d cropPatch=%d probeOffset=%d tolerance=%d\n",
        g_debugPatchSize, g_cropPatchSize, g_debugOffsetY3, g_analysisTolerance);

    if (argc >= 5) {
        const int expectedX = std::atoi(argv[2]);
        const int expectedY = std::atoi(argv[3]);
        const int expectedCell = std::atoi(argv[4]);
        const int tolerance = std::max(3, expectedCell / 10);

        const bool ok =
            std::abs(board->rect.x - expectedX) <= tolerance &&
            std::abs(board->rect.y - expectedY) <= tolerance &&
            std::abs(board->cellSize - expectedCell) <= tolerance;

        std::printf("%s: expected x=%d y=%d cell=%d within %d px\n",
            ok ? "PASS" : "FAIL", expectedX, expectedY, expectedCell, tolerance);
        return ok ? 0 : 1;
    }

    return 0;
}
