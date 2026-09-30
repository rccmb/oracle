#include "ui/Menu.h"

#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui_internal.h"

#include "chess/ChessRules.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#pragma comment(lib, "comdlg32")

using namespace theme;

namespace {

/* STATE. */

int g_tab = (int)MenuTab::Play;
bool g_hideRequested = false;

// What the last press of Detect board found, or why it found nothing.
std::string g_detectStatus;
bool g_detectFailed = false;

// Everything the detection thread publishes, copied once per frame under its
// lock. Reading the shared state directly would race with the thread
// reallocating it mid frame.
struct Snapshot {
    std::vector<std::string> rows;
    std::vector<StockfishMove> moves;
    bool movesCurrent = false;     // The moves belong to the position on the board.
    std::string trackedFen;
    std::string lastMoveUci;
    std::string lastMoveSan;
    int ply = 0;
    bool inSync = true;
    bool movesAreOurs = true;
    char sideToMove = 'w';
    std::string verdict;
    std::string verdictSan;
    std::string verdictBestSan;
    int verdictLoss = 0;
    bool verdictByUs = false;
};

Snapshot TakeSnapshot() {
    Snapshot s;
    std::lock_guard<std::mutex> lock(g_analysisStateMutex);
    s.rows = g_boardGridRows;
    s.moves = g_sfBestMoves;
    s.movesCurrent = !g_sfBestMovesFen.empty() && g_sfBestMovesFen == g_trackedFen;
    s.trackedFen = g_trackedFen;
    s.lastMoveUci = g_lastMoveUci;
    s.lastMoveSan = g_lastMoveSan.empty() ? g_lastMoveUci : g_lastMoveSan;
    s.ply = g_trackerPly;
    s.inSync = g_trackerInSync;
    s.movesAreOurs = g_sfMovesAreOurs;
    s.sideToMove = g_sideToMove;
    s.verdict = g_lastMoveVerdict;
    s.verdictSan = g_lastMoveVerdictSan.empty() ? g_lastMoveVerdictUci : g_lastMoveVerdictSan;
    s.verdictBestSan = g_lastMoveVerdictBestSan;
    s.verdictLoss = g_lastMoveLossCp;
    s.verdictByUs = g_lastMoveVerdictByUs;
    return s;
}

// Checkmate and stalemate, read off the position itself rather than inferred
// from the engine going quiet. Worked out once per position.
enum class GameState { Playing, Checkmate, Stalemate };

struct PositionFacts {
    GameState state = GameState::Playing;
    int fullmove = 1;
    int checkedKing = -1;   // FEN index of a king in check, or -1.
};

PositionFacts FactsFor(const std::string& fen) {
    static std::string cachedFen;
    static PositionFacts cached;
    if (fen == cachedFen) return cached;

    cachedFen = fen;
    cached = PositionFacts();
    if (const std::optional<ChessPosition> position = ChessPosition::FromFen(fen)) {
        cached.fullmove = position->FullmoveNumber();
        const Color side = position->SideToMove();
        const bool inCheck = position->IsInCheck(side);
        if (position->LegalMoves().empty()) {
            cached.state = inCheck ? GameState::Checkmate : GameState::Stalemate;
        }
        if (inCheck) {
            const char king = side == Color::White ? 'K' : 'k';
            for (int square = 0; square < 64; ++square) {
                if (position->PieceAt(square) == king) cached.checkedKing = square;
            }
        }
    }
    return cached;
}

/* FORMATTING. */

const char* kMinus = "\xE2\x88\x92";   // U+2212, a real minus sign.

// A score from White's side: +0.66, -1.20, M3, -M3, with a true minus sign.
std::string FormatScore(bool mate, int mateInWhite, int cpWhite, int decimals) {
    char text[32];
    if (mate) {
        if (mateInWhite == 0) return "#";
        std::snprintf(text, sizeof(text), "%sM%d", mateInWhite < 0 ? kMinus : "", std::abs(mateInWhite));
        return text;
    }
    if (cpWhite == 0) return decimals >= 2 ? "0.00" : "0.0";
    std::snprintf(text, sizeof(text), "%s%.*f", cpWhite < 0 ? kMinus : "+", decimals, std::abs(cpWhite) / 100.0f);
    return text;
}

// The score in words, the way a commentator would put it.
std::string DescribeScore(bool mate, int mateInWhite, int cpWhite) {
    if (mate) {
        if (mateInWhite == 0) return "Checkmate";
        return std::string(mateInWhite > 0 ? "White" : "Black") + " mates in " + std::to_string(std::abs(mateInWhite));
    }
    const int size = std::abs(cpWhite);
    const char* side = cpWhite > 0 ? "White" : "Black";
    if (size < 25) return "Level";
    if (size < 75) return std::string(side) + " is slightly better";
    if (size < 200) return std::string(side) + " is better";
    return std::string(side) + " is winning";
}

// "12. e4" or "12... e5", numbered the way a scoresheet numbers moves.
std::string NumberedMove(int fullmoveNow, char sideToMoveNow, const std::string& san) {
    // Played by the side not to move now. After Black's move the counter has
    // already ticked over.
    const bool playedByWhite = sideToMoveNow == 'b';
    const int number = playedByWhite ? fullmoveNow : fullmoveNow - 1;
    return std::to_string(std::max(1, number)) + (playedByWhite ? ". " : "\xE2\x80\xA6 ") + san;
}

/* ACTIONS. */

void DetectBoard() {
    g_detectStatus.clear();
    g_detectFailed = false;

    cv::Mat capture = CaptureVirtualScreen();
    std::optional<BoardCandidate> board = capture.empty()
        ? std::nullopt
        : DetectChessboard(capture);

    cv::Vec3b darkPiece, lightPiece;
    int orientation = -1;

    if (!board) {
        g_detectFailed = true;
        g_detectStatus = "No board found. Make sure the whole board is on screen, "
                         "then try again or calibrate by hand.";
        return;
    }
    if (!EstimatePieceColors(capture, *board, darkPiece, lightPiece, orientation)) {
        g_detectFailed = true;
        g_detectStatus = "Found a board, but not the pieces. Set it up at the starting "
                         "position and try again.";
        return;
    }

    g_userScreenshotColor = capture;
    cv::cvtColor(g_userScreenshotColor, g_userScreenshotGray, cv::COLOR_BGR2GRAY);
    g_userScreenshotReady = true;

    g_boardRect = { board->rect.x, board->rect.y,
                    board->rect.x + board->rect.width,
                    board->rect.y + board->rect.height };

    g_refBoardColor1Color = board->lightSquare;
    g_refBoardColor2Color = board->darkSquare;
    g_refBoardColor1 = LuminanceOf(board->lightSquare);
    g_refBoardColor2 = LuminanceOf(board->darkSquare);

    g_refBlackPieceColor = darkPiece;
    g_refWhitePieceColor = lightPiece;
    g_refBlackPiece = LuminanceOf(darkPiece);
    g_refWhitePiece = LuminanceOf(lightPiece);
    g_orientation = orientation;

    // Sites turn the board so the player's own pieces are at the bottom, so
    // that is the side to suggest moves for. The Play tab can still change it.
    g_sfPlayWhite = orientation == 0;

    ApplyDerivedSampleGeometry(*board);
    UpdateDebugSamples();
    UpdateCropRects();

    GenerateReferencePieceCrops(g_userScreenshotColor, board->cellSize, board->cellSize);

    // Everything the manual flow would have asked for is now known, so skip
    // straight past its stages.
    g_boardClicksReady = true;
    g_clickStage = 2;
    g_samplePointsSet = true;
    g_cropRegionSet = true;
    g_isConfiguringSamplePoints = false;
    g_isConfiguringCropRegion = false;
    g_isRescanning = false;
    g_noBoard = false;
    g_trackerResetRequested = true;
    g_hasAnalysisStarted = true;

    char summary[200];
    std::snprintf(summary, sizeof(summary),
        "Board at %d, %d, %d \xC3\x97 %d px, %d px squares, %s at the bottom.",
        board->rect.x, board->rect.y, board->rect.width, board->rect.height,
        board->cellSize, orientation == 0 ? "White" : "Black");
    g_detectStatus = summary;
}

void RescanBoard() {
    g_detectStatus.clear();
    g_detectFailed = false;
    g_trackerResetRequested = true;
    g_hasAnalysisStarted = false;
    g_isConfiguringSamplePoints = true;
    g_isRescanning = true;
    g_isConfiguringCropRegion = false;
    g_boardClicksReady = false;
    g_userScreenshotReady = false;
    g_userScreenshotGray.release();
    g_clickStage = 0;
    g_viewFirstClick = { -1, -1, 0 };
    g_viewSecondClick = { -1, -1, 0 };
    g_debugSamples.clear();
    g_cropRects.clear();
    g_boardRect = { 0, 0, 0, 0 };
    g_clicks = { g_viewFirstClick, g_viewSecondClick };
    g_debugPatchSize = 5;
    g_debugOffsetX = 0;
    g_debugOffsetY = 0;
    g_samplePointsSet = false;
    g_cropRegionSet = false;
    g_cropPatchSize = 10;
    g_cropOffsetX = 0;
    g_cropOffsetY = 0;
    g_refBlackPiece = -1;
    g_refWhitePiece = -1;
    g_refBoardColor1 = -1;
    g_refBoardColor2 = -1;
    g_noBoard = true;
    g_prevLetterDrawQueue.clear();
    {
        // Cleared from the render thread while the detection thread may be mid
        // publish, so this takes the same lock the publisher does.
        std::lock_guard<std::mutex> reset(g_analysisStateMutex);
        g_detectedLetters.assign(64, ' ');
        g_boardGridRows.assign(8, std::string(8, ' '));
        g_sfBestMoves.clear();
        g_sfBestMovesFen.clear();
    }
}

// The Windows file picker, for choosing an engine rather than typing its path.
bool BrowseForEngine(char* path, size_t size) {
    wchar_t file[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, file, MAX_PATH);

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    dialog.lpstrFilter = L"Engines (*.exe)\0*.exe\0All files\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = L"Choose a UCI engine";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (!GetOpenFileNameW(&dialog)) return false;
    WideCharToMultiByte(CP_UTF8, 0, file, -1, path, (int)size, nullptr, nullptr);
    return true;
}

/* PIECES OF THE PANEL. */

// A path too long for its space loses its start rather than its end, since
// the file name is the part worth reading. In the current font.
std::string EllipsizeStart(const std::string& text, float maxWidth) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

