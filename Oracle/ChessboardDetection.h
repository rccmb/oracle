#pragma once

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <optional>
#include <chrono>
#include <vector>
#include <numeric>
#include <string>
#include <filesystem>

#include "Utils.h"
#include "Overlay.h"

struct SAMPLE {
    int x;
    int y;
    int width;
    int height;
};

extern RECT g_boardRect;
extern std::vector<SAMPLE> g_debugSamples;

/**
 * @brief Thread function that continuously detects a chessboard on the desktop and sends its position/size to an overlay window.
 * 
 * @param param Pointer to the handle of the overlay window (HWND) where detection results will be sent.
 * 
 * @return Always returns 0 as the thread exit code.
 */
DWORD WINAPI ChessboardDetectionThread(LPVOID param);

/**
 * @brief Captures two mouse clicks to determine chessboard cell coordinates and grayscale values.
 * 
 * @param hwnd Handle to the window for screenshot capture and coordinate conversion.
 * 
 * @return std::pair<CLICK, CLICK> A pair of CLICK structs, each containing x, y coordinates and the grayscale value of the pixel at the click location.
 */
void SetChessboardClicks(std::pair<CLICK, CLICK> clicks);
