#include "Menu.h"

#include <cstdio>

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

    if (!show_window) {
        ImGui::End();
        PostQuitMessage(0);
        return;
    }

    /* AUTOMATIC SETUP. */
    static std::string autoSetupStatus;
    static bool autoSetupFailed = false;

    ImGui::Text("Automatic Setup");
    ImGui::TextWrapped("Show the board at the starting position, then press Detect. "
                       "Reference pieces are read from the starting rows, so the board "
                       "must be untouched.");

    if (ImGui::Button("Detect Board")) {
        autoSetupStatus.clear();
        autoSetupFailed = false;

        cv::Mat capture = CaptureVirtualScreen();
        std::optional<BoardCandidate> board = capture.empty()
            ? std::nullopt
            : DetectChessboard(capture);

        cv::Vec3b darkPiece, lightPiece;
        int orientation = -1;

        if (!board) {
            autoSetupFailed = true;
            autoSetupStatus = "No board found. Make sure it is fully visible, then retry "
                              "or use manual calibration below.";
        }
        else if (!EstimatePieceColors(capture, *board, darkPiece, lightPiece, orientation)) {
            autoSetupFailed = true;
            autoSetupStatus = "Board found, but the pieces could not be read. Reset to the "
                              "starting position and retry.";
        }
        else {
            g_userScreenshotColor = capture;
            cv::cvtColor(g_userScreenshotColor, g_userScreenshotGray, cv::COLOR_BGR2GRAY);
            g_userScreenshotReady = true;

            g_boardRect = { board->rect.x, board->rect.y,
                            board->rect.x + board->rect.width,
                            board->rect.y + board->rect.height };

            g_refBoardColor1Color = board->lightSquare;
            g_refBoardColor2Color = board->darkSquare;
            g_refBoardColor1 = LuminanceOf(board->lightSquare);
            g_refBoardColor2 = LuminanceOf(board->darkSquare);

            g_refBlackPieceColor = darkPiece;
            g_refWhitePieceColor = lightPiece;
            g_refBlackPiece = LuminanceOf(darkPiece);
            g_refWhitePiece = LuminanceOf(lightPiece);
            g_orientation = orientation;

            ApplyDerivedSampleGeometry(*board);
            UpdateDebugSamples();
            UpdateCropRects();

            GenerateReferencePieceCrops(g_userScreenshotColor, board->cellSize, board->cellSize);

            // Everything the manual flow would have asked for is now known, so
            // skip straight past its stages.
            g_boardClicksReady = true;
            g_clickStage = 2;
            g_samplePointsSet = true;
            g_cropRegionSet = true;
            g_isConfiguringSamplePoints = false;
            g_isConfiguringCropRegion = false;
            g_isRescanning = false;
            g_noBoard = false;
            g_hasAnalysisStarted = true;

            char summary[160];
            std::snprintf(summary, sizeof(summary),
                "Board at %dx%d, %d px squares, %s at the bottom. Analysis running.",
                board->rect.width, board->rect.height, board->cellSize,
                orientation == 0 ? "white" : "black");
            autoSetupStatus = summary;
        }
    }

    if (!autoSetupStatus.empty()) {
        ImGui::TextWrapped("%s", autoSetupStatus.c_str());
        if (autoSetupFailed) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Detection failed.");
        }
        else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Detection succeeded.");
        }
    }

    ImGui::Separator();

    /* MANUAL CALIBRATION, kept as a fallback for boards automatic setup cannot read. */
    if (ImGui::CollapsingHeader("Manual calibration")) {
        /* SET USER SCREENSHOT. */
        if (!g_userScreenshotReady) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "You must take a screenshot before setting clicks.");
            if (ImGui::Button("Take Screenshot")) {
                g_userScreenshotColor = CaptureVirtualScreen();
                if (!g_userScreenshotColor.empty()) {
                    cv::cvtColor(g_userScreenshotColor, g_userScreenshotGray, cv::COLOR_BGR2GRAY);
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
                if (ImGui::Button("Detect From Clicks")) {
                    g_boardClicksReady = true;
    				g_clicks = { g_viewFirstClick, g_viewSecondClick };
                    DetectBoardDimensions();
                    g_isRescanning = false;
                    g_isConfiguringSamplePoints = true;
                    g_samplePointsSet = false;
                    g_refBoardColor1 = (int)g_clicks.first.grayscaleValue;
                    g_refBoardColor2 = (int)g_clicks.second.grayscaleValue;
                
                    if (!g_userScreenshotColor.empty()) {
                        if (g_viewFirstClick.y >= 0 && g_viewFirstClick.y < g_userScreenshotColor.rows &&
                            g_viewFirstClick.x >= 0 && g_viewFirstClick.x < g_userScreenshotColor.cols) {
                            g_refBoardColor1Color = g_userScreenshotColor.at<cv::Vec3b>(g_viewFirstClick.y, g_viewFirstClick.x);
                        }
                        if (g_viewSecondClick.y >= 0 && g_viewSecondClick.y < g_userScreenshotColor.rows &&
                            g_viewSecondClick.x >= 0 && g_viewSecondClick.x < g_userScreenshotColor.cols) {
                            g_refBoardColor2Color = g_userScreenshotColor.at<cv::Vec3b>(g_viewSecondClick.y, g_viewSecondClick.x);
                        }
                    }
                }
                ImGui::Text("First: (%d, %d)  Second: (%d, %d)", g_viewFirstClick.x, g_viewFirstClick.y, g_viewSecondClick.x, g_viewSecondClick.y);
            }
            ImGui::Separator();
        }
    
    }

    /* CONFIGURATION RESET. */
    if (ImGui::Button("Rescan Board")) {
        autoSetupStatus.clear();
        autoSetupFailed = false;
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
        g_prevLetterDrawQueue.clear();
        {
            // Cleared from the render thread while the detection thread may be
            // mid publish, so this takes the same lock the publisher does.
            std::lock_guard<std::mutex> reset(g_analysisStateMutex);
            g_detectedLetters.assign(64, ' ');
            g_boardGridRows.assign(8, std::string(8, ' '));
            g_sfBestMoves.clear();
        }
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

    /* SAMPLING GEOMETRY. Derived from the detected cell size; exposed for tuning. */
    if (ImGui::CollapsingHeader("Advanced: sampling geometry")) {
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
        static int prevOffsetX2 = g_debugOffsetX2;
        static int prevOffsetY2 = g_debugOffsetY2;
        static int prevOffsetX3 = g_debugOffsetX3;
        static int prevOffsetY3 = g_debugOffsetY3;

        ImGui::SliderInt("Patch Size", &g_debugPatchSize, 1, 64);
        ImGui::SliderInt("Point 1 X Offset", &g_debugOffsetX, -64, 64);
        ImGui::SliderInt("Point 1 Y Offset", &g_debugOffsetY, -64, 64);
        ImGui::SliderInt("Point 2 X Offset", &g_debugOffsetX2, -64, 64);
        ImGui::SliderInt("Point 2 Y Offset", &g_debugOffsetY2, -64, 64);
        ImGui::SliderInt("Point 3 X Offset", &g_debugOffsetX3, -64, 64);
        ImGui::SliderInt("Point 3 Y Offset", &g_debugOffsetY3, -64, 64);
        if (ImGui::Button("Set Sample Points")) {
            UpdateDebugSamples();
            DetectPieceColorCoding((g_boardRect.right - g_boardRect.left) / 8, (g_boardRect.bottom - g_boardRect.top) / 8);
            g_samplePointsSet = true;
            g_isConfiguringCropRegion = true;
            UpdateCropRects();
        }
    
        if (!g_samplePointsSet && (prevPatchSize != g_debugPatchSize || prevOffsetX != g_debugOffsetX || prevOffsetY != g_debugOffsetY ||
                                   prevOffsetX2 != g_debugOffsetX2 || prevOffsetY2 != g_debugOffsetY2 ||
                                   prevOffsetX3 != g_debugOffsetX3 || prevOffsetY3 != g_debugOffsetY3)) {
            UpdateDebugSamples();
            prevPatchSize = g_debugPatchSize;
            prevOffsetX = g_debugOffsetX;
            prevOffsetY = g_debugOffsetY;
            prevOffsetX2 = g_debugOffsetX2;
            prevOffsetY2 = g_debugOffsetY2;
            prevOffsetX3 = g_debugOffsetX3;
            prevOffsetY3 = g_debugOffsetY3;
        }

        ImGui::EndDisabled();

        /* CROP REGION SETTINGS. */
        static int prevCropPatch = g_cropPatchSize;
        static int prevCropOffX = g_cropOffsetX;
        static int prevCropOffY = g_cropOffsetY;

        ImGui::Separator();
        ImGui::Text("Crop Region Parameters");
        ImGui::BeginDisabled(!g_samplePointsSet || g_cropRegionSet);
    
        int maxCropSize = 64;
        if ((g_boardRect.right - g_boardRect.left) > 0) {
            maxCropSize = (g_boardRect.right - g_boardRect.left) / 8;
        }
        if (g_cropPatchSize > maxCropSize) {
            g_cropPatchSize = maxCropSize;
        }

        ImGui::SliderInt("Crop Size", &g_cropPatchSize, 1, maxCropSize);
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
            cv::Mat srcColor = g_userScreenshotReady && !g_userScreenshotColor.empty() ? g_userScreenshotColor : CaptureVirtualScreen();
            GenerateReferencePieceCrops(srcColor, cellW, cellH);
            g_hasAnalysisStarted = true;
        }
        ImGui::EndDisabled();

    }

    ImGui::Separator();
    ImGui::Text("Analysis Settings");
    ImGui::SliderInt("Tolerance", &g_analysisTolerance, 1, 64);

    ImGui::BeginDisabled(!g_samplePointsSet || !g_cropRegionSet);
    ImGui::Separator();
    ImGui::Text("Reference Values");

    if (ImGui::BeginTable("ref_table", 2, ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Grayscale", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Black Piece: %d", g_refBlackPiece);
        ImGui::TableSetColumnIndex(1);
        ImGui::ColorButton("##black_piece_color",
            ImVec4(g_refBlackPieceColor[2] / 255.0f,
                g_refBlackPieceColor[1] / 255.0f,
                g_refBlackPieceColor[0] / 255.0f,
                1.0f),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
            ImVec2(40, 20));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("White Piece: %d", g_refWhitePiece);
        ImGui::TableSetColumnIndex(1);
        ImGui::ColorButton("##white_piece_color",
            ImVec4(g_refWhitePieceColor[2] / 255.0f,
                g_refWhitePieceColor[1] / 255.0f,
                g_refWhitePieceColor[0] / 255.0f,
                1.0f),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
            ImVec2(40, 20));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Board Color 1: %d", g_refBoardColor1);
        ImGui::TableSetColumnIndex(1);
        ImGui::ColorButton("##board_color1",
            ImVec4(g_refBoardColor1Color[2] / 255.0f,
                g_refBoardColor1Color[1] / 255.0f,
                g_refBoardColor1Color[0] / 255.0f,
                1.0f),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
            ImVec2(40, 20));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Board Color 2: %d", g_refBoardColor2);
        ImGui::TableSetColumnIndex(1);
        ImGui::ColorButton("##board_color2",
            ImVec4(g_refBoardColor2Color[2] / 255.0f,
                g_refBoardColor2Color[1] / 255.0f,
                g_refBoardColor2Color[0] / 255.0f,
                1.0f),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
            ImVec2(40, 20));

        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    
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
            // One snapshot of what the detection thread has produced, taken before
            // anything is drawn. Reading these directly would race with the thread
            // reallocating them mid frame.
            std::vector<std::string> boardRows;
            std::vector<StockfishMove> bestMoves;
            {
                std::lock_guard<std::mutex> snapshot(g_analysisStateMutex);
                boardRows = g_boardGridRows;
                bestMoves = g_sfBestMoves;
            }

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

            ImGui::Checkbox("Limit engine strength", &g_sfLimitStrength);
            ImGui::BeginDisabled(!g_sfLimitStrength);
            ImGui::SliderInt("Engine ELO", &g_sfElo, 1320, 3190);
            ImGui::EndDisabled();
            ImGui::SliderInt("Engine Move Depth", &g_sfMoveDepth, 1, 30);
            ImGui::SliderInt("Number of Moves", &g_sfNumberMoves, 1, 10);
            ImGui::SliderFloat("Match Threshold", &g_matchThreshold, 0.10f, 1.0f, "%.2f");

            ImGui::Text("Play As:");

            if (ImGui::RadioButton("Playing as White", g_sfPlayWhite == 1)) {
                g_sfPlayWhite = 1;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Playing as Black", g_sfPlayWhite == 0)) {
                g_sfPlayWhite = 0;
            }

            ImGui::Separator();
            // TODO: Implement board evaluation.

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
                        if (row < (int)boardRows.size() && col < (int)boardRows[row].size()) {
                            piece = boardRows[row][col];
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
                ImGui::Separator();

                // TODO: This isn't actually doing anything interesting.
                auto normalizeScore = [](int scoreCp) {
                    return g_sfPlayWhite ? scoreCp : -scoreCp;
                    };

                std::vector<StockfishMove> greenMoves; 
                std::vector<StockfishMove> yellowMoves; 
                std::vector<StockfishMove> redMoves;    

                for (const auto& mv : bestMoves) {
                    if (mv.mate) {
                        greenMoves.push_back(mv); 
                        continue;
                    }

                    int userScore = normalizeScore(mv.scoreCp);
                    if (userScore > 0) {
                        greenMoves.push_back(mv);
                    }
                    else if (userScore == 0) {
                        yellowMoves.push_back(mv);
                    }
                    else {
                        redMoves.push_back(mv);
                    }
                }

                ImGuiTableFlags mvTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;
                if (ImGui::BeginTable("MoveQuality", 3, mvTableFlags, ImVec2(boardSize, 0))) {
                    ImGui::TableSetupColumn("Advantage");
                    ImGui::TableSetupColumn("Balanced");  
                    ImGui::TableSetupColumn("Disadvantage"); 
                    ImGui::TableHeadersRow();
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    for (const auto& mv : greenMoves) {
                        if (mv.mate)
                            ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "%s (mate in %d)", mv.uci.c_str(), mv.mateIn);
                        else
                            ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "%s (%d cp)", mv.uci.c_str(), mv.scoreCp);
                    }

                    ImGui::TableSetColumnIndex(1);
                    for (const auto& mv : yellowMoves) {
                        if (mv.mate)
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "%s (mate in %d)", mv.uci.c_str(), mv.mateIn);
                        else
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "%s (%d cp)", mv.uci.c_str(), mv.scoreCp);
                    }

                    ImGui::TableSetColumnIndex(2);
                    for (const auto& mv : redMoves) {
                        if (mv.mate)
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s (mate in %d)", mv.uci.c_str(), mv.mateIn);
                        else
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s (%d cp)", mv.uci.c_str(), mv.scoreCp);
                    }

                    ImGui::EndTable();
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