    const char* ellipsis = "\xE2\x80\xA6";
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
    size_t start = 0;
    while (start < text.size()) {
        // Step over whole UTF-8 sequences, never into the middle of one.
        ++start;
        while (start < text.size() && ((unsigned char)text[start] & 0xC0) == 0x80) ++start;
        if (ImGui::CalcTextSize(text.c_str() + start).x + ellipsisWidth <= maxWidth) break;
    }
    return std::string(ellipsis) + text.substr(start);
}

// Right-aligns the next item, of the given width, on the current line.
void AlignRight(float width) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
}

void DrawHeader(const EngineStatus& engine, bool engineAlive, bool searching) {
    const Fonts& fonts = GetFonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float rowHeight = Px(28.0f);
    const float width = ImGui::GetContentRegionAvail().x;

    // The mark: a lens on an accent tile, for a tool that works by looking.
    const float tile = Px(24.0f);
    const ImVec2 tileMin(origin.x, origin.y + (rowHeight - tile) * 0.5f);
    const ImVec2 tileCenter(tileMin.x + tile * 0.5f, tileMin.y + tile * 0.5f);
    list->AddRectFilled(tileMin, ImVec2(tileMin.x + tile, tileMin.y + tile), kAccent, Px(6.0f));
    list->AddCircle(tileCenter, tile * 0.29f, IM_COL32(255, 255, 255, 255), 32, Px(2.0f));
    list->AddCircleFilled(ImVec2(tileCenter.x + tile * 0.06f, tileCenter.y - tile * 0.06f), tile * 0.11f,
                          IM_COL32(255, 255, 255, 255), 20);
    ui::DrawCenteredText(list, fonts.strong, Px(kSizeTitle),
        ImVec2(tileMin.x + tile + Px(9.0f) + fonts.strong->CalcTextSizeA(Px(kSizeTitle), FLT_MAX, 0.0f, "oracle").x * 0.5f,
               origin.y + rowHeight * 0.5f),
        kText, "oracle");

    // Engine status, then hide and quit, from the right.
    const float buttons = Px(28.0f) * 2.0f + Px(2.0f);
    const char* label = engine.loading ? "Starting engine"
                      : !engineAlive ? "No engine"
                      : engine.name.empty() ? "Engine ready" : engine.name.c_str();
    const ImU32 dot = engine.loading ? kWarn : (engineAlive ? kGood : kBad);

    ImGui::PushFont(nullptr, kSizeSmall);
    const float labelWidth = std::min(ImGui::CalcTextSize(label).x, Px(170.0f));
    const float statusWidth = Px(8.0f) + Px(7.0f) + labelWidth;
    const float statusX = origin.x + width - buttons - Px(10.0f) - statusWidth;
    const float textY = origin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(statusX, textY));
    ui::StatusDot(dot, engine.loading || searching);
    ImGui::SameLine(0.0f, Px(7.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushClipRect(ImVec2(statusX, origin.y), ImVec2(statusX + statusWidth, origin.y + rowHeight), true);
    ImGui::TextUnformatted(label);
    ImGui::PopClipRect();
    ImGui::PopStyleColor();
    if (!engine.resolved.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("%s", engine.resolved.c_str());
    }
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(origin.x + width - buttons, origin.y));
    if (ui::IconButton("hide", ICON_MINIMIZE, "Hide  (Ctrl+F1)")) g_hideRequested = true;
    ImGui::SameLine(0.0f, Px(2.0f));
    if (ui::IconButton("quit", ICON_CLOSE, "Quit Oracle", true)) PostQuitMessage(0);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rowHeight));
    ImGui::Dummy(ImVec2(width, 0.0f));
}

// One line of the setup checklist: its state, what it is about, and a detail.
void SetupStep(const char* icon, ImU32 iconColor, bool busy, const char* title, const char* detail,
               float reserveRight) {
    const Fonts& fonts = GetFonts();
    const float iconColumn = Px(26.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    if (busy) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + Px(1.0f), origin.y));
        ui::Spinner(Px(8.0f), kWarn);
    }
    else {
        ImGui::GetWindowDrawList()->AddText(fonts.body, Px(16.0f), ImVec2(origin.x, origin.y + Px(1.0f)),
                                            iconColor, icon);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + iconColumn, origin.y));
    ImGui::BeginGroup();
    ImGui::PushFont(fonts.strong, 0.0f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(5.0f));
    ImGui::PushFont(nullptr, kSizeSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - reserveRight);
    ImGui::TextUnformatted(detail);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::EndGroup();
}

