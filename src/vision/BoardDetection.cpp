#include "vision/BoardDetection.h"

#include <array>
#include <cstdio>
#include <map>
#include <set>

bool g_boardDetectionTrace = false;

int LuminanceOf(const cv::Vec3b& bgr) {
    return (int)std::lround(0.114 * bgr[0] + 0.587 * bgr[1] + 0.299 * bgr[2]);
}

namespace {

template <typename... Args>
void Trace(const char* format, Args... args) {
    if (g_boardDetectionTrace) std::printf(format, args...);
}

constexpr int kMinCellSize = 12;
constexpr int kMaxCellSize = 400;
constexpr int kBoardCells = 8;

// A square region found on screen. Most are board squares; the rest are icons,
// buttons and avatars, which the grid fit discards.
struct SquareCandidate {
    int x = 0;
    int y = 0;
    int size = 0;
};

struct GridFit {
    int cellSize = 0;
    int originX = 0;
    int originY = 0;
    int support = 0; // Squares that landed on this lattice inside the 8x8 window.
};

struct CellReading {
    bool uniform = false;         // Flat enough to be an empty square.
    cv::Vec3b color;              // Mean colour of the middle of the cell.
    bool hasBorderColor = false;  // Whether the corners could be sampled.
    cv::Vec3b borderColor;        // Colour of the corners, which a piece leaves showing.
};

int ColorDistance(const cv::Vec3b& a, const cv::Vec3b& b) {
    return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
}

cv::Vec3b MedianColor(std::vector<cv::Vec3b>& colors) {
    cv::Vec3b out(0, 0, 0);
    if (colors.empty()) return out;

    for (int channel = 0; channel < 3; ++channel) {
        std::vector<uchar> values;
        values.reserve(colors.size());
        for (const auto& color : colors) values.push_back(color[channel]);
        std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
        out[channel] = values[values.size() / 2];
    }

    return out;
}

// Square, axis-aligned, filled contours. The grid lines of a chessboard enclose
// its squares, so every square a piece does not overrun shows up here.
std::vector<SquareCandidate> FindSquareCandidates(const cv::Mat& gray) {
    cv::Mat edges;
    cv::Canny(gray, edges, 40, 120);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    std::vector<SquareCandidate> candidates;
    candidates.reserve(contours.size() / 4 + 8);

    for (const auto& contour : contours) {
        if (contour.size() < 4) continue;

        cv::Rect box = cv::boundingRect(contour);
        if (box.width < kMinCellSize || box.height < kMinCellSize) continue;
        if (box.width > kMaxCellSize || box.height > kMaxCellSize) continue;

        // Square, with room for antialiasing and boards whose size does not
        // divide evenly by eight.
        const int longest = std::max(box.width, box.height);
        if (std::abs(box.width - box.height) > std::max(2, longest / 8)) continue;

        // Rectangularity is tested by where the outline runs, not by area. A
        // board square appears in an edge map as a one pixel ring, and OpenCV
        // traces such a ring by running around it and back again: the shoelace
        // sum cancels, so contourArea reports zero for the very squares being
        // looked for, and the doubled-back path can repeat or skip corners.
        //
        // What holds regardless of traversal is that every point of an
        // axis-aligned rectangle lies on its own bounding box border, and that
        // all four borders are touched. A blob, an L or a rounded icon fails one
        // or the other.
        const int edgeTolerance = std::max(2, longest / 10);
        bool edgeSeen[4] = { false, false, false, false };
        bool allOnBorder = true;

        for (const cv::Point& point : contour) {
            const int toLeft = std::abs(point.x - box.x);
            const int toRight = std::abs(point.x - (box.x + box.width - 1));
            const int toTop = std::abs(point.y - box.y);
            const int toBottom = std::abs(point.y - (box.y + box.height - 1));

            const int nearest = std::min({ toLeft, toRight, toTop, toBottom });
            if (nearest > edgeTolerance) { allOnBorder = false; break; }

            if (toLeft == nearest) edgeSeen[0] = true;
            else if (toRight == nearest) edgeSeen[1] = true;
            else if (toTop == nearest) edgeSeen[2] = true;
            else edgeSeen[3] = true;
        }

        if (!allOnBorder) continue;
        if (!(edgeSeen[0] && edgeSeen[1] && edgeSeen[2] && edgeSeen[3])) continue;

        candidates.push_back({ box.x, box.y, (box.width + box.height) / 2 });
    }

    return candidates;
}

// Offset, modulo period, shared by the most values. Votes spread over a small
// window because a border antialiased across two pixels would otherwise split
// one grid into two competing peaks.
int ModuloPhase(const std::vector<int>& values, int period) {
    if (period <= 0) return 0;

    std::vector<int> votes(period, 0);
    for (int value : values) {
        const int base = ((value % period) + period) % period;
        for (int delta = -2; delta <= 2; ++delta) {
            const int slot = (((base + delta) % period) + period) % period;
            votes[slot] += 3 - std::abs(delta);
        }
    }

    return (int)std::distance(votes.begin(), std::max_element(votes.begin(), votes.end()));
}

// Places candidates of roughly one size onto a lattice, then returns the 8x8
// windows of that lattice holding the most of them.
//
// Several placements routinely tie. A board whose readable squares happen to
// form a block narrower than eight ranks can be covered by more than one window,
// and nothing at this stage can tell those apart, so the caller verifies each
// against the checker pattern instead of a winner being guessed here.
std::vector<GridFit> FitGrid(const std::vector<SquareCandidate>& candidates, int cellSize) {
    std::vector<GridFit> fits;
    if (cellSize < kMinCellSize) return fits;

    const int sizeTolerance = std::max(2, cellSize / 8);
    std::vector<SquareCandidate> sameSize;
    for (const auto& candidate : candidates) {
        if (std::abs(candidate.size - cellSize) <= sizeTolerance) sameSize.push_back(candidate);
    }
    if (sameSize.size() < 6) return fits;

    std::vector<int> xs, ys;
    xs.reserve(sameSize.size());
    ys.reserve(sameSize.size());
    for (const auto& candidate : sameSize) {
        xs.push_back(candidate.x);
        ys.push_back(candidate.y);
    }

    const int phaseX = ModuloPhase(xs, cellSize);
    const int phaseY = ModuloPhase(ys, cellSize);
    const int snapTolerance = std::max(2, cellSize / 10);

    std::set<std::pair<int, int>> onLattice;
    for (const auto& candidate : sameSize) {
        const int i = (int)std::lround((double)(candidate.x - phaseX) / cellSize);
        const int j = (int)std::lround((double)(candidate.y - phaseY) / cellSize);
        if (std::abs(candidate.x - (phaseX + i * cellSize)) > snapTolerance) continue;
        if (std::abs(candidate.y - (phaseY + j * cellSize)) > snapTolerance) continue;
        onLattice.insert({ i, j });
    }
    if (onLattice.size() < 6) return fits;

    int minI = INT_MAX, maxI = INT_MIN, minJ = INT_MAX, maxJ = INT_MIN;
    for (const auto& cell : onLattice) {
        minI = std::min(minI, cell.first);
        maxI = std::max(maxI, cell.first);
        minJ = std::min(minJ, cell.second);
        maxJ = std::max(maxJ, cell.second);
    }

    // Summed-area table over lattice occupancy, so scoring each 8x8 placement
    // costs four lookups rather than a scan.
    const int width = maxI - minI + 1;
    const int height = maxJ - minJ + 1;
    std::vector<int> sum((size_t)(width + 1) * (size_t)(height + 1), 0);
    auto at = [&](int i, int j) -> int& { return sum[(size_t)j * (size_t)(width + 1) + (size_t)i]; };

    for (const auto& cell : onLattice) at(cell.first - minI + 1, cell.second - minJ + 1) = 1;
    for (int j = 1; j <= height; ++j) {
        for (int i = 1; i <= width; ++i) {
            at(i, j) += at(i - 1, j) + at(i, j - 1) - at(i - 1, j - 1);
        }
    }

    // A board can sit anywhere on the lattice, including partly outside the span
    // of the squares actually found, so the window may start before them.
    int bestSupport = 0;
    for (int j0 = minJ - kBoardCells + 1; j0 <= maxJ; ++j0) {
        for (int i0 = minI - kBoardCells + 1; i0 <= maxI; ++i0) {
            const int left = std::max(0, i0 - minI);
            const int top = std::max(0, j0 - minJ);
            const int right = std::min(width, i0 - minI + kBoardCells);
            const int bottom = std::min(height, j0 - minJ + kBoardCells);
            if (right <= left || bottom <= top) continue;

            const int support = at(right, bottom) - at(left, bottom) - at(right, top) + at(left, top);
            if (support < 6) continue;

            GridFit fit;
            fit.support = support;
            fit.cellSize = cellSize;
            fit.originX = phaseX + i0 * cellSize;
            fit.originY = phaseY + j0 * cellSize;
            fits.push_back(fit);

            bestSupport = std::max(bestSupport, support);
        }
    }

    // Keep only the placements that are competitive, strongest first, so the
    // caller verifies a handful rather than every window on the lattice.
    const int cutoff = std::max(6, bestSupport - 2);
    fits.erase(std::remove_if(fits.begin(), fits.end(),
        [cutoff](const GridFit& fit) { return fit.support < cutoff; }), fits.end());
    std::sort(fits.begin(), fits.end(),
        [](const GridFit& a, const GridFit& b) { return a.support > b.support; });
    if (fits.size() > 12) fits.resize(12);

    return fits;
}

bool PatchStatistics(const cv::Mat& bgr, cv::Rect patch, cv::Vec3b& color, double& spread) {
    patch &= cv::Rect(0, 0, bgr.cols, bgr.rows);
    if (patch.width <= 0 || patch.height <= 0) return false;

    cv::Scalar mean, stddev;
    cv::meanStdDev(bgr(patch), mean, stddev);

    color = cv::Vec3b(
        (uchar)std::clamp((int)std::lround(mean[0]), 0, 255),
        (uchar)std::clamp((int)std::lround(mean[1]), 0, 255),
        (uchar)std::clamp((int)std::lround(mean[2]), 0, 255));
    spread = std::max({ stddev[0], stddev[1], stddev[2] });

    return true;
}

CellReading ReadCell(const cv::Mat& bgr, const cv::Rect& cell) {
    CellReading reading;

    // The middle half of the square: clear of the borders, and fully covered by
    // a piece when one is present, so a flat reading here means an empty square.
    const int inset = std::max(1, cell.width / 4);
    cv::Vec3b centerColor;
    double spread = 0;
    if (PatchStatistics(bgr,
            cv::Rect(cell.x + inset, cell.y + inset,
                     cell.width - 2 * inset, cell.height - 2 * inset),
            centerColor, spread)) {
        reading.color = centerColor;
        reading.uniform = spread < 6.0;
    }

    // Every square also gets a reading from its four corners. Piece glyphs are
    // drawn centred and leave the corners showing, so this yields the square's
    // own colour whether or not a piece stands on it. The median of the four
    // survives one corner being covered by an oversized set or a highlight dot.
    const int patchSize = std::max(2, cell.width / 6);
    const int corner = std::max(2, cell.width / 12);
    const int opposite = cell.width - corner - patchSize;

    std::vector<cv::Vec3b> cornerColors;
    const int offsets[4][2] = { { corner, corner }, { opposite, corner }, { corner, opposite }, { opposite, opposite } };
    for (const auto& offset : offsets) {
        cv::Vec3b cornerColor;
        double cornerSpread = 0;
        if (PatchStatistics(bgr,
                cv::Rect(cell.x + offset[0], cell.y + offset[1], patchSize, patchSize),
                cornerColor, cornerSpread)) {
            cornerColors.push_back(cornerColor);
        }
    }

    if (!cornerColors.empty()) {
        reading.borderColor = MedianColor(cornerColors);
        reading.hasBorderColor = true;
    }

    return reading;
}

// Confirms a located grid really is a chessboard, and reads its square colours.
bool VerifyBoard(const cv::Mat& bgr, BoardCandidate& board) {
    if (board.rect.x < 0 || board.rect.y < 0) return false;
    if (board.rect.br().x > bgr.cols || board.rect.br().y > bgr.rows) return false;

    std::array<CellReading, 64> readings;
    std::vector<cv::Vec3b> evenColors, oddColors;

    for (int row = 0; row < kBoardCells; ++row) {
        for (int col = 0; col < kBoardCells; ++col) {
            const cv::Rect cell(
                board.rect.x + col * board.cellSize,
                board.rect.y + row * board.cellSize,
                board.cellSize, board.cellSize);

            const CellReading reading = ReadCell(bgr, cell);
            readings[(size_t)row * kBoardCells + col] = reading;
            if (!reading.uniform) continue;

            ((row + col) % 2 == 0 ? evenColors : oddColors).push_back(reading.color);
        }
    }

    // Both shades have to be visible somewhere. Boards are rarely so crowded
    // that fewer than three squares of a shade can be read.
    if (evenColors.size() < 3 || oddColors.size() < 3) {
        Trace("    verify %d,%d cell=%d: only %zu/%zu readable squares per shade\n",
            board.rect.x, board.rect.y, board.cellSize, evenColors.size(), oddColors.size());
        return false;
    }

    const cv::Vec3b evenMedian = MedianColor(evenColors);
    const cv::Vec3b oddMedian = MedianColor(oddColors);

    // Two shades of one board, not a flat rectangle split down the middle.
    if (ColorDistance(evenMedian, oddMedian) < 20) {
        Trace("    verify %d,%d cell=%d: shades too close (%d)\n",
            board.rect.x, board.rect.y, board.cellSize, ColorDistance(evenMedian, oddMedian));
        return false;
    }

    const int uniformCount = (int)(evenColors.size() + oddColors.size());

    // Scoring runs over all sixty-four squares, using the corner reading so that
    // squares under a piece count too. Scoring only the empty ones would let a
    // window slid a whole rank off the board score just as well: a checkerboard
    // still alternates when shifted, so the squares that remain in view agree
    // perfectly among themselves. What gives the true placement away is that
    // every one of its sixty-four squares is board, and a shifted one's are not.
    int agreeing = 0;
    int scored = 0;
    for (int index = 0; index < 64; ++index) {
        const CellReading& reading = readings[index];
        if (!reading.hasBorderColor) continue;
        ++scored;

        const int row = index / kBoardCells;
        const int col = index % kBoardCells;
        const bool even = ((row + col) % 2 == 0);
        const cv::Vec3b& own = even ? evenMedian : oddMedian;
        const cv::Vec3b& other = even ? oddMedian : evenMedian;

        const int toOwn = ColorDistance(reading.borderColor, own);
        const int toOther = ColorDistance(reading.borderColor, other);

        // Nearer its own shade, and actually near it. Without the absolute bound
        // an unrelated colour off the edge of the board would still be counted
        // for whichever shade it happened to sit closer to.
        if (toOwn < toOther && toOwn <= 90) ++agreeing;
    }

    board.uniformCells = uniformCount;
    board.confidence = (scored > 0) ? (double)agreeing / (double)scored : 0.0;

    // Highlighted last-move squares and check markers legitimately break the
    // pattern, so a minority of disagreeing squares is tolerated. The bar sits
    // high enough that a placement including any rank of non-board still fails.
    if (uniformCount < 6 || scored < 64 || board.confidence < 0.90) {
        Trace("    verify %d,%d cell=%d: %d readable, %d scored, confidence %.2f\n",
            board.rect.x, board.rect.y, board.cellSize, uniformCount, scored, board.confidence);
        return false;
    }

    Trace("    verify %d,%d cell=%d: ACCEPTED %d readable, confidence %.2f\n",
        board.rect.x, board.rect.y, board.cellSize, uniformCount, board.confidence);

    if (LuminanceOf(evenMedian) >= LuminanceOf(oddMedian)) {
        board.lightSquare = evenMedian;
        board.darkSquare = oddMedian;
    }
    else {
        board.lightSquare = oddMedian;
        board.darkSquare = evenMedian;
    }

    return true;
}

} // namespace

