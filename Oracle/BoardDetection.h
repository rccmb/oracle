#pragma once

// Globals.h first: it includes windows.h, whose min/max macros OpenCV's headers
// then undefine. Including OpenCV first leaves those macros redefined and breaks
// every std::min / std::max in this translation unit.
#include "Globals.h"
#include "Structs.h"

#include <optional>

/**
 * @brief A chessboard located on screen, with everything calibration used to ask the user for.
 */
struct BoardCandidate {
    cv::Rect rect;          // Board bounds in capture-image coordinates.
    int cellSize = 0;       // Side of one square, in pixels.
    cv::Vec3b lightSquare;  // BGR of the light squares.
    cv::Vec3b darkSquare;   // BGR of the dark squares.
    double confidence = 0;  // Share of readable squares that matched the checker pattern.
    int uniformCells = 0;   // Squares that were empty enough to read a flat colour from.
};

/**
 * @brief Finds a chessboard anywhere in a desktop capture, without user input.
 *
 * A board is the only thing on a screen that is a grid of equally sized,
 * axis-aligned squares in two alternating colours, so the search looks for
 * exactly that: square contours, a cell size and grid phase shared by many of
 * them, and a 2-colour checker pattern to confirm the winner.
 *
 * Occlusion tolerant by construction. Pieces hide the middle of a square but
 * not its border, and the grid only needs a handful of agreeing squares to fix
 * its size and phase, so a board mid-game detects as readily as an empty one.
 *
 * @param bgr Full desktop capture.
 *
 * @return The best verified board, or nullopt when nothing convincing was found.
 */
std::optional<BoardCandidate> DetectChessboard(const cv::Mat& bgr);

/**
 * @brief Perceived brightness of a BGR colour, for comparing shades.
 *
 * Oracle stores a grayscale reference alongside every colour reference, and the
 * two have to be derived the same way for the comparisons to agree.
 *
 * @param bgr Colour to measure.
 *
 * @return Luminance in the range 0 to 255.
 */
int LuminanceOf(const cv::Vec3b& bgr);

/**
 * @brief When true, DetectChessboard reports each stage on stdout.
 *
 * Detection either works or silently does not, and which stage gave up is the
 * only thing worth knowing when it does not. Off by default.
 */
extern bool g_boardDetectionTrace;

/**
 * @brief Reads the two piece colours and the board orientation from a located board.
 *
 * Expects the starting position, which is also what reference piece generation
 * requires: it decides orientation from which back rank holds the darker pieces.
 *
 * @param bgr         Full desktop capture.
 * @param board       A board previously returned by DetectChessboard.
 * @param darkPiece   Receives the BGR colour of the dark pieces.
 * @param lightPiece  Receives the BGR colour of the light pieces.
 * @param orientation Receives 0 when white is at the bottom, 1 when it is at the top.
 *
 * @return true when both piece colours and the orientation could be read.
 */
bool EstimatePieceColors(const cv::Mat& bgr, const BoardCandidate& board,
                         cv::Vec3b& darkPiece, cv::Vec3b& lightPiece, int& orientation);

/**
 * @brief Sizes the sampling and crop regions from the detected cell size.
 *
 * These were six sliders in absolute pixels, which meant a value tuned on one
 * board was wrong on a board of any other size. They are proportions of a cell.
 *
 * The analysis tolerance follows from the board's own contrast for the same
 * reason: a fixed value is either too tight for a low-contrast theme or so loose
 * on a high-contrast one that empty squares read as pieces.
 *
 * @param board A board previously returned by DetectChessboard.
 */
void ApplyDerivedSampleGeometry(const BoardCandidate& board);
