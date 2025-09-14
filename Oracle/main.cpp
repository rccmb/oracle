#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#include "Utils.h"
#include "Globals.h"
#include "Structs.h"
#include "Overlay.h"
#include "ChessboardDetection.h"
#include "Direct3D.h"
#include "Menu.h"
#include "imgui.h"

static bool IMGUI_MENU_VISIBLE = false;

// Helper: Capture board clicks. 
void CaptureBoardClicks(HWND hwndDesktop) {
    ImGuiIO& io = ImGui::GetIO();
    if (!g_boardClicksReady && IMGUI_MENU_VISIBLE) {
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && !io.WantCaptureMouse) {
            cv::Mat screenshot = HWND2MAT(hwndDesktop);

            POINT p;
            GetCursorPos(&p);

            if (g_clickStage == 0) {
                g_firstClick.x = p.x;
                g_firstClick.y = p.y;
                if (g_firstClick.y >= 0 && g_firstClick.y < screenshot.rows && g_firstClick.x >= 0 && g_firstClick.x < screenshot.cols)
                    g_firstClick.grayscaleValue = screenshot.at<uchar>(g_firstClick.y, g_firstClick.x);
                g_clickStage = 1;
            } else if (g_clickStage == 1) {
                g_secondClick.x = p.x;
                g_secondClick.y = p.y;
                if (g_secondClick.y >= 0 && g_secondClick.y < screenshot.rows && g_secondClick.x >= 0 && g_secondClick.x < screenshot.cols)
                    g_secondClick.grayscaleValue = screenshot.at<uchar>(g_secondClick.y, g_secondClick.x);
                g_clickStage = 2;
            }
            Sleep(200); 
        }
    }
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

    // Drawing the sampling points.
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    
    // Only draw sample points during configuration mode.
    if (!g_isRescanning && g_isConfiguringSamplePoints && !g_userSamplePoints.empty() && 
        (g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
        int cellWidth = (g_boardRect.right - g_boardRect.left) / 8;
        int cellHeight = (g_boardRect.bottom - g_boardRect.top) / 8;
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                // Calculate cell center.
                int cellCenterX = g_boardRect.left + (col * cellWidth) + (cellWidth / 2);
                int cellCenterY = g_boardRect.top + (row * cellHeight) + (cellHeight / 2);
                
                // Apply slider values.
                int patchSize = g_debugPatchSize;
                int offset = g_debugOffset;
                int half = patchSize / 2;
                
                int sampleX = cellCenterX - half;
                int sampleY = cellCenterY - half + offset;
                
                draw_list->AddRectFilled(
                    ImVec2((float)sampleX, (float)sampleY),
                    ImVec2((float)(sampleX + patchSize), (float)(sampleY + patchSize)),
                    IM_COL32(0, 255, 255, 128) 
                );
            }
        }
    }
    
    // Draw debug samples only during configuration mode.
    if (!g_isRescanning && g_isConfiguringSamplePoints) {
        for (const SAMPLE& s : g_debugSamples) {
            draw_list->AddRectFilled(
                ImVec2((float)s.x, (float)s.y),
                ImVec2((float)(s.x + s.width), (float)(s.y + s.height)),
                IM_COL32(255, 0, 0, 128)
            );
        }
    }

	// Showing the ImGui menu.
    if (IMGUI_MENU_VISIBLE) {
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

    // Create ChessboardDetectionThread.
    CreateThread(NULL, 0, ChessboardDetectionThread, hwndOverlay, 0, NULL);

    MSG msg = {};
    while (true) {
		// Toggle ImGui menu. LCONTROL + F1 to enable/disable.
        if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_F1) & 0x8000)) {

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

        // Capture board clicks using hotkey
        CaptureBoardClicks(hwndDesktop);

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
