// Draws Oracle's menu and board overlay offscreen and writes them out as PNGs.
//
// The overlay window is excluded from screen capture, which is what stops Oracle
// reading its own arrows back as part of the board, so a screenshot never shows
// it. This runs the same drawing code into a texture instead, over a board, and
// composites the result the way the overlay window is composited on screen.
//
// That last part matters. The overlay is a colour keyed layered window: pure
// black is transparent and every other pixel is opaque. Nothing is ever alpha
// blended with the page underneath; anything drawn with alpha is blended with
// black instead, which is how a translucent yellow turns into brown. Previewing
// with ordinary alpha blending would hide exactly that.
//
// Usage: UiPreview.exe [output directory] [scene name filter]

#include "Globals.h"

#include <d3d11.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"

#include "Structs.h"
#include "chess/ChessRules.h"
#include "engine/StockfishHandler.h"
#include "ui/BoardOverlay.h"
#include "ui/Menu.h"
#include "ui/Theme.h"

#pragma comment(lib, "d3d11")

namespace {

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;

// Where the board sits in every scene. The same rectangle the screenshot in the
// repository root has its board at, so either backdrop lines up with it.
constexpr int kBoardLeft = 273;
constexpr int kBoardTop = 152;
constexpr int kCell = 107;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11Texture2D* g_target = nullptr;
ID3D11RenderTargetView* g_targetView = nullptr;
ID3D11Texture2D* g_staging = nullptr;

ImFont* g_pieceFont = nullptr;
ImFont* g_pageFont = nullptr;

bool CreateDevice() {
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    for (D3D_DRIVER_TYPE driver : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
        if (SUCCEEDED(D3D11CreateDevice(nullptr, driver, nullptr, 0, levels, 1,
                                        D3D11_SDK_VERSION, &g_device, nullptr, &g_context))) {
            break;
        }
    }
    if (!g_device) return false;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kWidth;
    desc.Height = kHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // The overlay's swap chain format.
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &g_target))) return false;
    if (FAILED(g_device->CreateRenderTargetView(g_target, nullptr, &g_targetView))) return false;

    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return SUCCEEDED(g_device->CreateTexture2D(&desc, nullptr, &g_staging));
}

// Renders ImGui's current draw data onto black, as the overlay does every
// frame, and reads it back as RGBA.
cv::Mat RenderToImage() {
    const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_context->OMSetRenderTargets(1, &g_targetView, nullptr);
    g_context->ClearRenderTargetView(g_targetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_context->CopyResource(g_staging, g_target);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(g_context->Map(g_staging, 0, D3D11_MAP_READ, 0, &mapped))) return {};

    cv::Mat rgba(kHeight, kWidth, CV_8UC4);
    for (int y = 0; y < kHeight; ++y) {
        std::memcpy(rgba.ptr(y), (const unsigned char*)mapped.pData + (size_t)y * mapped.RowPitch,
                    (size_t)kWidth * 4);
    }
    g_context->Unmap(g_staging, 0);
    return rgba;
}

// What the screen shows: the page wherever the overlay is pure black, and the
// overlay, opaque, everywhere else.
cv::Mat ColourKeyComposite(const cv::Mat& page, const cv::Mat& overlayRgba) {
    cv::Mat out = page.clone();
    for (int y = 0; y < out.rows; ++y) {
        const cv::Vec4b* source = overlayRgba.ptr<cv::Vec4b>(y);
        cv::Vec3b* target = out.ptr<cv::Vec3b>(y);
        for (int x = 0; x < out.cols; ++x) {
            const cv::Vec4b& p = source[x];
            if (p[0] == 0 && p[1] == 0 && p[2] == 0) continue;
            target[x] = cv::Vec3b(p[2], p[1], p[0]);
        }
    }
    return out;
}

void Frame(float deltaTime, const std::function<void()>& draw) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)kWidth, (float)kHeight);
    io.DeltaTime = deltaTime;
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    draw();
    ImGui::Render();
}

std::string Utf8(unsigned int codepoint) {
    char buffer[5] = {};
    ImTextCharToUtf8(buffer, codepoint);
    return buffer;
}

/* BACKDROPS. */

