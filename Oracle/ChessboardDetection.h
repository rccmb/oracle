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
#include <fstream>

#include "Utils.h"
#include "Overlay.h"
#include "Structs.h"
#include "Globals.h"
#include "InitialConfiguration.h"
#include "StockfishHandler.h"
#include "FileHandler.h"
#include "GameTracker.h"

// TODO: Documentation.
double SampleCellCenter(const cv::Mat& gray, int x, int y, int offsetX, int offsetY);

// TODO: Documentation.
double CompareEdges(const cv::Mat& a, const cv::Mat& b);

/**
 * @brief Re-orders a detected grid into FEN order and rejects impossible boards.
 *
 * @param rows        Eight rows as they appear on screen, top row first.
 * @param orientation 0 when white is at the bottom, 1 when it is at the top.
 * @param out         Receives 64 squares in FEN reading order, a8 first.
 *
 * @return false when the board cannot be a real position, which means a square
 *         was misread and the frame should be dropped rather than believed.
 */
bool BuildObservedBoard(const std::vector<std::string>& rows, int orientation, char out[64]);

// TODO: Documentation.
std::string BoardToFEN();

// TODO: Documentation.
DWORD WINAPI ChessboardDetectionThread(LPVOID param);