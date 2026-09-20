#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <WinBase.h>

#include "Utils.h"
#include "Globals.h"
#include "Structs.h"
#include "Overlay.h"
#include "ChessboardDetection.h"
#include "Direct3D.h"
#include "Menu.h"
#include "StockfishHandler.h"
#include "imgui.h"

static bool IMGUI_MENU_VISIBLE = false;

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
            IM_COL32(0, 255, 0, 255), 
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
    {
        std::lock_guard<std::mutex> snapshot(g_analysisStateMutex);
        bestMoves = g_sfBestMoves;
    }

    if (!g_isRescanning && !bestMoves.empty() && !g_isConfiguringCropRegion && !g_isConfiguringSamplePoints &&
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {

        const int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
        const int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;

        // Every suggestion is labelled with its rank, 1 being the engine's first
        // choice, on both the square it leaves and the square it lands on, so a
        // move can be read off the board without consulting the menu.
        struct Badge {
            std::string text;
            ImU32 fill;
            bool destination;
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
        auto verdictColor = [](const StockfishMove& move) -> ImU32 {
            if (move.mate) {
                return move.mateIn > 0 ? IM_COL32(150, 90, 240, 235)   // Mate for us.
                                       : IM_COL32(200, 40, 40, 235);   // Mate against us.
            }
            if (move.scoreCp > 50) return IM_COL32(40, 160, 70, 235);   // Winning.
            if (move.scoreCp < -50) return IM_COL32(200, 60, 50, 235);  // Losing.
            return IM_COL32(190, 150, 40, 235);                         // Level.
        };

        for (size_t i = 0; i < bestMoves.size(); ++i) {
            const StockfishMove& move = bestMoves[i];
            if (move.uci.length() < 4) continue;

            const std::string label = std::to_string((int)i + 1);
            const ImU32 color = verdictColor(move);

            const int from = squareIndex(move.uci[0], move.uci[1]);
            const int to = squareIndex(move.uci[2], move.uci[3]);
            if (from >= 0) badgesBySquare[from].push_back({ label, color, false });
            if (to >= 0) badgesBySquare[to].push_back({ label, color, true });
        }

        // Large enough to read at a glance, small enough that a row of them fits
        // across one square.
        const float fontSize = std::clamp(cellHeight * 0.30f, 11.0f, 34.0f);
        const float padX = std::max(3.0f, fontSize * 0.28f);
        const float padY = std::max(1.0f, fontSize * 0.10f);
        const float gap = std::max(2.0f, fontSize * 0.16f);

        for (const auto& entry : badgesBySquare) {
            const int row = entry.first / 8;
            const int col = entry.first % 8;
            const std::vector<Badge>& badges = entry.second;

            // Measure, then shrink to fit if the row would run past the square.
            // Several moves landing on one square is common, and the whole point
            // of laying them side by side is defeated if the row spills onto the
            // neighbours.
            float scale = 1.0f;
            std::vector<ImVec2> sizes;
            float totalWidth = 0.0f;
            float rowHeight = 0.0f;

            for (int attempt = 0; attempt < 2; ++attempt) {
                sizes.clear();
                sizes.reserve(badges.size());
                totalWidth = 0.0f;
                rowHeight = 0.0f;

                for (const Badge& badge : badges) {
                    const ImVec2 size = ImGui::GetFont()->CalcTextSizeA(
                        fontSize * scale, FLT_MAX, 0.0f, badge.text.c_str());
                    sizes.push_back(size);
                    totalWidth += size.x + padX * scale * 2.0f;
                    rowHeight = std::max(rowHeight, size.y + padY * scale * 2.0f);
                }
                totalWidth += gap * scale * (float)(badges.size() - 1);

                const float available = (float)cellWidth * 0.94f;
                if (attempt == 0 && totalWidth > available && totalWidth > 0.0f) {
                    // One rescale is enough: the measurement is very close to
                    // linear in the font size.
                    scale = std::max(0.45f, available / totalWidth);
                    continue;
                }
                break;
            }

            const float badgePadX = padX * scale;
            const float badgePadY = padY * scale;
            const float badgeGap = gap * scale;

            // Sat near the top of the square, which leaves the piece itself
            // visible underneath.
            const float squareLeft = (float)(g_boardRect.left + col * cellWidth);
            const float squareTop = (float)(g_boardRect.top + row * cellHeight);
            float cursorX = squareLeft + ((float)cellWidth - totalWidth) * 0.5f;
            const float badgeTop = squareTop + (float)cellHeight * 0.06f;

            for (size_t i = 0; i < badges.size(); ++i) {
                const Badge& badge = badges[i];
                const float badgeWidth = sizes[i].x + badgePadX * 2.0f;

                const ImVec2 min(cursorX, badgeTop);
                const ImVec2 max(cursorX + badgeWidth, badgeTop + rowHeight);

                // A source square is outlined, a destination is filled, so the
                // two ends of a move stay distinguishable at a glance.
                if (badge.destination) {
                    draw_list->AddRectFilled(min, max, badge.fill, rowHeight * 0.25f);
                }
                else {
                    draw_list->AddRectFilled(min, max, IM_COL32(20, 20, 20, 190), rowHeight * 0.25f);
                    draw_list->AddRect(min, max, badge.fill, rowHeight * 0.25f, 0, 2.0f);
                }

                draw_list->AddText(ImGui::GetFont(), fontSize * scale,
                    ImVec2(cursorX + badgePadX, badgeTop + badgePadY),
                    IM_COL32(255, 255, 255, 255), badge.text.c_str());

                cursorX += badgeWidth + badgeGap;
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

    // Initializing Stockfish.
    LaunchStockfish("stockfish/stockfish.exe");

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