struct BoardStyle {
    const char* name;
    ImU32 page, panel, light, dark, lightMarked, darkMarked, whiteFill, whiteLine, blackFill, blackLine;
};

const BoardStyle kChessCom = {
    "chesscom",
    IM_COL32(48, 46, 43, 255), IM_COL32(38, 37, 34, 255),
    IM_COL32(235, 236, 208, 255), IM_COL32(115, 149, 82, 255),
    IM_COL32(245, 246, 130, 255), IM_COL32(185, 202, 67, 255),
    IM_COL32(249, 249, 249, 255), IM_COL32(40, 40, 40, 255),
    IM_COL32(88, 85, 82, 255), IM_COL32(22, 22, 22, 255),
};

const BoardStyle kLichess = {
    "lichess",
    IM_COL32(22, 21, 18, 255), IM_COL32(38, 36, 33, 255),
    IM_COL32(240, 217, 181, 255), IM_COL32(181, 136, 99, 255),
    IM_COL32(205, 210, 106, 255), IM_COL32(170, 162, 58, 255),
    IM_COL32(250, 250, 250, 255), IM_COL32(30, 30, 30, 255),
    IM_COL32(40, 40, 40, 255), IM_COL32(12, 12, 12, 255),
};

// FEN reading order square (0 is a8) to its top left corner on screen.
ImVec2 SquareOrigin(int square, int orientation) {
    int row = square / 8;
    int col = square % 8;
    if (orientation == 1) {
        row = 7 - row;
        col = 7 - col;
    }
    return ImVec2((float)(kBoardLeft + col * kCell), (float)(kBoardTop + row * kCell));
}

int SquareFromName(const std::string& name) {
    if (name.size() < 2) return -1;
    return ('8' - name[1]) * 8 + (name[0] - 'a');
}

std::vector<char> PlacementFromFen(const std::string& fen) {
    std::vector<char> squares(64, ' ');
    int index = 0;
    for (char c : fen) {
        if (c == ' ') break;
        if (c == '/') continue;
        if (c >= '1' && c <= '8') index += c - '0';
        else if (index < 64) squares[index++] = c;
    }
    return squares;
}

void DrawPiece(ImDrawList* list, const BoardStyle& style, char piece, ImVec2 origin) {
    static const char* kKinds = "kqrbnp";
    const char* kind = std::strchr(kKinds, std::tolower((unsigned char)piece));
    if (!kind || !*kind) return;
    const int offset = (int)(kind - kKinds);

    // Segoe UI Symbol has hollow "white" pieces and solid "black" ones. The
    // solid glyph is the body and the hollow one drawn over it is the outline.
    const std::string body = Utf8(0x265A + offset);
    const std::string line = Utf8(0x2654 + offset);
    const bool white = std::isupper((unsigned char)piece) != 0;

    const float size = kCell * 0.82f;
    const ImVec2 extent = g_pieceFont->CalcTextSizeA(size, FLT_MAX, 0.0f, body.c_str());
    const ImVec2 at(origin.x + (kCell - extent.x) * 0.5f, origin.y + (kCell - extent.y) * 0.5f + kCell * 0.02f);
    list->AddText(g_pieceFont, size, at, white ? style.whiteFill : style.blackFill, body.c_str());
    list->AddText(g_pieceFont, size, at, white ? style.whiteLine : style.blackLine, line.c_str());
}

