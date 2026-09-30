#include "ui/BoardOverlay.h"

#include "Globals.h"
#include "Structs.h"
#include "chess/ChessRules.h"
#include "engine/StockfishHandler.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace theme;

// Everything here is drawn straight onto the page, through a window that is
// colour keyed on black: pure black is transparent and every other pixel is
// opaque. So every colour is chosen as the opaque colour it will be, nothing
// relies on alpha to look translucent, and nothing is ever pure black.
namespace {

constexpr ImU32 kTrack = IM_COL32(88, 142, 255, 255);         // Following the game.
constexpr ImU32 kChrome = IM_COL32(20, 22, 27, 255);          // Backing for text over the page.
constexpr ImU32 kChromeBorder = IM_COL32(54, 59, 70, 255);
constexpr ImU32 kGuide = IM_COL32(88, 142, 255, 255);         // Calibration sample points.
constexpr ImU32 kGuideOutline = IM_COL32(214, 219, 228, 255); // Calibration crop regions.

struct OverlayState {
    std::vector<StockfishMove> moves;   // Only when they belong to the position on the board.
    std::string movesKey;               // Changes whenever a new set of moves arrives.
    bool movesAreOurs = true;
    bool movesPending = false;          // The engine has not answered for this position yet.
    std::string trackedFen;
    bool inSync = true;
    std::string verdict;
    std::string verdictSan;
    std::string verdictBestSan;
    int verdictLoss = 0;
    bool verdictByUs = false;
};

OverlayState TakeSnapshot() {
    OverlayState s;
    std::lock_guard<std::mutex> snapshot(g_analysisStateMutex);

    // Only once the engine has answered for the position now on the board.
    // Until then the moves on hand are the ones for the position before, which
    // after the opponent moves are the opponent's own candidates.
    const bool current = !g_sfBestMovesFen.empty() && g_sfBestMovesFen == g_trackedFen;
    if (current) {
        s.moves = g_sfBestMoves;
        s.movesKey = g_sfBestMovesFen;
        for (const StockfishMove& move : s.moves) s.movesKey += ' ' + move.uci;
    }
    s.movesPending = !current;
    s.movesAreOurs = g_sfMovesAreOurs;
    s.trackedFen = g_trackedFen;
    s.inSync = g_trackerInSync;
    s.verdict = g_lastMoveVerdict;
    s.verdictSan = g_lastMoveVerdictSan.empty() ? g_lastMoveVerdictUci : g_lastMoveVerdictSan;
    s.verdictBestSan = g_lastMoveVerdictBestSan;
    s.verdictLoss = g_lastMoveLossCp;
    s.verdictByUs = g_lastMoveVerdictByUs;
    return s;
}

enum class Result { Playing, WhiteMated, BlackMated, Stalemate };

// Read off the position rather than the engine: at checkmate the engine has no
// score to give, and its last one belongs to the move before.
Result ResultOf(const std::string& fen) {
    static std::string cachedFen;
    static Result cached = Result::Playing;
    if (fen == cachedFen) return cached;

    cachedFen = fen;
    cached = Result::Playing;
    if (const std::optional<ChessPosition> position = ChessPosition::FromFen(fen)) {
        if (position->LegalMoves().empty()) {
            const Color side = position->SideToMove();
            if (!position->IsInCheck(side)) cached = Result::Stalemate;
            else cached = side == Color::White ? Result::WhiteMated : Result::BlackMated;
        }
    }
    return cached;
}

float EaseOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Overshoots a touch before settling, so a badge arriving reads as placed.
float EaseOutBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float c = 1.4f;
    const float u = t - 1.0f;
    return 1.0f + (c + 1.0f) * u * u * u + c * u * u;
}

// A score from White's side, to one decimal: +0.7, -1.2, M3, with a real minus.
std::string ShortScore(bool mate, int mateInWhite, int cpWhite) {
    const char* minus = "\xE2\x88\x92";
    char text[32];
    if (mate) {
        if (mateInWhite == 0) return "#";
        std::snprintf(text, sizeof(text), "%sM%d", mateInWhite < 0 ? minus : "", std::abs(mateInWhite));
        return text;
    }
    if (std::abs(cpWhite) < 5) return "0.0";
    std::snprintf(text, sizeof(text), "%s%.1f", cpWhite < 0 ? minus : "+", std::abs(cpWhite) / 100.0f);
    return text;
}

