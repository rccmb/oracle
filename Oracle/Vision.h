#pragma once

#include <windows.h>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>

#include "Utils.h"
#include "Overlay.h"

/**
 * @brief Thread function that continuously detects a chessboard on the desktop and sends its position/size to an overlay window.
 * 
 * @param param Pointer to the handle of the overlay window (HWND) where detection results will be sent.
 * @return Always returns 0 as the thread exit code.
 */
DWORD WINAPI ChessboardDetectionThread(LPVOID param);