// A plausible page around a board: the board with its coordinates and last
// move marked, the two player bars, and a side panel, in one site's colours.
cv::Mat RenderSyntheticPage(const BoardStyle& style, const std::string& fen, int orientation,
                            const std::string& lastMove) {
    Frame(1.0f / 60.0f, [&] {
        ImDrawList* list = ImGui::GetBackgroundDrawList();
        list->AddRectFilled(ImVec2(0, 0), ImVec2((float)kWidth, (float)kHeight), style.page);

        const float boardSize = kCell * 8.0f;
        const ImVec2 boardMin((float)kBoardLeft, (float)kBoardTop);
        const ImVec2 boardMax(boardMin.x + boardSize, boardMin.y + boardSize);

        // Side panel, where a site keeps its move list.
        const ImVec2 panelMin(boardMax.x + 28.0f, boardMin.y - 52.0f);
        const ImVec2 panelMax(panelMin.x + 460.0f, boardMax.y + 52.0f);
        list->AddRectFilled(panelMin, panelMax, style.panel, 6.0f);
        for (int i = 0; i < 12; ++i) {
            const float y = panelMin.y + 70.0f + i * 34.0f;
            list->AddRectFilled(ImVec2(panelMin.x + 24, y), ImVec2(panelMin.x + 54, y + 14), IM_COL32(90, 88, 84, 255), 3.0f);
            list->AddRectFilled(ImVec2(panelMin.x + 90, y), ImVec2(panelMin.x + 150, y + 14), IM_COL32(120, 118, 113, 255), 3.0f);
            if (i < 11) list->AddRectFilled(ImVec2(panelMin.x + 220, y), ImVec2(panelMin.x + 280, y + 14), IM_COL32(120, 118, 113, 255), 3.0f);
        }

        // Player bars above and below the board.
        auto playerBar = [&](float y, const char* name, const char* clock) {
            list->AddRectFilled(ImVec2(boardMin.x, y), ImVec2(boardMin.x + 40, y + 40), IM_COL32(110, 108, 104, 255), 4.0f);
            list->AddText(g_pageFont, 17.0f, ImVec2(boardMin.x + 52, y + 9), IM_COL32(236, 236, 236, 255), name);
            list->AddRectFilled(ImVec2(boardMax.x - 128, y), ImVec2(boardMax.x, y + 40), IM_COL32(152, 150, 145, 255), 4.0f);
            list->AddText(g_pageFont, 24.0f, ImVec2(boardMax.x - 104, y + 5), IM_COL32(40, 38, 35, 255), clock);
        };
        playerBar(boardMin.y - 52.0f, "Opponent  (1512)", "4:31");
        playerBar(boardMax.y + 12.0f, "You  (1498)", "4:48");

        const int markedFrom = lastMove.size() >= 4 ? SquareFromName(lastMove.substr(0, 2)) : -1;
        const int markedTo = lastMove.size() >= 4 ? SquareFromName(lastMove.substr(2, 2)) : -1;
        const std::vector<char> placement = PlacementFromFen(fen);

        for (int square = 0; square < 64; ++square) {
            const ImVec2 origin = SquareOrigin(square, orientation);
            const bool light = ((square / 8) + (square % 8)) % 2 == 0;
            const bool marked = square == markedFrom || square == markedTo;
            const ImU32 fill = marked ? (light ? style.lightMarked : style.darkMarked)
                                      : (light ? style.light : style.dark);
            list->AddRectFilled(origin, ImVec2(origin.x + kCell, origin.y + kCell), fill);
        }

        // Coordinates where sites put them: ranks in the top left of the left
        // column, files in the bottom right of the bottom row.
        for (int i = 0; i < 8; ++i) {
            const int rank = orientation == 0 ? 8 - i : i + 1;
            const char file = (char)(orientation == 0 ? 'a' + i : 'h' - i);
            const bool leftLight = (i % 2) == 0;
            char text[2] = { (char)('0' + rank), 0 };
            list->AddText(g_pageFont, 17.0f, ImVec2(boardMin.x + 5, boardMin.y + i * kCell + 3),
                          leftLight ? style.dark : style.light, text);
            text[0] = file;
            const bool bottomLight = (7 + i) % 2 == 0;
            list->AddText(g_pageFont, 17.0f, ImVec2(boardMin.x + (i + 1) * kCell - 14, boardMax.y - 22),
                          bottomLight ? style.dark : style.light, text);
        }

        for (int square = 0; square < 64; ++square) {
            if (placement[square] != ' ') DrawPiece(list, style, placement[square], SquareOrigin(square, orientation));
        }
    });

    cv::Mat rgba = RenderToImage();
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
    return bgr;
}

/* SCENES. */

