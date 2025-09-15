#include "Menu.h"

void ShowMenu(int imageWidth, int imageHeight) {
    static bool show_window = true;

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Chess Analysis Controls", &show_window, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    // Board Detection Section
    ImGui::Text("Board Detection");
    ImGui::Separator();

    if (!g_userScreenshotReady) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "You must take a screenshot before setting clicks.");
        if (ImGui::Button("Take Screenshot")) {
            g_userScreenshotGray = HWND2MAT(GetDesktopWindow());
            if (!g_userScreenshotGray.empty()) {
                g_userScreenshotReady = true;
                g_clickStage = 0;
                g_viewFirstClick = { -1, -1, 0 };
                g_viewSecondClick = { -1, -1, 0 };
                std::cout << "[INFO] Screenshot captured for click sampling (" << g_userScreenshotGray.cols << "x" << g_userScreenshotGray.rows << ")" << std::endl;
            } else {
                std::cout << "[ERROR] Failed to capture screenshot." << std::endl;
            }
        }
        ImGui::Separator();
    }

    if (!g_boardClicksReady && g_userScreenshotReady) {
        ImGui::Text("Click on the board using Ctrl+LMB to set corners:");
        if (g_clickStage == 0) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Waiting for FIRST click...");
        }
        else if (g_clickStage == 1) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Waiting for SECOND click...");
            ImGui::Text("First: (%d, %d)", g_viewFirstClick.x, g_viewFirstClick.y);
        }
        if (ImGui::Button("Reset Board Clicks")) {
            g_clickStage = 0;
            g_viewFirstClick = { -1, -1, 0 };
            g_viewSecondClick = { -1, -1, 0 };
			g_clicks = { g_viewFirstClick, g_viewSecondClick };
        }
        if (g_clickStage == 2) {
            if (ImGui::Button("Detect Board")) {
                g_boardClicksReady = true;
				g_clicks = { g_viewFirstClick, g_viewSecondClick };
                DetectBoardDimensions();
                g_isRescanning = false;
                g_isConfiguringSamplePoints = true;
                g_samplePointsSet = false;
                g_refBoardColor1 = (int)g_clicks.first.grayscaleValue;
                g_refBoardColor2 = (int)g_clicks.second.grayscaleValue;
            }
            ImGui::Text("First: (%d, %d)  Second: (%d, %d)", g_viewFirstClick.x, g_viewFirstClick.y, g_viewSecondClick.x, g_viewSecondClick.y);
        }
        ImGui::Separator();
    }
    
    if (ImGui::Button("Rescan Board")) {
        g_hasAnalysisStarted = false;
        g_isConfiguringSamplePoints = true;
        g_isRescanning = true;
        g_boardClicksReady = false;
        g_userScreenshotReady = false;
        g_userScreenshotGray.release();
        g_clickStage = 0;
        g_viewFirstClick = { -1, -1, 0 };
        g_viewSecondClick = { -1, -1, 0 };
        g_debugSamples.clear();
        g_boardRect = { 0, 0, 0, 0 };
        g_clicks = { g_viewFirstClick, g_viewSecondClick };
        g_debugPatchSize = 5;
        g_debugOffsetX = 0;
        g_debugOffsetY = 0;
        g_samplePointsSet = false;
        g_refBlackPiece = -1;
        g_refWhitePiece = -1;
        g_refBoardColor1 = -1;
        g_refBoardColor2 = -1;
        std::cout << "[INFO] Board rescan requested - please click two points to define the chessboard" << std::endl;
    }
    
    ImGui::Separator();
    
    // Sample Point Configuration Section
    ImGui::Text("Sample Point Configuration");
    ImGui::Separator();
    
    if (g_isConfiguringSamplePoints) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Configuration Mode: ACTIVE");

        ImGui::BeginDisabled(!(g_userScreenshotReady && g_boardClicksReady && g_samplePointsSet));
        if (ImGui::Button("Start Analysis")) {
            g_isConfiguringSamplePoints = false;
            g_hasAnalysisStarted = true;
        }
        ImGui::EndDisabled();
        
        ImGui::Text("Sample Points: %d", (int)g_debugSamples.size());
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Analysis Mode: ACTIVE");
        
        if (ImGui::Button("Reset Configuration")) {
            g_isConfiguringSamplePoints = true;
            g_hasAnalysisStarted = false;
            g_debugSamples.clear();
            // Update debug samples with current configuration. 
            UpdateDebugSamples();
        }
    }

    ImGui::Separator();
    ImGui::Text("Sample Point Parameters");

    ImGui::Text("Adjust the sliders below to position sample points.");
    
    // Track if sliders changed.
    static int prevPatchSize = g_debugPatchSize;
    static int prevOffsetX = g_debugOffsetX;
    static int prevOffsetY = g_debugOffsetY;
    
    ImGui::BeginDisabled(g_samplePointsSet);
    ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
    ImGui::SliderInt("X Offset", &g_debugOffsetX, -32, 32);
    ImGui::SliderInt("Y Offset", &g_debugOffsetY, -32, 32);
    if (ImGui::Button("Set Sample Points")) {
        UpdateDebugSamples();
        DetectPieceColorCoding((g_boardRect.right - g_boardRect.left) / 8, (g_boardRect.bottom - g_boardRect.top) / 8);
        g_samplePointsSet = true;
    }
    ImGui::EndDisabled();

    // Update debug samples if sliders changed.
    if (!g_samplePointsSet && (prevPatchSize != g_debugPatchSize || prevOffsetX != g_debugOffsetX || prevOffsetY != g_debugOffsetY)) {
        UpdateDebugSamples();
        prevPatchSize = g_debugPatchSize;
        prevOffsetX = g_debugOffsetX;
        prevOffsetY = g_debugOffsetY;
    }

    ImGui::Separator();
    ImGui::Text("Reference Values");
    ImGui::Text("Black Piece Intensity: %d", g_refBlackPiece);
    ImGui::Text("White Piece Intensity: %d", g_refWhitePiece);
    ImGui::Text("Board Color 1: %d", g_refBoardColor1);
    ImGui::Text("Board Color 2: %d", g_refBoardColor2);


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