void DrawSetup(const EngineStatus& engine, bool engineAlive) {
    ui::BeginCard("setup");
    {
        const char* engineDetail = engine.loading ? "Starting\xE2\x80\xA6"
                                 : engineAlive ? (engine.name.empty() ? "Ready" : engine.name.c_str())
                                 : "Not running. Choose one in the Engine tab.";
        const float changeWidth = ImGui::CalcTextSize("Change").x + Px(22.0f);
        const float rowTop = ImGui::GetCursorPosY();
        SetupStep(engineAlive ? ICON_DONE : ICON_ERROR, engineAlive ? kGood : kBad, engine.loading,
                  "Engine", engineDetail, changeWidth);

        // The engine comes first on purpose: detecting a board starts the
        // analysis at once, so this is where to pick a different one.
        const float rowBottom = ImGui::GetCursorPosY();
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - changeWidth,
                                   rowTop + (rowBottom - rowTop - Px(28.0f)) * 0.5f - Px(3.0f)));
        if (ui::Button("Change", ui::ButtonKind::Ghost, ImVec2(changeWidth, Px(28.0f)))) {
            g_tab = (int)MenuTab::Engine;
        }
        ImGui::SetCursorPosY(rowBottom);

        ImGui::Dummy(ImVec2(0.0f, Px(1.0f)));
        const ImVec2 lineStart = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(lineStart,
            ImVec2(lineStart.x + ImGui::GetContentRegionAvail().x, lineStart.y), kLine, 1.0f);
        ImGui::Dummy(ImVec2(0.0f, Px(3.0f)));

        SetupStep(ICON_PENDING, kTextFaint, false, "Board",
                  "At the starting position and fully on screen. The pieces are read from their starting squares.",
                  0.0f);
    }
    ui::EndCard();

    ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
    if (ui::Button(ICON_SEARCH "   Detect board", ui::ButtonKind::Primary,
                   ImVec2(ImGui::GetContentRegionAvail().x, Px(38.0f)))) {
        DetectBoard();
    }

    if (g_detectFailed && !g_detectStatus.empty()) {
        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        ui::Banner("detectFailed", ICON_WARNING, kWarn, g_detectStatus.c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
    ImGui::PushFont(nullptr, kSizeSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
    ImGui::TextUnformatted("Board not found?");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, Px(4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccentText);
    ImGui::TextUnformatted("Calibrate by hand");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) g_tab = (int)MenuTab::Calibrate;
    ImGui::PopFont();
}

void DrawEvaluation(const Snapshot& s, const PositionFacts& facts, bool searching) {
    const Fonts& fonts = GetFonts();

    const bool haveEval = g_liveEvalValid.load();
    const bool isMate = g_liveEvalIsMate.load();
    const int mateIn = g_liveEvalMateInWhite.load();
    const int cp = g_liveEvalCpWhite.load();
    const int depth = g_liveEvalDepth.load();

    // A finished game is read from the position: the engine has nothing left
    // to score in it, so its last number belongs to the move before.
    std::string score;
    std::string words;
    float target = 0.5f;
    if (facts.state == GameState::Checkmate) {
        const bool whiteWon = s.sideToMove == 'b';
        score = whiteWon ? "White wins" : "Black wins";
        words = "By checkmate";
        target = whiteWon ? 1.0f : 0.0f;
    }
    else if (facts.state == GameState::Stalemate) {
        score = "Draw";
        words = "By stalemate";
    }
    else if (haveEval) {
        score = FormatScore(isMate, mateIn, cp, 2);
        words = DescribeScore(isMate, mateIn, cp);
        if (isMate) {
            target = mateIn > 0 ? 1.0f : 0.0f;
        }
        else {
            // Centipawns are unbounded, so a linear bar would sit pinned at one
            // end for most of a game. The usual logistic mapping from score to
            // expected result keeps the interesting range legible.
            const float chances = 2.0f / (1.0f + std::exp(-0.004f * (float)cp)) - 1.0f;
            target = std::clamp(0.5f + 0.5f * chances, 0.02f, 0.98f);
        }
    }
    else {
        score = "0.00";
        words = searching ? "Thinking" : "No evaluation yet";
    }

    // The bar chases its target rather than being set to it, so a search that
    // lands depth by depth reads as one slide instead of a stack of jumps.
    static float shownShare = 0.5f;
    const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    shownShare += (target - shownShare) * (1.0f - std::exp(-deltaTime / 0.20f));
    if (std::fabs(target - shownShare) < 0.0015f) shownShare = target;

    // The score on the left, what it means on the right.
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float displaySize = Px(kSizeDisplay);
    const float lineHeight = displaySize * 1.10f;

    ImDrawList* list = ImGui::GetWindowDrawList();
    const float scoreWidth = fonts.strong->CalcTextSizeA(displaySize, FLT_MAX, 0.0f, score.c_str()).x;
    ui::DrawCenteredText(list, fonts.strong, displaySize,
        ImVec2(origin.x + scoreWidth * 0.5f, origin.y + lineHeight * 0.5f),
        haveEval || facts.state != GameState::Playing ? kText : kTextFaint, score.c_str());

    const float small = Px(kSizeSmall);
    const float wordsWidth = fonts.strong->CalcTextSizeA(small, FLT_MAX, 0.0f, words.c_str()).x;
    ui::DrawCenteredText(list, fonts.strong, small,
        ImVec2(origin.x + width - wordsWidth * 0.5f, origin.y + lineHeight * 0.5f - small * 0.62f),
        kTextDim, words.c_str());

    if (facts.state == GameState::Playing && haveEval) {
        char depthText[32];
        std::snprintf(depthText, sizeof(depthText), "Depth %d", depth);
        const float depthWidth = fonts.body->CalcTextSizeA(small, FLT_MAX, 0.0f, depthText).x;
        const ImVec2 depthCenter(origin.x + width - depthWidth * 0.5f, origin.y + lineHeight * 0.5f + small * 0.72f);
        ui::DrawCenteredText(list, fonts.body, small, depthCenter, kTextFaint, depthText);
        if (searching) {
            const ImVec2 center(depthCenter.x - depthWidth * 0.5f - Px(10.0f), depthCenter.y);
            const float start = (float)ImGui::GetTime() * 6.0f;
            list->PathArcTo(center, Px(4.5f), start, start + IM_PI * 1.4f, 16);
            list->PathStroke(kAccentText, ImDrawFlags_None, Px(1.6f));
        }
    }

    // The bar itself, White's share from the left.
    const float barTop = origin.y + lineHeight + Px(8.0f);
    const float barHeight = Px(8.0f);
    const ImVec2 barMin(origin.x, barTop);
    const ImVec2 barMax(origin.x + width, barTop + barHeight);
    const float split = origin.x + width * shownShare;
    list->AddRectFilled(barMin, barMax, kBlackSide, barHeight * 0.5f);
    if (split > barMin.x + 0.5f) {
        list->AddRectFilled(barMin, ImVec2(split, barMax.y), kWhiteSide, barHeight * 0.5f,
                            shownShare >= 0.995f ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft);
    }
    list->AddRect(barMin, barMax, kLineStrong, barHeight * 0.5f);
    const float middle = std::round(origin.x + width * 0.5f);
    list->AddLine(ImVec2(middle, barMin.y - Px(3.0f)), ImVec2(middle, barMax.y + Px(3.0f)), kTextFaint, 1.0f);

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, barTop + barHeight - origin.y));
}

// A pill with a short piece of text, tinted with a colour.
void Pill(const char* text, ImU32 tone, bool emphasised) {
    const Fonts& fonts = GetFonts();
    const float size = Px(kSizeSmall);
    const float textWidth = fonts.strong->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent(textWidth + Px(18.0f), Px(22.0f));

    ImDrawList* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(origin, ImVec2(origin.x + extent.x, origin.y + extent.y),
                        emphasised ? Mix(kBg, tone, 0.24f) : kSurface, extent.y * 0.5f);
    if (!emphasised) {
        list->AddRect(origin, ImVec2(origin.x + extent.x, origin.y + extent.y), kLine, extent.y * 0.5f);
    }
    ui::DrawCenteredText(list, fonts.strong, size, ImVec2(origin.x + extent.x * 0.5f, origin.y + extent.y * 0.5f),
                         emphasised ? kAccentText : kTextDim, text);
    ImGui::Dummy(extent);
}

