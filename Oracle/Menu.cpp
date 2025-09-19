#include "Menu.h"

ImFont* CHESSBOARD_FONT;
ImFont* DEFAULT_FONT;

void ShowMenu(int imageWidth, int imageHeight) {
    static bool show_window = true;

    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("oracle.pro", &show_window, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    /* SET USER SCREENSHOT. */
    if (!g_userScreenshotReady) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "You must take a screenshot before setting clicks.");
        if (ImGui::Button("Take Screenshot")) {
            g_userScreenshotGray = HWND2MAT(GetDesktopWindow());
            if (!g_userScreenshotGray.empty()) {
                g_userScreenshotReady = true;
                g_clickStage = 0;
                g_viewFirstClick = { -1, -1, 0 };
                g_viewSecondClick = { -1, -1, 0 };
            } 
        }
        ImGui::Separator();
    }

    /* SET USER CLICKS. */
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
    
    /* CONFIGURATION RESET. */
    if (ImGui::Button("Rescan Board")) {
        g_hasAnalysisStarted = false;
        g_isConfiguringSamplePoints = true;
        g_isRescanning = true;
        g_isConfiguringCropRegion = false;
        g_boardClicksReady = false;
        g_userScreenshotReady = false;
        g_userScreenshotGray.release();
        g_clickStage = 0;
        g_viewFirstClick = { -1, -1, 0 };
        g_viewSecondClick = { -1, -1, 0 };
        g_debugSamples.clear();
        g_cropRects.clear();
        g_boardRect = { 0, 0, 0, 0 };
        g_clicks = { g_viewFirstClick, g_viewSecondClick };
        g_debugPatchSize = 5;
        g_debugOffsetX = 0;
        g_debugOffsetY = 0;
        g_samplePointsSet = false;
        g_cropRegionSet = false;
        g_cropPatchSize = 10;
        g_cropOffsetX = 0;
        g_cropOffsetY = 0;
        g_refBlackPiece = -1;
        g_refWhitePiece = -1;
        g_refBoardColor1 = -1;
        g_refBoardColor2 = -1;
        g_noBoard = true;
    }
    
    /* CURRENT MODE. */
    ImGui::Separator();
    if (g_isConfiguringSamplePoints || g_isConfiguringCropRegion) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Configuration Mode: ACTIVE");
        ImGui::Text("Sample Points: %d", (int)g_debugSamples.size());
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), (g_hasAnalysisStarted ? "Analysis Mode: ACTIVE" : "Idle"));
        
        if (ImGui::Button("Reset Configuration")) {
            g_isConfiguringSamplePoints = true;
            g_hasAnalysisStarted = false;
            g_debugSamples.clear();
            UpdateDebugSamples();
        }
    }

    /* SAMPLE POINT SETTINGS. */
    // TODO: Implement debug sample grouping, for example: In lichess, one debug sample is not enough to get all of the pieces. 
    // Certain pieces have different colors in certain positions.
    // By using more than one debug sample groups, if tghe spot at the debug sample does not equal nor white nor black reference values:
    // - We move to the next debug sample for that cell, doing so until we either find a match for black or white reference values.
    ImGui::BeginDisabled(g_samplePointsSet || g_userScreenshotGray.empty() || !g_boardClicksReady);
    ImGui::Separator();
    ImGui::Text("Sample Point Parameters");
    ImGui::Text("Adjust the sliders below to position sample points.");
    
    static int prevPatchSize = g_debugPatchSize;
    static int prevOffsetX = g_debugOffsetX;
    static int prevOffsetY = g_debugOffsetY;

    ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
    ImGui::SliderInt("X Offset", &g_debugOffsetX, -64, 64);
    ImGui::SliderInt("Y Offset", &g_debugOffsetY, -64, 64);
    if (ImGui::Button("Set Sample Points")) {
        UpdateDebugSamples();
        DetectPieceColorCoding((g_boardRect.right - g_boardRect.left) / 8, (g_boardRect.bottom - g_boardRect.top) / 8);
        g_samplePointsSet = true;
        g_isConfiguringCropRegion = true;
        UpdateCropRects();
    }
    
    if (!g_samplePointsSet && (prevPatchSize != g_debugPatchSize || prevOffsetX != g_debugOffsetX || prevOffsetY != g_debugOffsetY)) {
        UpdateDebugSamples();
        prevPatchSize = g_debugPatchSize;
        prevOffsetX = g_debugOffsetX;
        prevOffsetY = g_debugOffsetY;
    }

    ImGui::EndDisabled();

    /* CROP REGION SETTINGS. */
    static int prevCropPatch = g_cropPatchSize;
    static int prevCropOffX = g_cropOffsetX;
    static int prevCropOffY = g_cropOffsetY;

    ImGui::Separator();
    ImGui::Text("Crop Region Parameters");
    ImGui::BeginDisabled(!g_samplePointsSet || g_cropRegionSet);
    ImGui::SliderInt("Crop Size", &g_cropPatchSize, 1, 64);
    ImGui::SliderInt("Crop X Offset", &g_cropOffsetX, -64, 64);
    ImGui::SliderInt("Crop Y Offset", &g_cropOffsetY, -64, 64);

    if (!g_cropRegionSet && g_isConfiguringCropRegion && (prevCropPatch != g_cropPatchSize || prevCropOffX != g_cropOffsetX || prevCropOffY != g_cropOffsetY)) {
        UpdateCropRects();
        prevCropPatch = g_cropPatchSize;
        prevCropOffX = g_cropOffsetX;
        prevCropOffY = g_cropOffsetY;
    }

    if (ImGui::Button("Set Crop Region")) {
        UpdateCropRects();
        g_cropRegionSet = true;
        g_isConfiguringCropRegion = false;
        int cellW = (g_boardRect.right - g_boardRect.left) / 8;
        int cellH = (g_boardRect.bottom - g_boardRect.top) / 8;
        cv::Mat src = g_userScreenshotReady && !g_userScreenshotGray.empty() ? g_userScreenshotGray : HWND2MAT(GetDesktopWindow());
        GenerateReferencePieceCrops(src, cellW, cellH);
        g_hasAnalysisStarted = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::Text("Analysis Settings");
    ImGui::SliderInt("Tolerance", &g_analysisTolerance, 1, 64);

    ImGui::Separator();
    ImGui::Text("Reference Values");
    ImGui::Text("Black Piece Intensity: %d", g_refBlackPiece);
    ImGui::Text("White Piece Intensity: %d", g_refWhitePiece);
    ImGui::Text("Board Color 1: %d", g_refBoardColor1);
    ImGui::Text("Board Color 2: %d", g_refBoardColor2);
    
    if (!g_noBoard) {
        /* LIVE-BOARD PREVIEW. */
        const float boardSize = (g_boardRect.right - g_boardRect.left) / 2.0f;

        const float windowPaddingY = ImGui::GetStyle().WindowPadding.y * 2.0f;
        const float headerHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
        const float windowSizeY = boardSize + windowPaddingY + headerHeight;

        const float windowPaddingX = ImGui::GetStyle().WindowPadding.x * 2.0f;
        const float windowSizeX = boardSize + windowPaddingX;

        ImGui::SetNextWindowSize(ImVec2(windowSizeX, 0), ImGuiCond_Always);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize;

        ImU32 darkSquare = IM_COL32(5, 5, 5, 255);
        ImU32 lightSquare = IM_COL32(25, 20, 20, 255);

        if (ImGui::Begin("Real-Time Analysis", nullptr, windowFlags)) {
            /* STOCKFISH RELATED. */
            bool stockfishAlive = StockfishIsAlive();

            ImVec4 colAlive = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            ImVec4 colDead = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

            ImGui::Text("Stockfish is: ");
            ImGui::SameLine();
            if (stockfishAlive) {
                ImGui::TextColored(colAlive, "ALIVE");
            }
            else {
                ImGui::TextColored(colDead, "DEAD");
            }

            ImGui::SliderInt("Engine ELO", &g_sfElo, 100, 4000);
            ImGui::SliderInt("Engine Move Depth", &g_sfMoveDepth, 1, 30);
            ImGui::SliderInt("Number of Moves", &g_sfNumberMoves, 1, 10);

            ImGui::Text("Play As:");

            if (ImGui::RadioButton("Play as White", g_sfPlayWhite == 1)) {
                g_sfPlayWhite = 1;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Play as Black", g_sfPlayWhite == 0)) {
                g_sfPlayWhite = 0;
            }

            ImGui::Separator();
            ImGui::Text("Real-Time Board");

            ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders
                | ImGuiTableFlags_SizingStretchSame;

            if (ImGui::BeginTable("ChessBoard", 8, tableFlags,
                ImVec2(boardSize, boardSize))) {
                ImGui::PushFont(CHESSBOARD_FONT);

                // Stretch all 8 columns equally.
                for (int col = 0; col < 8; col++) {
                    ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
                }

                const float cellSize = boardSize / 8.0f;

                for (int row = 0; row < 8; row++) {
                    ImGui::TableNextRow(0, cellSize);
                    for (int col = 0; col < 8; col++) {
                        ImGui::TableSetColumnIndex(col);

                        ImU32 bgColor = ((row + col) % 2 == 0) ? lightSquare : darkSquare;
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, bgColor);

                        ImVec2 cellMin = ImGui::GetCursorScreenPos();

                        char piece = ' ';
                        if (row < (int)g_boardGridRows.size() && col < (int)g_boardGridRows[row].size()) {
                            piece = g_boardGridRows[row][col];
                        }

                        std::string symbol = PieceToUnicode(piece);

                        ImVec2 textSize = ImGui::CalcTextSize(symbol.c_str());

                        ImVec2 textPos = {
                            (cellMin.x + cellSize * 0.5f) - (textSize.x * 0.65f),
                            (cellMin.y + cellSize * 0.5f) - (textSize.y * 0.65f)
                        };

                        ImGui::GetWindowDrawList()->AddText(
                            textPos, ImGui::GetColorU32(ImGuiCol_Text), symbol.c_str()
                        );
                    }
                }

                ImGui::EndTable();
                ImGui::PopFont();
            }

            /* STOCKFISH REAL-TIME MOVES. */
            // TODO: The user may want to change Stockfish settings mid move, if so, it should re-render.
            if (stockfishAlive) {
                std::string fen = BoardToFEN();
                static std::string prevFen;
                static std::vector<StockfishMove> prevMoves;

                if (fen != prevFen) {
                    prevMoves = GetBestMoves(fen, g_sfPlayWhite, g_sfElo, g_sfNumberMoves, g_sfMoveDepth);
                    prevFen = fen;
                }

                ImGui::Separator();
                std::stringstream topMovesText;
                topMovesText << "Top " << g_sfNumberMoves << " moves:" << std::endl;
                ImGui::Text(topMovesText.str().c_str());

                for (size_t i = 0; i < prevMoves.size(); ++i) {
                    const auto& mv = prevMoves[i];
                    if (mv.mate) {
                        ImGui::Text("%d. %s (mate in %d)", (int)i + 1, mv.uci.c_str(), mv.mateIn);
                    }
                    else {
                        ImGui::Text("%d. %s (score %d cp)", (int)i + 1, mv.uci.c_str(), mv.scoreCp);
                    }
                }
            }
        }
        ImGui::End();
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

    DEFAULT_FONT = io.Fonts->AddFontDefault();
    CHESSBOARD_FONT = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", 20.0f);
    io.FontDefault = DEFAULT_FONT;
}

void CleanupImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}