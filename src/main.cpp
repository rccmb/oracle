#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <WinBase.h>
#include <shellapi.h>

#include "platform/Utils.h"
#include "Globals.h"
#include "Structs.h"
#include "platform/Overlay.h"
#include "vision/ChessboardDetection.h"
#include "platform/Direct3D.h"
#include "ui/Menu.h"
#include "engine/StockfishHandler.h"
#include "imgui.h"

static bool IMGUI_MENU_VISIBLE = false;

// Engine given as "--engine <path>", or empty for the bundled one.
//
// Read from GetCommandLineW rather than the PSTR WinMain is handed, so a path
// with spaces or non-ASCII characters survives, which the narrow argument does
// not reliably manage.
static std::string EngineFromCommandLine() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return {};

    std::string engine;
    for (int i = 1; i + 1 < count; ++i) {
        if (wcscmp(arguments[i], L"--engine") != 0) continue;

        const int bytes = WideCharToMultiByte(CP_UTF8, 0, arguments[i + 1], -1,
                                              nullptr, 0, nullptr, nullptr);
        if (bytes > 1) {
            engine.resize((size_t)bytes - 1);
            WideCharToMultiByte(CP_UTF8, 0, arguments[i + 1], -1,
                                engine.data(), bytes, nullptr, nullptr);
        }
        break;
    }

    LocalFree(arguments);
    return engine;
}

void CaptureBoardClicks() {
    ImGuiIO& io = ImGui::GetIO();
    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && !io.WantCaptureMouse) {
        SetBoardClicks();
    }
}

