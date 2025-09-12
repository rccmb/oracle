#include "Menu.h"


void ShowMenu(int imageWidth, int imageHeight) {
    // Static variable to control ImGui window open/close state
    static bool show_window = true;

    // Optional: Set initial position/size only on first use
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

    // ImGui window with its own title bar (no NoTitleBar flag)
    if (!ImGui::Begin("ROI Debug Controls", &show_window, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    ImGui::SliderInt("ROI X", &g_debugROI_x, 0, imageWidth - 1);
    ImGui::SliderInt("ROI Y", &g_debugROI_y, 0, imageHeight - 1);
    ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
    ImGui::SliderInt("Offset", &g_debugOffset, -32, 32);

    ImGui::End();
}

void InitializeImGui(HWND hwndOverlay, ID3D11Device* device, ID3D11DeviceContext* deviceContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplWin32_Init(hwndOverlay);
    ImGui_ImplDX11_Init(device, deviceContext);
    ImGui::StyleColorsDark();
}

void CleanupImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}