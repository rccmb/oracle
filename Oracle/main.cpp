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
    if ((g_boardRect.right - g_boardRect.left) > 0 && (g_boardRect.bottom - g_boardRect.top) > 0) {
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

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    for (const SAMPLE& s : g_debugSamples) {
        draw_list->AddRectFilled(
            ImVec2((float)s.x, (float)s.y),
            ImVec2((float)(s.x + s.width), (float)(s.y + s.height)),
            IM_COL32(255, 0, 0, 128) // Semi-transparent red
        );
        // Optionally, add a border:
        draw_list->AddRect(
            ImVec2((float)s.x, (float)s.y),
            ImVec2((float)(s.x + s.width), (float)(s.y + s.height)),
            IM_COL32(255, 0, 0, 255), // Opaque red border
            0.0f, 0, 1.0f
        );
    }

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

	// Creating the Direct3D device.
    if (!CreateDeviceD3D(hwndOverlay)) {
        MessageBox(NULL, L"Failed to create D3D11 device!", L"Error", MB_OK);
        return 1;
    }

    // Creating the render target.
    CreateRenderTarget();

    // Initializing ImGui.
    InitializeImGui(hwndOverlay, g_pd3dDevice, g_pd3dDeviceContext);

	// Get chessboard color coding.
    HWND hwndDesktop = GetDesktopWindow();
	std::pair<CLICK, CLICK> clicks = GetChessboardColorCoding(hwndDesktop);
    SetChessboardClicks(clicks);

    // Create ChessboardDetectionThread.
    CreateThread(NULL, 0, ChessboardDetectionThread, hwndOverlay, 0, NULL);

    MSG msg = {};
    while (true) {
		// Toggle ImGui menu. LCONTROL + F1 to enable/disable.
        if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) && 
            (GetAsyncKeyState(VK_F1) & 0x8000)) {

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
