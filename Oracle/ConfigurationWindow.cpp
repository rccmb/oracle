#include "ConfigurationWindow.h"
#include "Overlay.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowDebugROIWindow(int imageWidth, int imageHeight) {
    ImGui::Begin("ROI Debug Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::SliderInt("ROI X", &g_debugROI_x, 0, imageWidth - 1);
    ImGui::SliderInt("ROI Y", &g_debugROI_y, 0, imageHeight - 1);
    ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
    ImGui::SliderInt("Offset", &g_debugOffset, -32, 32);

    ImGui::End();
}

HWND CreateImGuiWindow(HINSTANCE hInstance, const LPCWSTR className) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = ImGuiWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        className,
        L"ImGui Debug Window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL
    );
    ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}