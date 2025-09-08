#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

#define WM_CHESSBOARD_FOUND (WM_USER + 1)
#define WM_CHESSBOARD_CANDIDATES (WM_USER + 2)

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
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Creates and displays a transparent overlay window spanning the entire screen.
 *
 * @param hInstance Handle to the application instance.
 * @param className Name of the window class to register.
 * @param wndProc Pointer to the window procedure function (e.g., WndProc).
 * 
 * @return HWND Handle to the created window, or NULL if creation fails.
 */
HWND CreateOverlayWindow(HINSTANCE hInstance, const LPCWSTR className, WNDPROC wndProc);

/**
 * @brief Clears all stored candidate rectangles from the candidate list.
 */
void ClearCandidateRectangles();

/**
 * @brief Adds a candidate rectangle to the list of potential chessboard detections.
 * 
 * @param rect The bounding rectangle (in screen coordinates) to add as a candidate.
 */
void AddCandidateRectangle(RECT rect);

/**
 * @brief Clears the currently stored best (validated) rectangle.
 */
void ClearBestRectangle();

/**
 * @brief Sets the best (validated) rectangle for the current detection.
 * 
 * @param rect The validated bounding rectangle (in screen coordinates) to set as the best detection.
 */
void SetBestRectangle(RECT rect);