struct Scene {
    std::string name;
    std::string backdrop = "chesscom";   // chesscom, lichess, or screenshot
    bool menu = true;
    MenuTab tab = MenuTab::Play;
    bool glyphs = false;                 // Draw the icon sheet instead of a game.
    ImVec2 mouse = ImVec2(-FLT_MAX, -FLT_MAX); // Where the pointer rests, for hover states.
    ImVec2 click = ImVec2(-FLT_MAX, -FLT_MAX); // Clicked once, early on, to open something.
    bool boardDetected = true;

    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    int orientation = 0;                 // 0 is White at the bottom.
    bool playWhite = true;
    int ply = 0;
    std::string lastMove;
    std::string lastMoveSan;             // As the tracker would have recorded it.
    bool inSync = true;
    bool searching = false;              // The engine has not answered for this position yet.
    bool noLegalMoves = false;

    std::vector<StockfishMove> moves;    // Best first, scores for the side to move.

    bool haveEval = true;
    int evalCpWhite = 0;
    bool evalIsMate = false;
    int mateInWhite = 0;
    int depth = 20;

    std::string verdict;                 // Blunder, Mistake or Inaccuracy.
    std::string verdictMove;
    std::string verdictMoveSan;
    std::string verdictBestSan;          // What the engine wanted instead.
    int verdictLoss = 0;
    bool verdictByUs = false;
};

StockfishMove Suggest(const char* uci, int cp, int rank) {
    StockfishMove move;
    move.uci = uci;
    move.scoreCp = cp;
    move.multipv = rank;
    return move;
}

std::vector<Scene> BuildScenes() {
    std::vector<Scene> scenes;

    Scene setup;
    setup.name = "setup";
    setup.boardDetected = false;
    setup.haveEval = false;
    scenes.push_back(setup);

    // Italian game, playing Black, three candidate replies.
    Scene game;
    game.name = "game";
    game.fen = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3";
    game.orientation = 1;
    game.playWhite = false;
    game.ply = 5;
    game.lastMove = "f1c4";
    game.lastMoveSan = "Bc4";
    game.moves = { Suggest("g8f6", 62, 1), Suggest("f8c5", 18, 2), Suggest("f8e7", -64, 3) };
    game.evalCpWhite = -62;
    game.depth = 22;
    scenes.push_back(game);

    Scene overlay = game;
    overlay.name = "overlay";
    overlay.menu = false;
    scenes.push_back(overlay);

    // Legal's trap on a lichess board: the opponent's ...g6 hangs e5.
    Scene verdict;
    verdict.name = "verdict";
    verdict.backdrop = "lichess";
    verdict.fen = "rn1qkbnr/ppp2p1p/3p2p1/4p3/2B1P1b1/2N2N2/PPPP1PPP/R1BQK2R w KQkq - 0 5";
    verdict.ply = 8;
    verdict.lastMove = "g7g6";
    verdict.lastMoveSan = "g6";
    verdict.moves = { Suggest("f3e5", 324, 1), Suggest("h2h3", 96, 2), Suggest("d2d4", 71, 3) };
    verdict.evalCpWhite = 324;
    verdict.depth = 24;
    verdict.verdict = "Blunder";
    verdict.verdictMove = "g7g6";
    verdict.verdictMoveSan = "g6";
    verdict.verdictBestSan = "Nf6";
    verdict.verdictLoss = 318;
    verdict.verdictByUs = false;
    scenes.push_back(verdict);

    Scene thinking = game;
    thinking.name = "thinking";
    thinking.searching = true;
    thinking.depth = 14;
    scenes.push_back(thinking);

    Scene lost = game;
    lost.name = "lost";
    lost.inSync = false;
    scenes.push_back(lost);

    // Fool's mate, from the losing side.
    Scene mate;
    mate.name = "mate";
    mate.fen = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3";
    mate.ply = 4;
    mate.lastMove = "d8h4";
    mate.lastMoveSan = "Qh4#";
    mate.noLegalMoves = true;
    mate.evalIsMate = true;
    mate.mateInWhite = 0;
    mate.depth = 0;
    scenes.push_back(mate);

    Scene screenshot;
    screenshot.name = "screenshot";
    screenshot.backdrop = "screenshot";
    screenshot.orientation = 1;
    screenshot.moves = { Suggest("e2e4", 38, 1), Suggest("d2d4", 31, 2), Suggest("g1f3", 24, 3) };
    screenshot.evalCpWhite = 38;
    scenes.push_back(screenshot);

    // The pointer resting on the main button, for its hover state.
    Scene hover = setup;
    hover.name = "hover";
    hover.mouse = ImVec2(1330, 364);
    scenes.push_back(hover);

    // Manual calibration opened, before any board is found.
    Scene manual = setup;
    manual.name = "manual";
    manual.tab = MenuTab::Calibrate;
    manual.click = ImVec2(1300, 625);
    scenes.push_back(manual);

    Scene engineTab = game;
    engineTab.name = "engine";
    engineTab.tab = MenuTab::Engine;
    scenes.push_back(engineTab);

    Scene calibrateTab = game;
    calibrateTab.name = "calibrate";
    calibrateTab.tab = MenuTab::Calibrate;
    scenes.push_back(calibrateTab);

    // Their move: the list shows their options, neutral, and nothing is drawn
    // on the board.
    Scene theirs = game;
    theirs.name = "theirs";
    theirs.fen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
    theirs.ply = 4;
    theirs.lastMove = "b8c6";
    theirs.lastMoveSan = "Nc6";
    theirs.moves = { Suggest("f1b5", 34, 1), Suggest("f1c4", 29, 2), Suggest("d2d4", 26, 3) };
    theirs.evalCpWhite = 34;
    scenes.push_back(theirs);

    // Every icon the interface uses, to check each one exists in the font.
    Scene glyphs;
    glyphs.name = "glyphs";
    glyphs.menu = false;
    glyphs.boardDetected = false;
    glyphs.glyphs = true;
    scenes.push_back(glyphs);

    return scenes;
}

