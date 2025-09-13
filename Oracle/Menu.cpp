#include "Menu.h"
#include "ChessboardDetection.h"
#include "Utils.h"

void ShowMenu(int imageWidth, int imageHeight) {
    static bool show_window = true;

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

    // Main window.
    if (!ImGui::Begin("Chess Analysis Controls", &show_window, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    // Board Detection Section
    ImGui::Text("Board Detection");
    ImGui::Separator();
    
    if (ImGui::Button("Rescan Board")) {
        // Stop any ongoing analysis
        g_hasAnalysisStarted = false;
        g_isConfiguringSamplePoints = true;
        g_isRescanning = true; // Set rescan flag to clear visuals
        
        // Clear existing data
        g_debugSamples.clear();
        g_userSamplePoints.clear();
        g_boardRect = { 0, 0, 0, 0 }; // This will hide the board rectangle
        
        std::cout << "[INFO] Starting complete board rescan - please click two points to define the chessboard" << std::endl;
        
        // Restart the entire process - get new clicks and detect board
        std::pair<CLICK, CLICK> clicks = DetectChessboardColorCoding(GetDesktopWindow());
        SetChessboardClicks(clicks);
        
        cv::Mat screenshot = HWND2MAT(GetDesktopWindow());
        DetectBoardDimensions(screenshot);
        
        // Clear rescan flag after detection is complete
        g_isRescanning = false;
        
        std::cout << "[INFO] Board rescanned - please reconfigure sample points" << std::endl;
    }
    
    ImGui::Separator();
    
    // Sample Point Configuration Section
    ImGui::Text("Sample Point Configuration");
    ImGui::Separator();
    
    if (g_isConfiguringSamplePoints) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Configuration Mode: ACTIVE");
        ImGui::Text("Adjust the sliders below to position sample points");
        ImGui::Text("Sample points are automatically placed in each cell center");
        ImGui::Text("Sample points are visible on screen for configuration");
        
        if (ImGui::Button("Start Analysis")) {
            g_isConfiguringSamplePoints = false;
            g_hasAnalysisStarted = true;
            // Start the piece color analysis here
            cv::Mat screenshot = HWND2MAT(GetDesktopWindow());
            DetectPieceColorCoding(screenshot, (g_boardRect.right - g_boardRect.left) / 8, (g_boardRect.bottom - g_boardRect.top) / 8);
        }
        
        ImGui::Text("Sample Points: %d", (int)g_userSamplePoints.size());
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Analysis Mode: ACTIVE");
        ImGui::Text("Sample points are hidden to avoid interference");
        ImGui::Text("Analysis is using your configured sampling parameters");
        
        if (ImGui::Button("Reset Configuration")) {
            g_isConfiguringSamplePoints = true;
            g_hasAnalysisStarted = false;
            g_debugSamples.clear();
            // Update debug samples with current configuration
            UpdateDebugSamples();
        }
    }

    ImGui::Separator();
    ImGui::Text("Sample Point Parameters");
    
    // Track if sliders changed
    static int prevPatchSize = g_debugPatchSize;
    static int prevOffset = g_debugOffset;
    
    ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
    ImGui::SliderInt("Offset", &g_debugOffset, -32, 32);
    
    // Update debug samples if sliders changed
    if (prevPatchSize != g_debugPatchSize || prevOffset != g_debugOffset) {
        UpdateDebugSamples();
        prevPatchSize = g_debugPatchSize;
        prevOffset = g_debugOffset;
    }
    

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