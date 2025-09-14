#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#include "ChessboardDetection.h"
#include "Utils.h"
#include "Globals.h"
#include "Structs.h"
#include "Overlay.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

/**
 * @brief Responsible for rendering the ImGui menu with sliders to adjust debugging parameters.
 *
 * @param imageWidth Width of the image for setting slider limits.
 * @param imageHeight Height of the image for setting slider limits.
 */
void ShowMenu(int imageWidth, int imageHeight);

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
 * @brief Cleans up ImGui resources and shuts down backends.
 * 
 * @param None.
 * 
 * @return None.
 */
void CleanupImGui();