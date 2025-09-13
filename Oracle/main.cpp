#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#include "Utils.h"
#include "Overlay.h"
#include "ChessboardDetection.h"
#include "Direct3D.h"
#include "Menu.h"

static bool show_imgui_menu = false;

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

    // Drawing the sampling points.
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    
    // Only draw sample points during configuration mode (not during analysis) and not during rescan
    if (!g_isRescanning && g_isConfiguringSamplePoints && !g_userSamplePoints.empty() && 
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
        int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
        int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                // Calculate cell center
                int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
                int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
                
                // Apply slider values
                int patchSize = g_debugPatchSize;
                int offset = g_debugOffset;
                int half = patchSize / 2;
                
                int sampleX = cellCenterX - half;
                int sampleY = cellCenterY - half + offset;
                
                // Draw the sample point
                draw_list->AddRectFilled(
                    ImVec2((float)sampleX, (float)sampleY),
                    ImVec2((float)(sampleX + patchSize), (float)(sampleY + patchSize)),
                    IM_COL32(0, 255, 255, 128) // Semi-transparent cyan
                );
            }
        }
    }
    
    // Draw debug samples only during configuration mode (exact ROI preview) and not during rescan
    if (!g_isRescanning && g_isConfiguringSamplePoints) {
        for (const SAMPLE& s : g_debugSamples) {
            draw_list->AddRectFilled(
                ImVec2((float)s.x, (float)s.y),
                ImVec2((float)(s.x + s.width), (float)(s.y + s.height)),
                IM_COL32(255, 0, 0, 128) // Semi-transparent red for debug samples
            );
        }
    }

	// Showing the ImGui menu.
    if (show_imgui_menu) {
        ShowMenu(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }

    ImGui::Render();

    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

int main() {
	// Creating overlay.
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const LPCWSTR className = L"Oracle Overlay";
    HWND hwndOverlay = CreateOverlayWindow(hInstance, className);
    HWND hwndDesktop = GetDesktopWindow();

	// Creating the Direct3D device.
    if (!CreateDeviceD3D(hwndOverlay)) {
        MessageBox(NULL, L"Failed to create D3D11 device!", L"Error", MB_OK);
        return 1;
    }

    // Creating the render target.
    CreateRenderTarget();

    // Initializing ImGui.
    InitializeImGui(hwndOverlay, g_pd3dDevice, g_pd3dDeviceContext);

    // CONFIGURATION - Board Detection Only.
    std::pair<CLICK, CLICK> clicks = DetectChessboardColorCoding(hwndDesktop);
    SetChessboardClicks(clicks);

    cv::Mat screenshot = HWND2MAT(hwndDesktop);
    DetectBoardDimensions(screenshot);

    // Create ChessboardDetectionThread.
    CreateThread(NULL, 0, ChessboardDetectionThread, hwndOverlay, 0, NULL);

    MSG msg = {};
    while (true) {
		// Toggle ImGui menu. LCONTROL + F1 to enable/disable.
        if ((GetAsyncKeyState(VK_ADD) & 0x8000) && 
            (GetAsyncKeyState(VK_SUBTRACT) & 0x8000)) {

            show_imgui_menu = !show_imgui_menu;

			// Enable/disable click-through.
            LONG_PTR exStyle = GetWindowLongPtr(hwndOverlay, GWL_EXSTYLE);
            if (show_imgui_menu) {
                SetWindowLongPtr(hwndOverlay, GWL_EXSTYLE, exStyle & ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
                SetForegroundWindow(hwndOverlay);
            }
            else {
                SetWindowLongPtr(hwndOverlay, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
            }

            Sleep(100);
        }

		// Handle Windows messages.
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                return 0;
        }
        
        // New frame.
        RenderFrame();

        Sleep(10);
    }

    CleanupImGui();
	CleanupDirect3D();

    return 0;
}