void ApplyScene(const Scene& scene) {
    g_isRescanning = false;
    g_isConfiguringSamplePoints = false;
    g_isConfiguringCropRegion = false;
    g_showMoveArrows = true;
    g_sfPlayWhite = scene.playWhite;
    g_sfNoLegalMoves = scene.noLegalMoves;
    SelectMenuTab(scene.tab);

    if (!scene.boardDetected) {
        g_boardRect = { 0, 0, 0, 0 };
        g_noBoard = true;
        g_hasAnalysisStarted = false;
        g_samplePointsSet = false;
        g_cropRegionSet = false;
        g_boardClicksReady = false;
    }
    else {
        g_boardRect = { kBoardLeft, kBoardTop, kBoardLeft + kCell * 8, kBoardTop + kCell * 8 };
        g_noBoard = false;
        g_hasAnalysisStarted = true;
        g_samplePointsSet = true;
        g_cropRegionSet = true;
        g_boardClicksReady = true;
        g_clickStage = 2;
    }
    g_orientation = scene.orientation;

    // Reference colours as detection would have read them off this backdrop.
    const BoardStyle& style = scene.backdrop == "lichess" ? kLichess : kChessCom;
    auto bgr = [](ImU32 c) {
        return cv::Vec3b((uchar)((c >> IM_COL32_B_SHIFT) & 0xFF), (uchar)((c >> IM_COL32_G_SHIFT) & 0xFF),
                         (uchar)((c >> IM_COL32_R_SHIFT) & 0xFF));
    };
    g_refBoardColor1Color = bgr(style.light);
    g_refBoardColor2Color = bgr(style.dark);
    g_refWhitePieceColor = bgr(style.whiteFill);
    g_refBlackPieceColor = bgr(style.blackFill);
    g_refBoardColor1 = LuminanceOf(g_refBoardColor1Color);
    g_refBoardColor2 = LuminanceOf(g_refBoardColor2Color);
    g_refWhitePiece = LuminanceOf(g_refWhitePieceColor);
    g_refBlackPiece = LuminanceOf(g_refBlackPieceColor);

    {
        std::lock_guard<std::mutex> lock(g_highlightMutex);
        g_highlightColors = { bgr(style.lightMarked), bgr(style.darkMarked) };
    }

    const std::vector<char> placement = PlacementFromFen(scene.fen);
    std::vector<std::string> rows(8, std::string(8, ' '));
    for (int square = 0; square < 64; ++square) {
        int row = square / 8;
        int col = square % 8;
        if (scene.orientation == 1) {
            row = 7 - row;
            col = 7 - col;
        }
        rows[row][col] = placement[square];
    }

    const bool whiteToMove = scene.fen.find(" w ") != std::string::npos;

    // In SAN, as the analysis thread writes them.
    std::vector<StockfishMove> moves = scene.moves;
    if (const std::optional<ChessPosition> position = ChessPosition::FromFen(scene.fen)) {
        for (StockfishMove& move : moves) move.san = UciToSan(*position, move.uci);
    }
    {
        std::lock_guard<std::mutex> lock(g_analysisStateMutex);
        g_boardGridRows = scene.boardDetected ? rows : std::vector<std::string>(8, std::string(8, ' '));
        g_detectedLetters = placement;
        g_trackedFen = scene.boardDetected ? scene.fen : std::string();
        g_trackerPly = scene.ply;
        g_trackerInSync = scene.inSync;
        g_sfMovesAreOurs = whiteToMove == scene.playWhite;
        g_sideToMove = whiteToMove ? 'w' : 'b';
        g_lastMoveUci = scene.lastMove;
        g_lastMoveSan = scene.lastMoveSan;
        g_sfBestMoves = moves;
        g_sfBestMovesFen = scene.boardDetected && !scene.searching ? scene.fen : std::string();
        g_lastMoveVerdict = scene.verdict;
        g_lastMoveVerdictUci = scene.verdictMove;
        g_lastMoveVerdictSan = scene.verdictMoveSan;
        g_lastMoveVerdictBestSan = scene.verdictBestSan;
        g_lastMoveLossCp = scene.verdictLoss;
        g_lastMoveVerdictByUs = scene.verdictByUs;
    }

    g_liveEvalValid = scene.haveEval && scene.boardDetected;
    g_liveEvalCpWhite = scene.evalCpWhite;
    g_liveEvalIsMate = scene.evalIsMate;
    g_liveEvalMateInWhite = scene.mateInWhite;
    g_liveEvalDepth = scene.depth;
}

