#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#define WM_CHESSBOARD_DETECTED (WM_USER + 1)

extern int g_debugROI_x;
extern int g_debugROI_y;
extern int g_debugPatchSize;
extern int g_debugOffset;

struct SAMPLE {
    int x;
    int y;
    int width;
    int height;
};

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

void AddDebugSample(SAMPLE sample);

/**
 * @brief Clears the currently stored best (validated) rectangle.
 */
void ClearboardRectangle();

/**
 * @brief Sets the best (validated) rectangle for the current detection.
 * 
 * @param rect The validated bounding rectangle (in screen coordinates) to set as the best detection.
 */
void SetboardRectangle(RECT rect);