std::optional<BoardCandidate> DetectChessboard(const cv::Mat& bgr) {
    if (bgr.empty()) return std::nullopt;

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    const std::vector<SquareCandidate> candidates = FindSquareCandidates(gray);
    Trace("square candidates: %zu\n", candidates.size());
    if (candidates.size() < 6) return std::nullopt;

    // Rank the sizes actually observed by how many squares share them, then try
    // the strongest few. Chasing every distinct size would be wasted work.
    std::map<int, int> sizeCounts;
    for (const auto& candidate : candidates) ++sizeCounts[candidate.size];

    std::vector<std::pair<int, int>> sizesByCount(sizeCounts.begin(), sizeCounts.end());
    std::sort(sizesByCount.begin(), sizesByCount.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second > b.second; });

    std::vector<int> sizesToTry;
    for (const auto& entry : sizesByCount) {
        if (sizesToTry.size() >= 15) break;

        bool tooClose = false;
        for (int chosen : sizesToTry) {
            if (std::abs(chosen - entry.first) <= 2) { tooClose = true; break; }
        }
        if (!tooClose) sizesToTry.push_back(entry.first);
    }

    std::optional<BoardCandidate> best;
    for (int cellSize : sizesToTry) {
        const std::vector<GridFit> fits = FitGrid(candidates, cellSize);
        Trace("  cell size %d: %zu candidate placements (best support %d)\n",
            cellSize, fits.size(), fits.empty() ? 0 : fits.front().support);

        for (const GridFit& fit : fits) {
            BoardCandidate candidate;
            candidate.cellSize = fit.cellSize;
            candidate.rect = cv::Rect(fit.originX, fit.originY,
                                      fit.cellSize * kBoardCells, fit.cellSize * kBoardCells);

            if (!VerifyBoard(bgr, candidate)) continue;

            // Prefer the board its squares agree on most, breaking ties by how
            // many squares were readable: a real board outranks a short run of
            // coincidentally square UI elements.
            const bool better = !best.has_value() ||
                candidate.confidence > best->confidence + 0.02 ||
                (std::abs(candidate.confidence - best->confidence) <= 0.02 &&
                 candidate.uniformCells > best->uniformCells);

            if (better) best = candidate;
        }
    }

    return best;
}