void ToggleMenu(HWND hwndOverlay) {
    IMGUI_MENU_VISIBLE = !IMGUI_MENU_VISIBLE;

    // Enable/disable click-through.
    LONG_PTR exStyle = GetWindowLongPtr(hwndOverlay, GWL_EXSTYLE);
    if (IMGUI_MENU_VISIBLE) {
        SetWindowLongPtr(hwndOverlay, GWL_EXSTYLE, exStyle & ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
        SetForegroundWindow(hwndOverlay);
    }
    else {
        SetWindowLongPtr(hwndOverlay, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }

    Sleep(100);
}

void RenderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Drawing the board rectangle.
    if (!g_isRescanning && (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        draw_list->AddRect(
            ImVec2((float)g_boardRect.left, (float)g_boardRect.top),
            ImVec2((float)g_boardRect.right, (float)g_boardRect.bottom),
            IM_COL32(64, 196, 148, 120),
            0.0f,
            0,
            1.0f 
        );
    }

    // Drawing the sampling points from during configuration mode.
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (!g_isRescanning && g_isConfiguringSamplePoints && !g_debugSamples.empty() &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
        for (const SAMPLE& s : g_debugSamples) {
            draw_list->AddRectFilled(
                ImVec2((float)s.x, (float)s.y),
                ImVec2((float)(s.x + s.width), (float)(s.y + s.height)),
                IM_COL32(0, 255, 255, 128)
            );
        }
    }

    // Draw crop rects during crop configuration.
    if (!g_isRescanning && g_isConfiguringCropRegion && !g_cropRects.empty() &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
        for (const SAMPLE& r : g_cropRects) {
            draw_list->AddRect(
                ImVec2((float)r.x, (float)r.y),
                ImVec2((float)(r.x + r.width), (float)(r.y + r.height)),
                IM_COL32(255, 255, 0, 200), 0.0f, 0, 1.0f
            );
        }
    }

    // Draw the best move on the board. Copied out of the shared state first: the
    // detection thread reallocates this vector and the strings inside it.
    std::vector<StockfishMove> bestMoves;
    bool movesAreOurs = true;
    std::string moveVerdict;
    std::string moveVerdictUci;
    int moveVerdictLoss = 0;
    bool moveVerdictByUs = false;
    {
        std::lock_guard<std::mutex> snapshot(g_analysisStateMutex);
        bestMoves = g_sfBestMoves;
        movesAreOurs = g_sfMovesAreOurs;
        moveVerdict = g_lastMoveVerdict;
        moveVerdictUci = g_lastMoveVerdictUci;
        moveVerdictLoss = g_lastMoveLossCp;
        moveVerdictByUs = g_lastMoveVerdictByUs;
    }

    /* BLUNDER CALLOUT. */
    // Sits above the board, so the one thing most worth knowing does not require
    // opening the menu to see.
    if (!g_isRescanning && !moveVerdict.empty() &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {

        const ImU32 severity =
            (moveVerdict == "Blunder")  ? IM_COL32(214, 48, 49, 240) :
            (moveVerdict == "Mistake")  ? IM_COL32(243, 156, 18, 240) :
                                          IM_COL32(241, 196, 15, 240);

        char callout[96];
        std::snprintf(callout, sizeof(callout), "%s  %s  %s  -%.1f",
            moveVerdictByUs ? "You" : "Opponent",
            moveVerdict.c_str(),
            moveVerdictUci.c_str(),
            moveVerdictLoss / 100.0f);

        const int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
        const float fontSize = std::clamp(cellHeight * 0.20f, 12.0f, 20.0f);
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, callout);

        const float padX = fontSize * 0.5f;
        const float padY = fontSize * 0.28f;
        const float boxWidth = textSize.x + padX * 2.0f;
        const float boxHeight = textSize.y + padY * 2.0f;

        const float centerX = (float)(g_boardRect.left + g_boardRect.right) * 0.5f;
        float boxTop = (float)g_boardRect.top - boxHeight - 8.0f;
        // Dropped below the board when there is no room above it.
        if (boxTop < 4.0f) boxTop = (float)g_boardRect.bottom + 8.0f;

        const ImVec2 boxMin(centerX - boxWidth * 0.5f, boxTop);
        const ImVec2 boxMax(boxMin.x + boxWidth, boxTop + boxHeight);

        draw_list->AddRectFilled(boxMin, boxMax, severity, boxHeight * 0.22f);
        draw_list->AddText(ImGui::GetFont(), fontSize,
            ImVec2(boxMin.x + padX, boxMin.y + padY),
            IM_COL32(255, 255, 255, 255), callout);
    }

    if (!g_isRescanning && movesAreOurs && !bestMoves.empty() && !g_isConfiguringCropRegion && !g_isConfiguringSamplePoints &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {

        const int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
        const int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

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

        // Colour carries the evaluation, the number carries the ranking. Scores
        // arrive from the moving side's point of view, and moves are only
        // requested on our own turn, so a positive score is good for us.
        //
        // A level score is slate, not the muddy yellow it used to be: a dark
        // desaturated yellow over a wooden board is indistinguishable from the
        // board, and looks like a stain rather than a reading.
        auto verdictColor = [](const StockfishMove& move) -> ImU32 {
            if (move.mate) {
                return move.mateIn > 0 ? IM_COL32(138, 84, 222, 240)   // Mate for us.
                                       : IM_COL32(198, 44, 44, 240);   // Mate against us.
            }
            if (move.scoreCp > 50) return IM_COL32(38, 152, 70, 240);   // Winning.
            if (move.scoreCp < -50) return IM_COL32(208, 56, 56, 240);  // Losing.
            return IM_COL32(92, 106, 128, 240);                         // Level.
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

            const std::string label = std::to_string((int)i + 1);
            const ImU32 color = verdictColor(move);

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

                const ImVec2 shaftStart(start.x + dx * clearTail, start.y + dy * clearTail);
                const ImVec2 tip(end.x - dx * clearHead, end.y - dy * clearHead);
                const ImVec2 shaftEnd(tip.x - dx * headLength, tip.y - dy * headLength);

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
        for (const auto& entry : badgesBySquare) {
            for (const Badge& badge : entry.second) {
                if (badge.radius <= 0.0f) continue;

                // A destination is solid, a source is a ring, so the two ends of
                // one move stay distinguishable without a second colour.
                if (badge.destination) {
                    draw_list->AddCircleFilled(badge.center, badge.radius, badge.fill, 20);
                }
                else {
                    draw_list->AddCircleFilled(badge.center, badge.radius, IM_COL32(18, 18, 20, 205), 20);
                    draw_list->AddCircle(badge.center, badge.radius - 0.5f, badge.fill, 20,
                                         std::max(1.2f, badge.radius * 0.16f));
                }

                const float fontSize = badge.radius * 1.30f;
                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(
                    fontSize, FLT_MAX, 0.0f, badge.text.c_str());
                draw_list->AddText(ImGui::GetFont(), fontSize,
                    ImVec2(badge.center.x - textSize.x * 0.5f, badge.center.y - textSize.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), badge.text.c_str());
            }
        }
    }

	// Showing the ImGui menu.
    if (IMGUI_MENU_VISIBLE) {
        ShowMenu(g_virtualScreen.right - g_virtualScreen.left, g_virtualScreen.bottom - g_virtualScreen.top);
    }

    ImGui::Render();

    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
    // Must precede every window and every capture: on a scaled display an
    // unaware process is handed virtualised coordinates, so cursor positions and
    // captured pixels disagree and calibration clicks land on the wrong square.
    InitializeDisplayMetrics();

    // Reference pieces live beside the executable, not in whatever directory
    // Oracle happened to be started from. Otherwise a board detected while
    // debugging leaves its references somewhere the released binary cannot find.
    if (const std::filesystem::path exeDir = ExecutableDirectory(); !exeDir.empty()) {
        g_tempDir = exeDir / "temp";
    }

	// Creating overlay.
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const LPCWSTR className = L"Oracle Overlay";
    HWND hwndOverlay = CreateOverlayWindow(hInstance, className);

	// Creating the Direct3D device.
    if (!CreateDeviceD3D(hwndOverlay)) {
        MessageBox(NULL, L"Failed to create D3D11 device!", L"Error", MB_OK);
        return 1;
    }

    // Creating the render target.
    CreateRenderTarget();

    // Initializing ImGui.
    InitializeImGui(hwndOverlay, g_pd3dDevice, g_pd3dDeviceContext);

    // Any UCI engine, not just the bundled one. Empty means "find the bundled
    // Stockfish", which is what happens when the flag is not given.
    LaunchStockfish(EngineFromCommandLine());

    bool analysisNotStarted = true;

    MSG msg = {};
    while (true) {
		// Toggle ImGui menu. LCONTROL + F1  OR ADD + SUBTRACT to enable/disable.
        if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_F1) & 0x8000) || (GetAsyncKeyState(VK_ADD) & 0x8000) &&
            (GetAsyncKeyState(VK_SUBTRACT) & 0x8000)) {

            ToggleMenu(hwndOverlay);
        }

        // Capture board clicks using hotkey. ONLY USED IN CONFIGURATION.
        if (!g_boardClicksReady && IMGUI_MENU_VISIBLE && g_userScreenshotReady) {
            CaptureBoardClicks();
        }

		// Handle Windows messages.
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                return 0;
        }

        if (g_hasAnalysisStarted && analysisNotStarted) {
            CreateThread(NULL, 0, ChessboardDetectionThread, hwndOverlay, 0, NULL);
			analysisNotStarted = false;
        }
        
        // New frame.
        RenderFrame();

        Sleep(10);
    }

    CleanupImGui();
	CleanupDirect3D();

    return 0;
}
