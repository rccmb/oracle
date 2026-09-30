#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>
#include <cstdio>
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
#include "ui/BoardOverlay.h"
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

    DrawBoardOverlay(ImGui::GetBackgroundDrawList());

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

        // The menu's own hide button.
        if (IMGUI_MENU_VISIBLE && ConsumeMenuHideRequest()) {
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