/* TRACKING CORNERS. */

// Four corner marks just outside the board rather than a full outline: they
// say which board is being followed without drawing a line along the edge of
// every square on it. Blue while the game is being followed, amber once the
// board has stopped making sense.
void DrawCorners(ImDrawList* list, ImVec2 min, ImVec2 max, float cell, ImU32 color) {
    const float out = 3.0f;
    const float arm = std::clamp(cell * 0.28f, 10.0f, 30.0f);
    const float thickness = 2.0f;

    const ImVec2 a(min.x - out, min.y - out);
    const ImVec2 b(max.x + out, min.y - out);
    const ImVec2 c(max.x + out, max.y + out);
    const ImVec2 d(min.x - out, max.y + out);

    const ImVec2 corners[4][3] = {
        { ImVec2(a.x, a.y + arm), a, ImVec2(a.x + arm, a.y) },
        { ImVec2(b.x - arm, b.y), b, ImVec2(b.x, b.y + arm) },
        { ImVec2(c.x, c.y - arm), c, ImVec2(c.x - arm, c.y) },
        { ImVec2(d.x + arm, d.y), d, ImVec2(d.x, d.y - arm) },
    };
    for (const auto& corner : corners) {
        list->AddPolyline(corner, 3, color, ImDrawFlags_None, thickness);
    }
}

/* CALIBRATION GUIDES. */

void DrawCalibrationGuides(ImDrawList* list) {
    if (g_isConfiguringSamplePoints && !g_debugSamples.empty()) {
        for (const SAMPLE& sample : g_debugSamples) {
            list->AddRectFilled(ImVec2((float)sample.x, (float)sample.y),
                                ImVec2((float)(sample.x + sample.width), (float)(sample.y + sample.height)), kGuide);
        }
    }

    if (g_isConfiguringCropRegion && !g_cropRects.empty()) {
        for (const SAMPLE& crop : g_cropRects) {
            list->AddRect(ImVec2((float)crop.x, (float)crop.y),
                          ImVec2((float)(crop.x + crop.width), (float)(crop.y + crop.height)), kGuideOutline, 0.0f, 0, 1.0f);
        }
    }
}

/* EVALUATION BAR. */