bool EstimatePieceColors(const cv::Mat& bgr, const BoardCandidate& board,
                         cv::Vec3b& darkPiece, cv::Vec3b& lightPiece, int& orientation) {
    if (bgr.empty() || board.cellSize <= 0) return false;

    struct OccupiedCell {
        int row = 0;
        cv::Vec3b color;
        int luminance = 0;
    };

    std::vector<OccupiedCell> occupied;

    for (int row = 0; row < kBoardCells; ++row) {
        for (int col = 0; col < kBoardCells; ++col) {
            const cv::Rect cell(
                board.rect.x + col * board.cellSize,
                board.rect.y + row * board.cellSize,
                board.cellSize, board.cellSize);

            const int inset = std::max(1, board.cellSize / 5);
            cv::Rect patch(cell.x + inset, cell.y + inset,
                           cell.width - 2 * inset, cell.height - 2 * inset);
            patch &= cv::Rect(0, 0, bgr.cols, bgr.rows);
            if (patch.width <= 0 || patch.height <= 0) continue;

            // Pixels matching neither square shade belong to the piece on top.
            const cv::Mat region = bgr(patch);
            std::vector<cv::Vec3b> piecePixels;
            piecePixels.reserve((size_t)region.total() / 4);

            for (int y = 0; y < region.rows; ++y) {
                const cv::Vec3b* scan = region.ptr<cv::Vec3b>(y);
                for (int x = 0; x < region.cols; ++x) {
                    if (ColorDistance(scan[x], board.lightSquare) > 40 &&
                        ColorDistance(scan[x], board.darkSquare) > 40) {
                        piecePixels.push_back(scan[x]);
                    }
                }
            }

            const double coverage = (double)piecePixels.size() / (double)region.total();
            if (coverage < 0.15) continue;

            OccupiedCell entry;
            entry.row = row;
            entry.color = MedianColor(piecePixels);
            entry.luminance = LuminanceOf(entry.color);
            occupied.push_back(entry);
        }
    }

    if (occupied.size() < 10) return false;

    // Two piece colours, so split the luminances at their widest gap. That is
    // more forgiving than a fixed threshold, which no single value fits across
    // themes ranging from near-white boards to near-black ones.
    std::vector<int> luminances;
    luminances.reserve(occupied.size());
    for (const auto& cell : occupied) luminances.push_back(cell.luminance);
    std::sort(luminances.begin(), luminances.end());

    int splitAt = 0;
    int widestGap = -1;
    for (size_t i = 1; i < luminances.size(); ++i) {
        const int gap = luminances[i] - luminances[i - 1];
        if (gap > widestGap) {
            widestGap = gap;
            splitAt = (luminances[i] + luminances[i - 1]) / 2;
        }
    }

    // Pieces sharing a colour vary by less than the distance between the two
    // piece colours, so too small a gap means only one colour is on the board.
    if (widestGap < 25) return false;

    std::vector<cv::Vec3b> darkColors, lightColors;
    int darkInTopRanks = 0;
    int darkInBottomRanks = 0;

    for (const auto& cell : occupied) {
        if (cell.luminance <= splitAt) {
            darkColors.push_back(cell.color);
            if (cell.row <= 1) ++darkInTopRanks;
            if (cell.row >= 6) ++darkInBottomRanks;
        }
        else {
            lightColors.push_back(cell.color);
        }
    }

    if (darkColors.empty() || lightColors.empty()) return false;
    if (darkInTopRanks == darkInBottomRanks) return false;

    darkPiece = MedianColor(darkColors);
    lightPiece = MedianColor(lightColors);

    // Black occupying the far ranks is the usual view, with white at the bottom.
    orientation = (darkInTopRanks > darkInBottomRanks) ? 0 : 1;

    return true;
}

void ApplyDerivedSampleGeometry(const BoardCandidate& board) {
    const int cellSize = board.cellSize;
    if (cellSize <= 0) return;

    g_debugPatchSize = std::max(3, cellSize / 10);

    // Three probes down the vertical centre of the square. Pieces are taller
    // than they are wide, so a column through the middle is the likeliest place
    // to land on piece pixels whatever the piece set.
    g_debugOffsetX = 0;
    g_debugOffsetY = 0;
    g_debugOffsetX2 = 0;
    g_debugOffsetY2 = -cellSize / 6;
    g_debugOffsetX3 = 0;
    g_debugOffsetY3 = cellSize / 6;

    // Wide enough to hold the whole glyph, short of the borders of the square.
    g_cropPatchSize = std::max(8, (cellSize * 78) / 100);
    g_cropOffsetX = 0;
    g_cropOffsetY = 0;

    // Scaled to how far apart this board's two shades actually are. The old
    // fixed default of 5 was tight enough that antialiased piece edges fell
    // outside it on most themes.
    const int separation = std::abs(LuminanceOf(board.lightSquare) - LuminanceOf(board.darkSquare));
    g_analysisTolerance = std::clamp(separation / 8, 8, 30);
}