float PillWidth(const char* text) {
    return GetFonts().strong->CalcTextSizeA(Px(kSizeSmall), FLT_MAX, 0.0f, text).x + Px(18.0f);
}

// A small board showing the tracked position: what Oracle believes is on the
// board, which is the quickest way to spot a square it is misreading.
void DrawMiniBoard(const Snapshot& s, const PositionFacts& facts, float size, const StockfishMove* best) {
    const Fonts& fonts = GetFonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));

    const float cell = size / 8.0f;
    // Grey rather than the panel's near black: pieces of both colours need a
    // square they stand out against, and a site's own colours would compete
    // with the real board beside it.
    const ImU32 light = IM_COL32(139, 146, 161, 255);
    const ImU32 dark = IM_COL32(101, 109, 125, 255);

    // FEN index to where it is drawn, respecting which way round the board is.
    auto display = [&](int fenIndex) {
        int row = fenIndex / 8;
        int col = fenIndex % 8;
        if (g_orientation == 1) {
            row = 7 - row;
            col = 7 - col;
        }
        return ImVec2(origin.x + col * cell, origin.y + row * cell);
    };
    auto fenIndexOf = [](char file, char rank) {
        return ('8' - rank) * 8 + (file - 'a');
    };

    int lastFrom = -1, lastTo = -1;
    if (s.lastMoveUci.size() >= 4) {
        lastFrom = fenIndexOf(s.lastMoveUci[0], s.lastMoveUci[1]);
        lastTo = fenIndexOf(s.lastMoveUci[2], s.lastMoveUci[3]);
    }

    for (int square = 0; square < 64; ++square) {
        const ImVec2 at = display(square);
        const bool isLight = ((square / 8) + (square % 8)) % 2 == 0;
        ImU32 fill = isLight ? light : dark;
        if (square == lastFrom || square == lastTo) fill = Mix(fill, kAccent, 0.38f);
        if (square == facts.checkedKing) fill = Mix(fill, kBad, 0.55f);
        list->AddRectFilled(at, ImVec2(at.x + cell, at.y + cell), fill);
    }

    // Pieces: the solid glyph is the body and the hollow one drawn over it the
    // outline, so both colours read on both shades of square.
    // Segoe UI Symbol draws its chess pieces well inside the em box, so the
    // size asked for is larger than the square to get a piece that fills it.
    const float glyphSize = cell * 1.28f;
    static const char* kKinds = "kqrbnp";
    for (int row = 0; row < 8 && row < (int)s.rows.size(); ++row) {
        for (int col = 0; col < 8 && col < (int)s.rows[row].size(); ++col) {
            const char piece = s.rows[row][col];
            if (piece == ' ') continue;
            const char* kind = std::strchr(kKinds, std::tolower((unsigned char)piece));
            if (!kind || !*kind) continue;
            const int offset = (int)(kind - kKinds);

            char body[5] = {}, outline[5] = {};
            ImTextCharToUtf8(body, 0x265A + offset);
            ImTextCharToUtf8(outline, 0x2654 + offset);
            const bool white = std::isupper((unsigned char)piece) != 0;

            const ImVec2 center(origin.x + (col + 0.5f) * cell, origin.y + (row + 0.5f) * cell + cell * 0.03f);
            ui::DrawCenteredText(list, fonts.pieces, glyphSize, center,
                                 white ? IM_COL32(246, 247, 249, 255) : IM_COL32(17, 19, 23, 255), body);
            ui::DrawCenteredText(list, fonts.pieces, glyphSize, center,
                                 white ? IM_COL32(17, 19, 23, 255) : IM_COL32(17, 19, 23, 255), outline);
        }
    }

    // The engine's first choice in the overlay's own language: a near black
    // line from the piece, ending on the rank badge where it lands.
    if (best && best->uci.size() >= 4) {
        const std::string& uci = best->uci;
        const ImVec2 from = display(fenIndexOf(uci[0], uci[1]));
        const ImVec2 to = display(fenIndexOf(uci[2], uci[3]));
        const ImVec2 a(from.x + cell * 0.5f, from.y + cell * 0.5f);
        const ImVec2 b(to.x + cell * 0.5f, to.y + cell * 0.5f);
        float dx = b.x - a.x, dy = b.y - a.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        const float disc = cell * 0.30f;
        if (length > disc) {
            dx /= length;
            dy /= length;
            const ImU32 ink = IM_COL32(12, 12, 14, 255);
            const float thickness = std::max(2.0f, cell * 0.12f);
            list->AddCircleFilled(a, thickness * 0.5f, ink, 12);
            list->AddLine(a, ImVec2(b.x - dx * disc, b.y - dy * disc), ink, thickness);
        }
        ui::DrawRankDisc(list, b, disc, EvaluationColor(best->mate, best->mateIn, best->scoreCp), false, "1");
    }

    list->AddRect(ImVec2(origin.x - 1.0f, origin.y - 1.0f), ImVec2(origin.x + size + 1.0f, origin.y + size + 1.0f),
                  kLineStrong, Px(2.0f), 0, 1.0f);
}