// Beside the board, with the side being played at the bottom, the way chess
// sites draw theirs. The score rides the boundary between the two colours, so
// the number is always where the eye already is.
void DrawEvalBar(ImDrawList* list, const OverlayState& s, ImVec2 boardMin, ImVec2 boardMax, float cell) {
    const Fonts& fonts = GetFonts();
    const Result result = ResultOf(s.trackedFen);

    const bool haveEval = g_liveEvalValid.load();
    const bool isMate = g_liveEvalIsMate.load();
    const int mateIn = g_liveEvalMateInWhite.load();
    const int cp = g_liveEvalCpWhite.load();

    float target = 0.5f;
    std::string score;
    if (result == Result::WhiteMated || result == Result::BlackMated) {
        const bool whiteWon = result == Result::BlackMated;
        target = whiteWon ? 1.0f : 0.0f;
        score = "#";
    }
    else if (result == Result::Stalemate) {
        score = "Draw";
    }
    else if (haveEval) {
        if (isMate) {
            target = mateIn > 0 ? 1.0f : 0.0f;
        }
        else {
            // The usual logistic mapping from score to expected result, so the
            // interesting range stays legible and big leads saturate gently.
            const float chances = 2.0f / (1.0f + std::exp(-0.004f * (float)cp)) - 1.0f;
            target = std::clamp(0.5f + 0.5f * chances, 0.02f, 0.98f);
        }
        score = ShortScore(isMate, mateIn, cp);
    }

    // Glides towards each new depth's score rather than jumping to it.
    static float shownShare = 0.5f;
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    shownShare += (target - shownShare) * (1.0f - std::exp(-deltaTime / 0.20f));
    if (std::fabs(target - shownShare) < 0.0015f) shownShare = target;

    const float width = std::round(std::clamp(cell * 0.12f, 8.0f, 14.0f));
    const float gap = std::round(std::max(10.0f, cell * 0.09f));
    const float boardHeight = boardMax.y - boardMin.y;

    // Left of the board, unless the board is too close to the left edge.
    const float pillRoom = 64.0f;
    const bool onLeft = boardMin.x - gap - width - pillRoom >= 0.0f;
    const float left = onLeft ? boardMin.x - gap - width : boardMax.x + gap;
    const ImVec2 barMin(left, boardMin.y);
    const ImVec2 barMax(left + width, boardMax.y);
    const float radius = width * 0.5f;

    // White fills from the white side of the board, whichever way round it is.
    const bool whiteAtBottom = g_orientation != 1;
    const float whiteHeight = boardHeight * shownShare;
    const float split = whiteAtBottom ? boardMax.y - whiteHeight : boardMin.y + whiteHeight;

    list->AddRectFilled(ImVec2(barMin.x - 1.0f, barMin.y - 1.0f), ImVec2(barMax.x + 1.0f, barMax.y + 1.0f),
                        IM_COL32(8, 9, 11, 255), radius + 1.0f);
    list->AddRectFilled(barMin, barMax, kBlackSide, radius);
    if (whiteHeight > 0.5f) {
        const ImVec2 whiteMin(barMin.x, whiteAtBottom ? split : barMin.y);
        const ImVec2 whiteMax(barMax.x, whiteAtBottom ? barMax.y : split);
        const bool full = shownShare >= 0.995f;
        list->AddRectFilled(whiteMin, whiteMax, kWhiteSide, radius,
            full ? ImDrawFlags_RoundCornersAll
                 : (whiteAtBottom ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersTop));
    }

    // Level, marked across the bar, so a small edge still reads as an edge.
    const float middle = std::round(boardMin.y + boardHeight * 0.5f) + 0.5f;
    list->AddLine(ImVec2(barMin.x, middle), ImVec2(barMax.x, middle), IM_COL32(128, 136, 149, 255), 1.0f);

    if (score.empty()) return;

    // The score, riding the boundary.
    const float size = std::clamp(cell * 0.13f, 12.0f, 15.0f);
    const float textWidth = fonts.strong->CalcTextSizeA(size, FLT_MAX, 0.0f, score.c_str()).x;
    // The plain flag rather than a probe: this runs every frame, menu or not,
    // and must never wait on the engine.
    const bool searching = s.movesPending && result == Result::Playing && g_sfRunning && !StockfishIsLoading();
    const float spinnerRoom = searching ? size * 0.95f : 0.0f;
    const float pillWidth = std::round(textWidth + size * 1.3f + spinnerRoom);
    const float pillHeight = std::round(size * 1.75f);
    const float centerY = std::clamp(split, boardMin.y + pillHeight * 0.5f, boardMax.y - pillHeight * 0.5f);
    const float pillLeft = onLeft ? barMin.x - 7.0f - pillWidth : barMax.x + 7.0f;
    const ImVec2 pillMin(std::round(pillLeft), std::round(centerY - pillHeight * 0.5f));
    const ImVec2 pillMax(pillMin.x + pillWidth, pillMin.y + pillHeight);

    list->AddRectFilled(pillMin, pillMax, kChromeBorder, pillHeight * 0.5f);
    list->AddRectFilled(ImVec2(pillMin.x + 1.0f, pillMin.y + 1.0f), ImVec2(pillMax.x - 1.0f, pillMax.y - 1.0f),
                        kChrome, pillHeight * 0.5f - 1.0f);

    const float pillMiddle = (pillMin.y + pillMax.y) * 0.5f;
    float textLeft = pillMin.x + size * 0.65f;
    if (searching) {
        // Still searching this position: suggestions are on their way.
        const ImVec2 center(textLeft + size * 0.30f, pillMiddle);
        const float start = (float)ImGui::GetTime() * 6.0f;
        list->PathArcTo(center, size * 0.30f, start, start + 3.14159265f * 1.4f, 14);
        list->PathStroke(kTrack, ImDrawFlags_None, 1.6f);
        textLeft += spinnerRoom;
    }
    ui::DrawCenteredText(list, fonts.strong, size, ImVec2(textLeft + textWidth * 0.5f, pillMiddle), kText, score.c_str());
}

