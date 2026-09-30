#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#include "vision/BoardDetection.h"
#include "vision/ChessboardDetection.h"
#include "platform/Utils.h"
#include "Globals.h"
#include "Structs.h"
#include "platform/Overlay.h"
#include "vision/InitialConfiguration.h"
#include "engine/StockfishHandler.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// The menu's three tabs.
enum class MenuTab {
    Play,       // Setting up, then the game: evaluation, board and best moves.
    Engine,     // Which engine, how deep it searches, how strong it plays.
    Calibrate,  // How the board is read, and the manual fallback.
};

/**
 * @brief Draws the menu: one panel with a header and the three tabs.
 *
 * @param imageWidth Unused; kept for the render loop's call.
 * @param imageHeight Unused; kept for the render loop's call.
 */
void ShowMenu(int imageWidth, int imageHeight);

/**
 * @brief Whether the menu's own hide button was pressed since last asked.
 *
 * The render loop owns showing and hiding, since hiding also makes the overlay
 * click-through again, so the button asks rather than doing it.
 */
bool ConsumeMenuHideRequest();

/// Opens the menu on a given tab.
void SelectMenuTab(MenuTab tab);

/**
 * @brief Initializes ImGui with Win32 and DirectX11 backends.
 * 
 * @param hwndOverlay Handle to the overlay window.
 * @param device Pointer to the DirectX11 device.
 * @param deviceContext Pointer to the DirectX11 device context.
 * 
 * @return None.
 */
void InitializeImGui(HWND hwndOverlay, ID3D11Device* device, ID3D11DeviceContext* deviceContext);

/**
 * @brief Applies Oracle's style and loads its fonts into the current context.
 *
 * Separate from InitializeImGui so something other than the overlay, such as
 * the offscreen preview in tools/UiPreview, can dress a context of its own the
 * same way without creating a window.
 */
void SetupImGuiStyleAndFonts();

/**
 * @brief Cleans up ImGui resources and shuts down backends.
 * 
 * @param None.
 * 
 * @return None.
 */
void CleanupImGui();