void DrawMoveList(const Snapshot& s, const PositionFacts& facts, bool engineAlive, float width, float height) {
    const Fonts& fonts = GetFonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, height));

    const float small = Px(kSizeSmall);
    const float body = Px(kSizeBody);
    auto caption = [&](const char* text, ImU32 color) {
        list->AddText(fonts.strong, small, origin, color, text);
    };

    if (facts.state != GameState::Playing) {
        // The same words the callout on the board uses, then what happens next.
        const bool mate = facts.state == GameState::Checkmate;
        const bool whiteWon = s.sideToMove == 'b';
        list->AddText(fonts.body, Px(20.0f), ImVec2(origin.x, origin.y + Px(2.0f)), kTextDim, ICON_FLAG);

        const char* headline = mate ? "Checkmate," : "Stalemate,";
        const float headlineWidth = fonts.strong->CalcTextSizeA(body, FLT_MAX, 0.0f, headline).x;
        list->AddText(fonts.strong, body, ImVec2(origin.x, origin.y + Px(34.0f)), kText, headline);
        list->AddText(fonts.body, body, ImVec2(origin.x + headlineWidth + Px(4.0f), origin.y + Px(34.0f)), kTextDim,
                      ResultForPlayer(mate, whiteWon, g_sfPlayWhite));
        list->AddText(fonts.body, small, ImVec2(origin.x, origin.y + Px(60.0f)), kTextFaint,
                      "A new game with the same colour is picked up on its own.", nullptr, width);
        return;
    }

    if (!engineAlive) {
        caption("No engine", kTextDim);
        const float wrap = width;
        list->AddText(fonts.body, small, ImVec2(origin.x, origin.y + Px(22.0f)), kTextFaint,
                      "Load one in the Engine tab to see moves here.", nullptr, wrap);
        return;
    }

    const float rowHeight = Px(28.0f);
    const float listTop = origin.y + Px(26.0f);
    const float discRadius = Px(9.5f);
    caption(s.movesAreOurs ? "Your best moves" : "Their best moves", kTextFaint);

    if (!s.movesCurrent || s.moves.empty()) {
        // Where the moves will land, rather than an empty space.
        for (int i = 0; i < 3; ++i) {
            const float y = listTop + rowHeight * i + rowHeight * 0.5f;
            list->AddCircleFilled(ImVec2(origin.x + discRadius, y), discRadius, kSurface, 20);
            list->AddCircle(ImVec2(origin.x + discRadius, y), discRadius - 0.5f, kLine, 20, 1.0f);
            list->AddRectFilled(ImVec2(origin.x + discRadius * 2.0f + Px(10.0f), y - Px(4.0f)),
                                ImVec2(origin.x + discRadius * 2.0f + Px(10.0f) + Px(46.0f - 9.0f * i), y + Px(4.0f)),
                                kSurfaceHover, Px(4.0f));
        }
        list->AddText(fonts.body, small, ImVec2(origin.x, listTop + rowHeight * 3.0f + Px(6.0f)), kTextFaint,
                      "Searching\xE2\x80\xA6");
        return;
    }

    const bool whiteToMove = s.sideToMove == 'w';
    const int fits = std::max(1, (int)((height - (listTop - origin.y)) / rowHeight));
    const int shown = std::min((int)s.moves.size(), fits);
    for (int i = 0; i < shown; ++i) {
        const StockfishMove& move = s.moves[i];
        const float y = listTop + rowHeight * i + rowHeight * 0.5f;

        // Our moves wear the colours the badges on the board do. Theirs are
        // neutral: green for a move that is good for them would read as good
        // for us.
        char rank[4];
        std::snprintf(rank, sizeof(rank), "%d", i + 1);
        const ImU32 fill = s.movesAreOurs ? EvaluationColor(move.mate, move.mateIn, move.scoreCp) : kSurfaceActive;
        ui::DrawRankDisc(list, ImVec2(origin.x + discRadius, y), discRadius, fill, false, rank);

        const std::string& san = move.san.empty() ? move.uci : move.san;
        const float sanWidth = fonts.strong->CalcTextSizeA(body, FLT_MAX, 0.0f, san.c_str()).x;
        ui::DrawCenteredText(list, fonts.strong, body,
            ImVec2(origin.x + discRadius * 2.0f + Px(10.0f) + sanWidth * 0.5f, y),
            i == 0 ? kText : Mix(kText, kTextDim, 0.45f), san.c_str());

        // From White's side, like the evaluation above it.
        const int cpWhite = whiteToMove ? move.scoreCp : -move.scoreCp;
        const int mateWhite = whiteToMove ? move.mateIn : -move.mateIn;
        const std::string score = FormatScore(move.mate, mateWhite, cpWhite, 2);
        const float scoreWidth = fonts.body->CalcTextSizeA(small, FLT_MAX, 0.0f, score.c_str()).x;
        ui::DrawCenteredText(list, fonts.body, small, ImVec2(origin.x + width - scoreWidth * 0.5f, y),
                             kTextDim, score.c_str());
    }
}

void DrawGame(const Snapshot& s, bool engineAlive, bool searching) {
    const Fonts& fonts = GetFonts();
    const PositionFacts facts = FactsFor(s.trackedFen);
    const bool over = facts.state != GameState::Playing;

    DrawEvaluation(s, facts, searching);
    ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

    // Whose move it is, in words and as a pill.
    {
        char line[64];
        std::snprintf(line, sizeof(line), "Move %d \xC2\xB7 %s to move", facts.fullmove,
                      s.sideToMove == 'w' ? "White" : "Black");
        ImGui::PushFont(nullptr, kSizeSmall);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(3.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted(line);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        const char* pill = over ? "Game over" : (s.movesAreOurs ? "Your move" : "Their move");
        AlignRight(PillWidth(pill));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(3.0f));
        Pill(pill, kAccent, s.movesAreOurs && !over);
    }

    // The last move, and what it cost if it cost something, marked with the
    // same chip the callout on the board uses.
    if (!s.lastMoveSan.empty()) {
        const bool lastByUs = (s.sideToMove == 'b') == g_sfPlayWhite;
        const bool judged = !s.verdict.empty() && s.verdictSan == s.lastMoveSan;

        ImGui::PushFont(nullptr, kSizeSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
        ImGui::TextUnformatted(lastByUs ? "You played" : "They played");
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, Px(6.0f));
        ImGui::PushFont(fonts.strong, 0.0f);
        ImGui::TextUnformatted(NumberedMove(facts.fullmove, s.sideToMove, s.lastMoveSan).c_str());
        ImGui::PopFont();

        if (judged) {
            const float size = ImGui::GetFontSize();
            const float chipHeight = std::round(size * 1.45f);
            ImGui::SameLine(0.0f, Px(8.0f));
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float lineMiddle = at.y + ImGui::GetTextLineHeight() * 0.5f;
            const float chipWidth = ui::VerdictChipWidth(s.verdict.c_str(), size);
            ui::DrawVerdictChip(ImGui::GetWindowDrawList(), ImVec2(at.x, std::round(lineMiddle - chipHeight * 0.5f)),
                                chipHeight, size, s.verdict.c_str());
            ImGui::Dummy(ImVec2(chipWidth, ImGui::GetTextLineHeight()));
            ImGui::SameLine(0.0f, Px(6.0f));
            ImGui::PushFont(fonts.strong, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, Mix(VerdictColor(s.verdict.c_str()), kText, 0.25f));
            ImGui::TextUnformatted(s.verdict.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // What it cost, unsigned, and what was better.
            char detail[128];
            std::snprintf(detail, sizeof(detail), "Lost %.1f.%s%s%s", s.verdictLoss / 100.0f,
                s.verdictBestSan.empty() ? "" : " ", s.verdictBestSan.c_str(),
                s.verdictBestSan.empty() ? "" : " was best.");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            ImGui::TextUnformatted(detail);
            ImGui::PopStyleColor();
        }
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

    // The board as Oracle sees it, beside the moves it would play.
    {
        const float boardSize = Px(168.0f);
        const float gap = Px(16.0f);
        const float listWidth = ImGui::GetContentRegionAvail().x - boardSize - gap;
        const bool showBest = s.movesCurrent && s.movesAreOurs && s.inSync && !s.moves.empty() && !over;
        DrawMiniBoard(s, facts, boardSize, showBest ? &s.moves.front() : nullptr);
        ImGui::SameLine(0.0f, gap);
        DrawMoveList(s, facts, engineAlive, listWidth, boardSize);
    }

    if (!s.inSync) {
        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        if (ui::Banner("lost", ICON_WARNING, kWarn,
                       "The board no longer matches the game being followed.", "Rescan")) {
            RescanBoard();
        }
    }

    // Which side Oracle suggests moves for, and starting over.
    ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
    {
        const ImVec2 rowStart = ImGui::GetCursorPos();
        const float rowHeight = Px(28.0f);

        ImGui::PushFont(nullptr, kSizeSmall);
        ImGui::SetCursorPosY(rowStart.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted("You play");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SameLine(0.0f, Px(10.0f));
        ImGui::SetCursorPosY(rowStart.y);
        static const char* kSides[] = { "White", "Black" };
        int side = g_sfPlayWhite ? 0 : 1;
        if (ui::Segmented("side", &side, kSides, 2, Px(148.0f))) g_sfPlayWhite = side == 0;

        // Not while the banner above offers the same thing.
        if (s.inSync) {
            const char* rescan = ICON_REFRESH "  Rescan";
            const float rescanWidth = ImGui::CalcTextSize(rescan).x + Px(22.0f);
            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - rescanWidth, rowStart.y));
            if (ui::Button(rescan, ui::ButtonKind::Ghost, ImVec2(rescanWidth, rowHeight))) RescanBoard();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("Forget this board and detect one again");
            }
        }
    }
}