// Bounding box of every window drawn this frame, which is the menu.
cv::Rect WindowBounds() {
    ImRect bounds;
    bool any = false;
    for (ImGuiWindow* window : GImGui->Windows) {
        if (!window->Active || window->Hidden) continue;
        if (window->Flags & ImGuiWindowFlags_ChildWindow) continue;
        if (std::strncmp(window->Name, "Debug##", 7) == 0) continue;
        const ImRect rect = window->Rect();
        if (!any) bounds = rect;
        else bounds.Add(rect);
        any = true;
    }
    if (!any) return {};
    return cv::Rect((int)bounds.Min.x, (int)bounds.Min.y, (int)bounds.GetWidth(), (int)bounds.GetHeight());
}

void WriteCrop(const cv::Mat& image, cv::Rect region, const std::filesystem::path& path) {
    region &= cv::Rect(0, 0, image.cols, image.rows);
    if (region.width <= 0 || region.height <= 0) return;
    cv::imwrite(path.string(), image(region));
}

// Each icon the interface names, with its code point, over the panel colour.
void DrawGlyphSheet() {
    struct Glyph { const char* name; const char* text; };
    const Glyph glyphs[] = {
        { "CLOSE", ICON_CLOSE }, { "MINIMIZE", ICON_MINIMIZE }, { "SEARCH", ICON_SEARCH },
        { "REFRESH", ICON_REFRESH }, { "CHECK", ICON_CHECK }, { "DONE", ICON_DONE },
        { "PENDING", ICON_PENDING }, { "ERROR", ICON_ERROR }, { "WARNING", ICON_WARNING },
        { "INFO", ICON_INFO }, { "FOLDER", ICON_FOLDER }, { "CHEVRON_DOWN", ICON_CHEVRON_DOWN },
        { "CHEVRON_RIGHT", ICON_CHEVRON_RIGHT }, { "FLAG", ICON_FLAG }, { "POWER", ICON_POWER },
        { "PLAY", ICON_PLAY }, { "SETTINGS", ICON_SETTINGS }, { "BOLT", ICON_BOLT }, { "EYE", ICON_EYE },
    };

    ImDrawList* list = ImGui::GetForegroundDrawList();
    list->AddRectFilled(ImVec2(0, 0), ImVec2((float)kWidth, (float)kHeight), theme::kBg);
    const theme::Fonts& fonts = theme::GetFonts();
    for (int i = 0; i < (int)(sizeof(glyphs) / sizeof(glyphs[0])); ++i) {
        const float x = 60.0f + (i % 4) * 300.0f;
        const float y = 60.0f + (i / 4) * 90.0f;
        list->AddText(fonts.body, 32.0f, ImVec2(x, y), theme::kText, glyphs[i].text);
        std::string label = std::string(glyphs[i].name) + "  Aa";
        list->AddText(fonts.body, 14.0f, ImVec2(x + 50.0f, y + 4.0f), theme::kTextDim, label.c_str());
        // The icon inline with text, to check it sits on the baseline.
        std::string inline_ = std::string(glyphs[i].text) + "  Label";
        list->AddText(fonts.strong, 14.0f, ImVec2(x + 50.0f, y + 26.0f), theme::kText, inline_.c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path outDir = argc > 1 ? argv[1] : "out";
    const std::string filter = argc > 2 ? argv[2] : "";
    std::filesystem::create_directories(outDir);

    // Beside the executable, two folders down from the repository root.
    const std::filesystem::path repoRoot = ExecutableDirectory().parent_path().parent_path();

    if (!CreateDevice()) {
        std::fprintf(stderr, "Could not create a Direct3D 11 device.\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)kWidth, (float)kHeight);
    ImGui_ImplDX11_Init(g_device, g_context);
    SetupImGuiStyleAndFonts();

    g_pieceFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", 64.0f);
    g_pageFont = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeuib.ttf", 17.0f);

    // The real engine, so the menu reports what it would really report.
    LaunchStockfish("");
    std::printf("Engine: %s\n", g_sfEngineName.empty() ? "(none)" : g_sfEngineName.c_str());

    const cv::Mat screenshot = cv::imread((repoRoot / "screenshot.png").string(), cv::IMREAD_COLOR);

    for (const Scene& scene : BuildScenes()) {
        if (!filter.empty() && scene.name.find(filter) == std::string::npos) continue;

        cv::Mat page;
        if (scene.backdrop == "screenshot") {
            if (screenshot.empty() || screenshot.cols != kWidth || screenshot.rows != kHeight) {
                std::printf("skip %s: no 1920x1080 screenshot.png in the repository root\n", scene.name.c_str());
                continue;
            }
            page = screenshot;
        }
        else {
            page = RenderSyntheticPage(scene.backdrop == "lichess" ? kLichess : kChessCom,
                                       scene.fen, scene.orientation, scene.lastMove);
        }

        ApplyScene(scene);

        // Long enough for anything easing towards a value to arrive at it.
        cv::Rect menuBounds;
        for (int i = 0; i < 60; ++i) {
            ImGuiIO& input = ImGui::GetIO();
            if (scene.click.x > -FLT_MAX && (i == 4 || i == 5)) {
                input.AddMousePosEvent(scene.click.x, scene.click.y);
                input.AddMouseButtonEvent(0, i == 4);
            }
            else {
                input.AddMousePosEvent(scene.mouse.x, scene.mouse.y);
            }
            Frame(1.0f / 30.0f, [&] {
                // Beside the board rather than over it, where a player would
                // put the menu. Naming a window that does not exist is harmless.
                ImGui::SetWindowPos("oracle", ImVec2(1186, 104), ImGuiCond_Always);
                DrawBoardOverlay(ImGui::GetBackgroundDrawList());
                if (scene.menu) ShowMenu(kWidth, kHeight);
                if (scene.glyphs) DrawGlyphSheet();
                menuBounds = WindowBounds();
            });
        }

        const cv::Mat composite = ColourKeyComposite(page, RenderToImage());
        cv::imwrite((outDir / (scene.name + ".png")).string(), composite);

        if (scene.boardDetected) {
            WriteCrop(composite, cv::Rect(kBoardLeft - 150, kBoardTop - 100, kCell * 8 + 200, kCell * 8 + 170),
                      outDir / (scene.name + ".board.png"));
        }
        if (scene.menu && menuBounds.width > 0) {
            WriteCrop(composite, cv::Rect(menuBounds.x - 16, menuBounds.y - 16, menuBounds.width + 32, menuBounds.height + 32),
                      outDir / (scene.name + ".menu.png"));
        }
        std::printf("wrote %s\n", scene.name.c_str());
    }

    ShutdownStockfish();
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