/* CALLOUT. */

// One line above the board for the one thing most worth knowing without
// opening the menu: what the last move cost, or how the game ended.
struct Segment {
    std::string text;
    ImFont* font;
    ImU32 color;
};

void DrawCallout(ImDrawList* list, const OverlayState& s, ImVec2 boardMin, ImVec2 boardMax, float cell) {
    const Fonts& fonts = GetFonts();
    const Result result = ResultOf(s.trackedFen);
    if (s.verdict.empty() && result == Result::Playing) return;

    const float size = std::clamp(cell * 0.15f, 12.0f, 16.0f);
    const bool gameOver = result != Result::Playing;
    std::string chipText;
    ImU32 chipFill = kLevel;
    std::vector<Segment> segments;
    std::string key;

    if (gameOver) {
        const bool mate = result != Result::Stalemate;
        const bool whiteWon = result == Result::BlackMated;
        const bool weWon = mate && whiteWon == g_sfPlayWhite;
        chipText = mate ? "#" : "\xC2\xBD";
        chipFill = !mate ? kLevel : (weWon ? kWinning : kLosing);
        segments.push_back({ mate ? "Checkmate," : "Stalemate,", fonts.strong, kText });
        segments.push_back({ ResultForPlayer(mate, whiteWon, g_sfPlayWhite), fonts.body, kTextDim });
        key = "end " + s.trackedFen;
    }
    else {
        // What it cost, in words and without a sign: a minus here beside the
        // bar's plus for the same position read as a contradiction.
        char loss[32];
        std::snprintf(loss, sizeof(loss), "%.1f", s.verdictLoss / 100.0f);

        segments.push_back({ s.verdictByUs ? "You played" : "They played", fonts.body, kTextDim });
        segments.push_back({ s.verdictSan, fonts.strong, kText });
        segments.push_back({ "lost", fonts.body, kTextDim });
        segments.push_back({ loss, fonts.strong, Mix(VerdictColor(s.verdict.c_str()), kText, 0.25f) });
        if (!s.verdictBestSan.empty()) {
            segments.push_back({ "\xC2\xB7", fonts.body, kTextFaint });
            segments.push_back({ "best", fonts.body, kTextDim });
            segments.push_back({ s.verdictBestSan, fonts.strong, kText });
        }
        key = s.verdict + s.verdictSan + loss + (s.verdictByUs ? "u" : "t");
    }

    // Slides into place when it changes. Movement only: a fading colour on
    // this window would be blended with black, not with the page.
    static std::string shownKey;
    static double shownAt = 0.0;
    const double now = ImGui::GetTime();
    if (key != shownKey) {
        shownKey = key;
        shownAt = now;
    }
    const float arrive = EaseOutCubic((float)(now - shownAt) / 0.20f);

    const float spacing = std::round(size * 0.42f);
    const float chipWidth = gameOver
        ? std::round(fonts.strong->CalcTextSizeA(size, FLT_MAX, 0.0f, chipText.c_str()).x + size)
        : ui::VerdictChipWidth(s.verdict.c_str(), size);
    float contentWidth = chipWidth;
    for (const Segment& segment : segments) {
        contentWidth += spacing + segment.font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment.text.c_str()).x;
    }

    const float inset = std::round(size * 0.30f);
    const float height = std::round(size * 2.1f);
    const float width = std::round(contentWidth + inset + size * 0.85f);
    const float centerX = (boardMin.x + boardMax.x) * 0.5f;

    float top = boardMin.y - height - 10.0f;
    float from = top + 8.0f;
    if (top < 4.0f) {
        // No room above the board, so below it.
        top = boardMax.y + 10.0f;
        from = top - 8.0f;
    }
    top = std::round(from + (top - from) * arrive);

    const ImVec2 boxMin(std::round(centerX - width * 0.5f), top);
    const ImVec2 boxMax(boxMin.x + width, top + height);
    list->AddRectFilled(boxMin, boxMax, kChromeBorder, height * 0.5f);
    list->AddRectFilled(ImVec2(boxMin.x + 1.0f, boxMin.y + 1.0f), ImVec2(boxMax.x - 1.0f, boxMax.y - 1.0f),
                        kChrome, height * 0.5f - 1.0f);

    const float middle = (boxMin.y + boxMax.y) * 0.5f;
    const float chipHeight = height - inset * 2.0f;
    const ImVec2 chipMin(boxMin.x + inset, middle - chipHeight * 0.5f);
    const ImVec2 chipMax(chipMin.x + chipWidth, middle + chipHeight * 0.5f);
    if (gameOver) {
        list->AddRectFilled(chipMin, chipMax, chipFill, chipHeight * 0.5f);
        ui::DrawCenteredText(list, fonts.strong, size, ImVec2((chipMin.x + chipMax.x) * 0.5f, middle),
                             IM_COL32(255, 255, 255, 255), chipText.c_str());
    }
    else {
        ui::DrawVerdictChip(list, chipMin, chipHeight, size, s.verdict.c_str());
    }

    float x = chipMax.x + spacing;
    for (const Segment& segment : segments) {
        const float segmentWidth = segment.font->CalcTextSizeA(size, FLT_MAX, 0.0f, segment.text.c_str()).x;
        ui::DrawCenteredText(list, segment.font, size, ImVec2(x + segmentWidth * 0.5f, middle), segment.color,
                             segment.text.c_str());
        x += segmentWidth + spacing;
    }
}