void DrawEngineTab(const EngineStatus& engine, bool engineAlive) {
    const Fonts& fonts = GetFonts();

    // Any UCI engine will do. Empty means the bundled Stockfish.
    static char enginePath[512] = "";
    static bool enginePathLoaded = false;
    if (!enginePathLoaded) {
        std::snprintf(enginePath, sizeof(enginePath), "%s", engine.requested.c_str());
        enginePathLoaded = true;
    }

    ui::BeginCard("engine");
    {
        ImGui::PushFont(fonts.strong, 0.0f);
        ImGui::TextUnformatted(engine.loading ? "Starting\xE2\x80\xA6"
                               : engine.name.empty() ? "No engine" : engine.name.c_str());
        ImGui::PopFont();

        const char* state = engine.loading ? "Starting" : engineAlive ? "Ready" : "Not running";
        const ImU32 tone = engine.loading ? kWarn : engineAlive ? kGood : kBad;
        ImGui::PushFont(nullptr, kSizeSmall);
        AlignRight(ImGui::CalcTextSize(state).x + Px(15.0f));
        ui::StatusDot(tone, engine.loading);
        ImGui::SameLine(0.0f, Px(7.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted(state);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (!engine.resolved.empty()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(3.0f));
            ImGui::PushFont(fonts.mono, kSizeSmall - 1.0f);
            const std::string shown = EllipsizeStart(engine.resolved, ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
            ImGui::TextUnformatted(shown.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (shown != engine.resolved && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", engine.resolved.c_str());
            }
        }

        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        const float loadWidth = ImGui::CalcTextSize("Load").x + Px(30.0f);
        const float browseWidth = Px(28.0f);
        const float fieldHeight = Px(28.0f);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - loadWidth - browseWidth - Px(12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Px(9.0f), (fieldHeight - Px(kSizeSmall) * 1.33f) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, kBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kBg);
        ImGui::PushStyleColor(ImGuiCol_Border, kLineStrong);
        ImGui::PushFont(fonts.mono, kSizeSmall);
        ImGui::InputTextWithHint("##enginePath", "Bundled Stockfish", enginePath, sizeof(enginePath));
        ImGui::PopFont();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        ImGui::SameLine(0.0f, Px(6.0f));
        if (ui::IconButton("browse", ICON_FOLDER, "Choose an engine file")) {
            if (BrowseForEngine(enginePath, sizeof(enginePath))) LaunchStockfishAsync(enginePath);
        }
        ImGui::SameLine(0.0f, Px(6.0f));
        ImGui::BeginDisabled(engine.loading);
        if (ui::Button("Load", ui::ButtonKind::Primary, ImVec2(loadWidth, fieldHeight))) {
            LaunchStockfishAsync(enginePath);
        }
        ImGui::EndDisabled();

        if (!engine.error.empty()) {
            ImGui::PushFont(nullptr, kSizeSmall);
            ui::WrappedText(kBad, engine.error.c_str());
            ImGui::PopFont();
        }
        ui::Caption("Any engine that speaks UCI. Leave the path empty for the bundled Stockfish.");
    }
    ui::EndCard();

    ui::SectionLabel("Search");
    ui::Slider("Depth", &g_sfMoveDepth, 1, 30);
    ui::Slider("Moves shown", &g_sfNumberMoves, 1, 10);

    ui::SectionLabel("Strength");
    ui::Toggle("Limit strength", &g_sfLimitStrength);
    ImGui::BeginDisabled(!g_sfLimitStrength);
    ui::Slider("Elo", &g_sfElo, 1320, 3190);
    ImGui::EndDisabled();

    ui::SectionLabel("Board");
    ui::Toggle("Draw move arrows", &g_showMoveArrows);
}

void Swatch(const char* label, const cv::Vec3b& bgr, int luminance) {
    const Fonts& fonts = GetFonts();
    const float side = Px(30.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* list = ImGui::GetWindowDrawList();

    // The colour as it was read, even inside a disabled block: dimmed towards
    // this panel, a board's green or a wooden square turns to mud.
    const bool known = luminance >= 0;
    const ImU32 fill = known ? IM_COL32(bgr[2], bgr[1], bgr[0], 255) : kSurface;
    list->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), fill, Px(6.0f));
    list->AddRect(origin, ImVec2(origin.x + side, origin.y + side), kLineStrong, Px(6.0f));

    ImGui::Dummy(ImVec2(side, side));
    ImGui::SameLine(0.0f, Px(8.0f));
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, kSizeSmall);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(7.0f));
    char value[16];
    if (known) std::snprintf(value, sizeof(value), "%d", luminance);
    else std::snprintf(value, sizeof(value), "not read");
    ImGui::PushFont(fonts.mono, kSizeSmall - 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::EndGroup();
}

// A collapsible section header in the panel's own style.
bool Disclosure(const char* label) {
    const Fonts& fonts = GetFonts();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetID(label);
    bool open = storage->GetBool(id, false);

    const float width = ImGui::GetContentRegionAvail().x;
    const float height = Px(32.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    if (ImGui::InvisibleButton("##disclosure", ImVec2(width, height))) {
        open = !open;
        storage->SetBool(id, open);
    }
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    ImDrawList* list = ImGui::GetWindowDrawList();
    if (hovered) list->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), kSurface, Px(6.0f));
    list->AddLine(ImVec2(origin.x, origin.y), ImVec2(origin.x + width, origin.y), kLine, 1.0f);

    const float turn = ui::Animate(ImGui::GetID((std::string(label) + "##turn").c_str()), open ? 1.0f : 0.0f, 0.06f);
    const float size = Px(kSizeBody);
    ui::DrawCenteredText(list, fonts.strong, size,
        ImVec2(origin.x + Px(4.0f) + fonts.strong->CalcTextSizeA(size, FLT_MAX, 0.0f, label).x * 0.5f, origin.y + height * 0.5f),
        hovered || open ? kText : kTextDim, label);
    ui::DrawCenteredText(list, fonts.body, Px(10.0f), ImVec2(origin.x + width - Px(12.0f), origin.y + height * 0.5f),
                         kTextFaint, turn > 0.5f ? ICON_CHEVRON_DOWN : ICON_CHEVRON_RIGHT);
    return open;
}

