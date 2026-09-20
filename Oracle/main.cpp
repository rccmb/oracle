#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>
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
        
        const auto& bestMove = bestMoves.front();
        if (bestMove.uci.length() >= 4) {
            char srcFile = bestMove.uci[0];
            char srcRank = bestMove.uci[1];
            char dstFile = bestMove.uci[2];
            char dstRank = bestMove.uci[3];
            
            auto DrawMoveDot = [&](char f, char r, ImU32 color) {
                int col = f - 'a';
                int row = '8' - r;
                if (g_orientation == 1) { // Black at bottom
                    col = 7 - col;
                    row = 7 - row;
                }
                
                int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
                int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
                
                float radius = std::min(cellWidth, cellHeight) / 20.0f;
                int cx = g_boardRect.left + col * cellWidth + (int)(radius + 4.0f);
                int cy = g_boardRect.top + row * cellHeight + (int)(radius + 4.0f);
                
                draw_list->AddCircleFilled(ImVec2((float)cx, (float)cy), radius, color);
            };
            
            // Draw Orange dot for Source, Green dot for Destination
            DrawMoveDot(srcFile, srcRank, IM_COL32(255, 165, 0, 200)); 
            DrawMoveDot(dstFile, dstRank, IM_COL32(0, 255, 0, 200));   
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