/* SUGGESTIONS. */

void DrawSuggestions(ImDrawList* draw_list, const OverlayState& s) {
    const std::vector<StockfishMove>& bestMoves = s.moves;

    const int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
    const int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

    // A new set of suggestions arrives with a short movement: badges settle
    // into place and arrows extend from the piece towards its square, one rank
    // after another. Movement only, never fading: see the note at the top.
    static std::string shownKey;
    static double shownAt = 0.0;
    const double now = ImGui::GetTime();
    if (s.movesKey != shownKey) {
        shownKey = s.movesKey;
        shownAt = now;
    }
    auto progress = [&](int rank, float duration) {
        return (float)(now - shownAt - 0.045 * rank) / duration;
    };

    // Every suggestion is labelled with its rank, 1 being the engine's first
    // choice, on both the square it leaves and the square it lands on, so a
    // move can be read off the board without consulting the menu.
    struct Badge {
        std::string text;
        ImU32 fill = 0;
        bool destination = false;
        int moveIndex = 0;    // Which suggestion this badge belongs to.
        ImVec2 center{};      // Filled in by the layout pass below.
        float radius = 0.0f;  // Filled in by the layout pass below.
    };

    // Gathered per square before anything is drawn. Several moves commonly
    // touch the same square, and stacking their labels would make all but
    // the last unreadable, so each square lays its badges out in a row.
    std::map<int, std::vector<Badge>> badgesBySquare;

    auto squareIndex = [&](char fileChar, char rankChar) -> int {
        int col = fileChar - 'a';
        int row = '8' - rankChar;
        if (col < 0 || col > 7 || row < 0 || row > 7) return -1;
        if (g_orientation == 1) { // Black at the bottom.
            col = 7 - col;
            row = 7 - row;
        }
        return row * 8 + col;
    };

    // Both the badges and the ends of the arrows sit here, in the top left
    // of a square. Pieces are drawn centred, so this corner is the emptiest
    // part of a square, and anchoring both to the same point makes an arrow
    // read as a line between two numbered markers rather than a separate
    // decoration laid over the board.
    const float anchorInset = (float)std::min(cellWidth, cellHeight) * 0.20f;
    auto squareAnchor = [&](int index) {
        const int row = index / 8;
        const int col = index % 8;
        return ImVec2(
            (float)(g_boardRect.left + col * cellWidth) + anchorInset,
            (float)(g_boardRect.top + row * cellHeight) + anchorInset);
    };

    const float badgeRadius = std::clamp((float)std::min(cellWidth, cellHeight) * 0.095f, 6.0f, 14.0f);

    for (size_t i = 0; i < bestMoves.size(); ++i) {
        const StockfishMove& move = bestMoves[i];
        if (move.uci.length() < 4) continue;

        // Colour carries the evaluation, the number carries the ranking. Scores
        // arrive from the moving side's point of view, and moves are only
        // drawn on our own turn, so a positive score is good for us.
        const std::string label = std::to_string((int)i + 1);
        const ImU32 color = EvaluationColor(move.mate, move.mateIn, move.scoreCp);

        const int from = squareIndex(move.uci[0], move.uci[1]);
        const int to = squareIndex(move.uci[2], move.uci[3]);
        if (from >= 0) badgesBySquare[from].push_back({ label, color, false, (int)i });
        if (to >= 0) badgesBySquare[to].push_back({ label, color, true, (int)i });
    }

    /* BADGE LAYOUT. */
    // Worked out before anything is drawn, because an arrow has to start and
    // end on the badge that belongs to it. A piece with two suggestions puts
    // two badges in a row on the square it stands on, and an arrow anchored
    // to the square rather than to its own badge leaves from underneath the
    // wrong number.
    struct Endpoint {
        bool set = false;
        ImVec2 center{};
        float radius = 0.0f;
    };
    std::vector<Endpoint> sourceOf(bestMoves.size());
    std::vector<Endpoint> targetOf(bestMoves.size());

    for (auto& entry : badgesBySquare) {
        std::vector<Badge>& badges = entry.second;
        const int count = (int)badges.size();
        if (count == 0) continue;

        // Shrink the row to fit rather than letting it spill onto the
        // neighbouring squares. Several moves touching one square is common.
        float radius = badgeRadius;
        float gap = radius * 0.35f;
        float rowWidth = count * radius * 2.0f + (count - 1) * gap;

        // Anchored in the top left corner, so the row runs from there rather
        // than across the middle of the square.
        const ImVec2 anchor = squareAnchor(entry.first);
        const float available = (float)cellWidth - anchorInset - radius;
        if (rowWidth > available && rowWidth > 0.0f) {
            radius *= available / rowWidth;
            gap = radius * 0.35f;
            rowWidth = count * radius * 2.0f + (count - 1) * gap;
        }

        float centerX = anchor.x;
        for (Badge& badge : badges) {
            badge.radius = radius;
            badge.center = ImVec2(centerX, anchor.y);

            Endpoint& endpoint = badge.destination ? targetOf[badge.moveIndex]
                                                   : sourceOf[badge.moveIndex];
            endpoint.set = true;
            endpoint.center = badge.center;
            endpoint.radius = radius;

            centerX += radius * 2.0f + gap;
        }
    }

    /* ARROWS. */
    if (g_showMoveArrows) {
        const float cellSide = (float)std::min(cellWidth, cellHeight);

        // Drawn weakest first so the engine's first choice ends up on top of
        // the ones it likes less.
        for (int i = (int)bestMoves.size() - 1; i >= 0; --i) {
            const Endpoint& tail = sourceOf[i];
            const Endpoint& head = targetOf[i];
            if (!tail.set || !head.set) continue;

            // Black, with rank carried by weight and opacity. Colouring the
            // arrows as well as the badges said the same thing twice and put
            // a second saturated colour across the board; one neutral line
            // between two coloured markers reads more cleanly.
            const float thickness = std::max(1.5f, cellSide * (0.050f - 0.008f * i));
            const int alpha = std::max(70, 200 - 42 * i);
            const ImU32 color = IM_COL32(12, 12, 14, alpha);

            const ImVec2 start = tail.center;
            const ImVec2 end = head.center;

            float dx = end.x - start.x;
            float dy = end.y - start.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length < 1.0f) continue;
            dx /= length;
            dy /= length;

            // The arrow runs between the two badges, stopping clear of each
            // by its own radius, so it never crosses the middle of a square
            // where the pieces are and never runs under a number.
            const float clearTail = tail.radius + 3.0f;
            const float clearHead = head.radius + 3.0f;
            const float headLength = cellSide * 0.18f;
            if (length <= clearTail + clearHead + headLength) continue;

            // Extends from the piece towards its square as it arrives.
            const float reach = EaseOutCubic(progress(i, 0.22f));
            if (reach <= 0.0f) continue;

            const ImVec2 shaftStart(start.x + dx * clearTail, start.y + dy * clearTail);
            const float span = length - clearTail - clearHead;
            const ImVec2 tip(shaftStart.x + dx * span * reach, shaftStart.y + dy * span * reach);
            const float headNow = std::min(headLength, span * reach);
            const ImVec2 shaftEnd(tip.x - dx * headNow, tip.y - dy * headNow);

            // Rounded tail. AddLine has square ends, which read as ragged at
            // these weights; a disc the width of the shaft closes it off.
            draw_list->AddCircleFilled(shaftStart, thickness * 0.5f, color, 12);
            draw_list->AddLine(shaftStart, shaftEnd, color, thickness);

            // Arrowhead, built on the perpendicular at the end of the shaft.
            const float halfWidth = headLength * 0.44f;
            draw_list->AddTriangleFilled(
                tip,
                ImVec2(shaftEnd.x - dy * halfWidth, shaftEnd.y + dx * halfWidth),
                ImVec2(shaftEnd.x + dy * halfWidth, shaftEnd.y - dx * halfWidth),
                color);
        }
    }

    /* BADGES. */
    // Small discs rather than labels. A rank is one character, so a circle
    // sized to that character is the least ink that can carry it, and it
    // stays legible over a piece without covering one. Positions come from
    // the layout pass, so these land exactly where the arrows expect them.
    // A destination is solid and a source is a ring, so the two ends of one
    // move stay distinguishable without a second colour.
    for (const auto& entry : badgesBySquare) {
        for (const Badge& badge : entry.second) {
            if (badge.radius <= 0.0f) continue;
            const float settle = EaseOutBack(progress(badge.moveIndex, 0.20f));
            if (settle <= 0.0f) continue;
            ui::DrawRankDisc(draw_list, badge.center, badge.radius * (0.55f + 0.45f * settle), badge.fill,
                             !badge.destination, badge.text.c_str());
        }
    }
}

} // namespace