void DrawCalibrateTab() {
    const Fonts& fonts = GetFonts();

    /* THE BOARD. */
    ui::BeginCard("board");
    {
        const bool haveBoard = (g_boardRect.right - g_boardRect.left) > 0;
        const char* rescan = ICON_REFRESH "  Rescan";
        const float rescanWidth = haveBoard ? ImGui::CalcTextSize(rescan).x + Px(22.0f) : 0.0f;
        const float top = ImGui::GetCursorPosY();

        ImGui::PushFont(fonts.strong, 0.0f);
        ImGui::TextUnformatted(haveBoard ? "Board found" : "No board yet");
        ImGui::PopFont();

        char detail[220];
        if (haveBoard) {
            std::snprintf(detail, sizeof(detail), "%d \xC3\x97 %d px at %d, %d, %s at the bottom.",
                (int)(g_boardRect.right - g_boardRect.left), (int)(g_boardRect.bottom - g_boardRect.top),
                (int)g_boardRect.left, (int)g_boardRect.top, g_orientation == 1 ? "Black" : "White");
        }
        else {
            std::snprintf(detail, sizeof(detail), "%s",
                g_detectStatus.empty() ? "Detect one from the Play tab, or by hand below." : g_detectStatus.c_str());
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(3.0f));
        ImGui::PushFont(nullptr, kSizeSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - rescanWidth - Px(10.0f));
        ImGui::TextUnformatted(detail);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (haveBoard) {
            const float bottom = ImGui::GetCursorPosY();
            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - rescanWidth,
                                       top + (bottom - top - Px(28.0f)) * 0.5f - Px(3.0f)));
            if (ui::Button(rescan, ui::ButtonKind::Secondary, ImVec2(rescanWidth, Px(28.0f)))) RescanBoard();
            ImGui::SetCursorPosY(bottom);
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
    }
    ui::EndCard();

    /* MATCHING. */
    ui::SectionLabel("Matching");
    ui::Slider("Match threshold", &g_matchThreshold, 0.10f, 1.0f, "%.2f");
    ui::Slider("Tolerance", &g_analysisTolerance, 1, 64);

    /* HIGHLIGHTS. */
    ui::SectionLabel("Highlight colours");
    {
        std::vector<cv::Vec3b> colors;
        {
            std::lock_guard<std::mutex> lock(g_highlightMutex);
            colors = g_highlightColors;
        }

        if (colors.empty()) {
            ui::Caption("None yet. Oracle learns the colours a site paints over the last move from the moves it sees.");
        }
        else {
            ImDrawList* list = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float side = Px(24.0f);
            float x = origin.x;
            for (const cv::Vec3b& color : colors) {
                list->AddRectFilled(ImVec2(x, origin.y), ImVec2(x + side, origin.y + side),
                                    IM_COL32(color[2], color[1], color[0], 255), Px(5.0f));
                list->AddRect(ImVec2(x, origin.y), ImVec2(x + side, origin.y + side), kLineStrong, Px(5.0f));
                x += side + Px(6.0f);
            }
            ImGui::Dummy(ImVec2(x - origin.x, side));

            char learned[48];
            std::snprintf(learned, sizeof(learned), "%d learned", (int)colors.size());
            ImGui::SameLine(0.0f, Px(4.0f));
            ImGui::PushFont(nullptr, kSizeSmall);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (side - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            ImGui::TextUnformatted(learned);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            const float forgetWidth = ImGui::CalcTextSize("Forget").x + Px(22.0f);
            AlignRight(forgetWidth);
            ImGui::SetCursorPosY(origin.y - ImGui::GetWindowPos().y + ImGui::GetScrollY() + (side - Px(26.0f)) * 0.5f);
            if (ui::Button("Forget", ui::ButtonKind::Ghost, ImVec2(forgetWidth, Px(26.0f)))) {
                std::lock_guard<std::mutex> lock(g_highlightMutex);
                g_highlightColors.clear();
            }
        }
    }

    /* REFERENCE COLOURS. */
    ui::SectionLabel("Reference colours");
    ImGui::BeginDisabled(!g_samplePointsSet || !g_cropRegionSet);
    if (ImGui::BeginTable("references", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        Swatch("Light squares", g_refBoardColor1Color, g_refBoardColor1);
        ImGui::TableSetColumnIndex(1);
        Swatch("Dark squares", g_refBoardColor2Color, g_refBoardColor2);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        Swatch("White pieces", g_refWhitePieceColor, g_refWhitePiece);
        ImGui::TableSetColumnIndex(1);
        Swatch("Black pieces", g_refBlackPieceColor, g_refBlackPiece);
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));

    /* MANUAL CALIBRATION, kept for boards automatic setup cannot read. */
    if (Disclosure("Calibrate by hand")) {
        if (!g_userScreenshotReady) {
            ui::Caption("First freeze the screen as it is, so the clicks land on a still image.");
            if (ui::Button("Take screenshot", ui::ButtonKind::Secondary)) {
                g_userScreenshotColor = CaptureVirtualScreen();
                if (!g_userScreenshotColor.empty()) {
                    cv::cvtColor(g_userScreenshotColor, g_userScreenshotGray, cv::COLOR_BGR2GRAY);
                    g_userScreenshotReady = true;
                    g_clickStage = 0;
                    g_viewFirstClick = { -1, -1, 0 };
                    g_viewSecondClick = { -1, -1, 0 };
                }
            }
        }

        if (!g_boardClicksReady && g_userScreenshotReady) {
            ui::Caption("Hold Ctrl and click the top left square of the board, then the square beside it.");

            const char* stage = g_clickStage == 0 ? "Waiting for the first click"
                              : g_clickStage == 1 ? "Waiting for the second click"
                              : "Both squares clicked";
            ImGui::PushFont(nullptr, kSizeSmall);
            ui::StatusDot(g_clickStage == 2 ? kGood : kAccentText, g_clickStage < 2);
            ImGui::SameLine(0.0f, Px(7.0f));
            ImGui::TextUnformatted(stage);
            if (g_clickStage >= 1) {
                ImGui::SameLine();
                ImGui::PushFont(fonts.mono, kSizeSmall - 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
                if (g_clickStage == 1) ImGui::Text("%d, %d", g_viewFirstClick.x, g_viewFirstClick.y);
                else ImGui::Text("%d, %d and %d, %d", g_viewFirstClick.x, g_viewFirstClick.y,
                                 g_viewSecondClick.x, g_viewSecondClick.y);
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            ImGui::PopFont();

            if (ui::Button("Clear clicks", ui::ButtonKind::Ghost)) {
                g_clickStage = 0;
                g_viewFirstClick = { -1, -1, 0 };
                g_viewSecondClick = { -1, -1, 0 };
                g_clicks = { g_viewFirstClick, g_viewSecondClick };
            }
            if (g_clickStage == 2) {
                ImGui::SameLine();
                if (ui::Button("Find board from clicks", ui::ButtonKind::Primary)) {
                    g_boardClicksReady = true;
                    g_clicks = { g_viewFirstClick, g_viewSecondClick };
                    DetectBoardDimensions();
                    g_isRescanning = false;
                    g_isConfiguringSamplePoints = true;
                    g_samplePointsSet = false;
                    g_refBoardColor1 = (int)g_clicks.first.grayscaleValue;
                    g_refBoardColor2 = (int)g_clicks.second.grayscaleValue;

                    if (!g_userScreenshotColor.empty()) {
                        if (g_viewFirstClick.y >= 0 && g_viewFirstClick.y < g_userScreenshotColor.rows &&
                            g_viewFirstClick.x >= 0 && g_viewFirstClick.x < g_userScreenshotColor.cols) {
                            g_refBoardColor1Color = g_userScreenshotColor.at<cv::Vec3b>(g_viewFirstClick.y, g_viewFirstClick.x);
                        }
                        if (g_viewSecondClick.y >= 0 && g_viewSecondClick.y < g_userScreenshotColor.rows &&
                            g_viewSecondClick.x >= 0 && g_viewSecondClick.x < g_userScreenshotColor.cols) {
                            g_refBoardColor2Color = g_userScreenshotColor.at<cv::Vec3b>(g_viewSecondClick.y, g_viewSecondClick.x);
                        }
                    }
                }
            }
        }

        if (g_boardClicksReady) {
            ui::Caption("The board is set. Rescan to start again.");
        }
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
    }

    /* SAMPLING GEOMETRY. Derived from the detected cell size; exposed for tuning. */
    if (Disclosure("Sampling geometry")) {
        // TODO: Implement debug sample grouping, for example: In lichess, one debug sample is not enough to get all of the pieces.
        // Certain pieces have different colors in certain positions.
        // By using more than one debug sample groups, if tghe spot at the debug sample does not equal nor white nor black reference values:
        // - We move to the next debug sample for that cell, doing so until we either find a match for black or white reference values.
        ImGui::BeginDisabled(g_samplePointsSet || g_userScreenshotGray.empty() || !g_boardClicksReady);
        ui::Caption("Where each square is sampled for the colour of its piece.");

        static int prevPatchSize = g_debugPatchSize;
        static int prevOffsetX = g_debugOffsetX;
        static int prevOffsetY = g_debugOffsetY;
        static int prevOffsetX2 = g_debugOffsetX2;
        static int prevOffsetY2 = g_debugOffsetY2;
        static int prevOffsetX3 = g_debugOffsetX3;
        static int prevOffsetY3 = g_debugOffsetY3;

        ui::Slider("Patch size", &g_debugPatchSize, 1, 64);
        ui::Slider("Point 1 across", &g_debugOffsetX, -64, 64);
        ui::Slider("Point 1 down", &g_debugOffsetY, -64, 64);
        ui::Slider("Point 2 across", &g_debugOffsetX2, -64, 64);
        ui::Slider("Point 2 down", &g_debugOffsetY2, -64, 64);
        ui::Slider("Point 3 across", &g_debugOffsetX3, -64, 64);
        ui::Slider("Point 3 down", &g_debugOffsetY3, -64, 64);
        if (ui::Button("Set sample points", ui::ButtonKind::Secondary)) {
            UpdateDebugSamples();
            DetectPieceColorCoding((g_boardRect.right - g_boardRect.left) / 8, (g_boardRect.bottom - g_boardRect.top) / 8);
            g_samplePointsSet = true;
            g_isConfiguringCropRegion = true;
            UpdateCropRects();
        }

        if (!g_samplePointsSet && (prevPatchSize != g_debugPatchSize || prevOffsetX != g_debugOffsetX || prevOffsetY != g_debugOffsetY ||
                                   prevOffsetX2 != g_debugOffsetX2 || prevOffsetY2 != g_debugOffsetY2 ||
                                   prevOffsetX3 != g_debugOffsetX3 || prevOffsetY3 != g_debugOffsetY3)) {
            UpdateDebugSamples();
            prevPatchSize = g_debugPatchSize;
            prevOffsetX = g_debugOffsetX;
            prevOffsetY = g_debugOffsetY;
            prevOffsetX2 = g_debugOffsetX2;
            prevOffsetY2 = g_debugOffsetY2;
            prevOffsetX3 = g_debugOffsetX3;
            prevOffsetY3 = g_debugOffsetY3;
        }
        ImGui::EndDisabled();

        /* CROP REGION. */
        static int prevCropPatch = g_cropPatchSize;
        static int prevCropOffX = g_cropOffsetX;
        static int prevCropOffY = g_cropOffsetY;

        ui::SectionLabel("Crop region");
        ImGui::BeginDisabled(!g_samplePointsSet || g_cropRegionSet);

        int maxCropSize = 64;
        if ((g_boardRect.right - g_boardRect.left) > 0) {
            maxCropSize = (g_boardRect.right - g_boardRect.left) / 8;
        }
        if (g_cropPatchSize > maxCropSize) {
            g_cropPatchSize = maxCropSize;
        }

        ui::Slider("Crop size", &g_cropPatchSize, 1, maxCropSize);
        ui::Slider("Crop across", &g_cropOffsetX, -64, 64);
        ui::Slider("Crop down", &g_cropOffsetY, -64, 64);

        if (!g_cropRegionSet && g_isConfiguringCropRegion && (prevCropPatch != g_cropPatchSize || prevCropOffX != g_cropOffsetX || prevCropOffY != g_cropOffsetY)) {
            UpdateCropRects();
            prevCropPatch = g_cropPatchSize;
            prevCropOffX = g_cropOffsetX;
            prevCropOffY = g_cropOffsetY;
        }

        if (ui::Button("Set crop region", ui::ButtonKind::Secondary)) {
            UpdateCropRects();
            g_cropRegionSet = true;
            g_isConfiguringCropRegion = false;
            int cellW = (g_boardRect.right - g_boardRect.left) / 8;
            int cellH = (g_boardRect.bottom - g_boardRect.top) / 8;
            cv::Mat srcColor = g_userScreenshotReady && !g_userScreenshotColor.empty() ? g_userScreenshotColor : CaptureVirtualScreen();
            GenerateReferencePieceCrops(srcColor, cellW, cellH);
            g_hasAnalysisStarted = true;
        }
        ImGui::EndDisabled();
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
    }

    /* CURRENT MODE. */
    {
        const ImVec2 lineStart = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(lineStart,
            ImVec2(lineStart.x + ImGui::GetContentRegionAvail().x, lineStart.y), kLine, 1.0f);
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));

        const bool configuring = g_isConfiguringSamplePoints || g_isConfiguringCropRegion;
        char mode[64];
        if (configuring) std::snprintf(mode, sizeof(mode), "Configuring, %d sample points", (int)g_debugSamples.size());
        else std::snprintf(mode, sizeof(mode), "%s", g_hasAnalysisStarted ? "Analysis running" : "Idle");

        const float rowTop = ImGui::GetCursorPosY();
        const float rowHeight = Px(28.0f);
        ImGui::PushFont(nullptr, kSizeSmall);
        ImGui::SetCursorPosY(rowTop + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
        ui::StatusDot(configuring ? kAccentText : (g_hasAnalysisStarted ? kGood : kTextFaint), false);
        ImGui::SameLine(0.0f, Px(7.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted(mode);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (!configuring) {
            const char* reset = "Reset configuration";
            const float resetWidth = ImGui::CalcTextSize(reset).x + Px(22.0f);
            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - resetWidth, rowTop));
            if (ui::Button(reset, ui::ButtonKind::Ghost, ImVec2(resetWidth, rowHeight))) {
                g_isConfiguringSamplePoints = true;
                g_hasAnalysisStarted = false;
                g_debugSamples.clear();
                UpdateDebugSamples();
            }
        }
    }
}

} // namespace

