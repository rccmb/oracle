#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#define WM_CHESSBOARD_DETECTED (WM_USER + 1)

// ImGui parameters for debugging.
extern int g_debugPatchSize;
extern int g_debugOffset;

/**
 * @brief Window procedure to handle messages for the overlay window.
 *
 * @param hwnd Handle to the window receiving the message.
 * @param msg The message identifier.
 * @param wParam Additional message-specific information.
 * @param lParam Additional message-specific information.
 * 
 * @return LRESULT The result of message processing, typically 0 for handled messages, or the result of DefWindowProc for unhandled messages.
 */
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Creates and displays a transparent overlay window spanning the entire screen.
 *
 * @param hInstance Handle to the application instance.
 * @param className Name of the window class to register.
 * 
 * @return HWND Handle to the created window, or NULL if creation fails.
 */
HWND CreateOverlayWindow(HINSTANCE hInstance, const LPCWSTR className);