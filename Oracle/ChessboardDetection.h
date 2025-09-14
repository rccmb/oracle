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
#include "Structs.h"
#include "Globals.h"
#include "InitialConfiguration.h"

/** 
 * @brief Thread function that continuously detects a chessboard on the desktop and sends its position/size to an overlay window.
 * 
 * @param param Pointer to the handle of the overlay window (HWND) where detection results will be sent.
 * 
 * @return Always returns 0 as the thread exit code.
 */
DWORD WINAPI ChessboardDetectionThread(LPVOID param);

// TODO: Documentation.
int DetectBoardDimensions(cv::Mat screenshot);

// TODO: Documentation.
int DetectPieceColorCoding(cv::Mat screenshot, int cellWidth, int cellHeight);

// TODO: Documentation.
void UpdateDebugSamples();