void ShowMenu(int imageWidth, int imageHeight) {
    (void)imageWidth;
    (void)imageHeight;

    const float width = Px(376.0f);

    // Opens at the top right of the main display, clear of where boards usually
    // sit. Moved anywhere after that, and remembered.
    const float mainRight = (float)(GetSystemMetrics(SM_CXSCREEN) - g_virtualScreen.left);
    const float mainTop = (float)(0 - g_virtualScreen.top);
    ImGui::SetNextWindowPos(ImVec2(mainRight - width - Px(24.0f), mainTop + Px(96.0f)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("oracle", nullptr, flags)) {
        ImGui::End();
        return;
    }

    // Never StockfishIsAlive here: it waits for any search in progress, and this
    // runs on the thread that presents every frame.
    const bool engineAlive = StockfishLooksAlive();
    const EngineStatus engine = GetEngineStatus();
    const Snapshot snapshot = TakeSnapshot();
    const bool searching = engineAlive && !g_noBoard && !snapshot.movesCurrent &&
                           FactsFor(snapshot.trackedFen).state == GameState::Playing;

    DrawHeader(engine, engineAlive, searching);
    ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

    static const char* kTabs[] = { "Play", "Engine", "Calibrate" };
    ui::Tabs("tabs", &g_tab, kTabs, 3);
    ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));

    switch ((MenuTab)g_tab) {
    case MenuTab::Play:
        if (g_noBoard) DrawSetup(engine, engineAlive);
        else DrawGame(snapshot, engineAlive, searching);
        break;
    case MenuTab::Engine:
        DrawEngineTab(engine, engineAlive);
        break;
    case MenuTab::Calibrate:
        DrawCalibrateTab();
        break;
    }

    ImGui::End();
}

bool ConsumeMenuHideRequest() {
    const bool requested = g_hideRequested;
    g_hideRequested = false;
    return requested;
}

void SelectMenuTab(MenuTab tab) {
    g_tab = (int)tab;
}

void InitializeImGui(HWND hwndOverlay, ID3D11Device* device, ID3D11DeviceContext* deviceContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hwndOverlay);
    ImGui_ImplDX11_Init(device, deviceContext);
    SetupImGuiStyleAndFonts();
}

void SetupImGuiStyleAndFonts() {
    theme::Apply();
}

void CleanupImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