void DrawBoardOverlay(ImDrawList* draw_list) {
    const bool haveBoard = !g_isRescanning &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0;
    if (!haveBoard) return;

    const ImVec2 boardMin((float)g_boardRect.left, (float)g_boardRect.top);
    const ImVec2 boardMax((float)g_boardRect.right, (float)g_boardRect.bottom);
    const float cell = (boardMax.x - boardMin.x) / 8.0f;
    const bool configuring = g_isConfiguringSamplePoints || g_isConfiguringCropRegion;

    DrawCalibrationGuides(draw_list);

    // Copied out of the shared state first: the detection thread reallocates
    // these vectors and the strings inside them.
    const OverlayState state = TakeSnapshot();

    const bool following = g_hasAnalysisStarted && !configuring;
    DrawCorners(draw_list, boardMin, boardMax, cell,
                !following ? kTextDim : (state.inSync ? kTrack : kWarn));

    if (!following) return;

    DrawEvalBar(draw_list, state, boardMin, boardMax, cell);
    DrawCallout(draw_list, state, boardMin, boardMax, cell);

    // Not while the board has stopped matching the game: the moves belong to a
    // position that may no longer be the one on screen, and drawn at full
    // weight they would look as trustworthy as ever.
    if (state.inSync && state.movesAreOurs && !state.moves.empty()) {
        DrawSuggestions(draw_list, state);
    }
}
