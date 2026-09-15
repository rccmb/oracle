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

// TODO: Documentation.
double SampleCellCenter(const cv::Mat& gray, int x, int y, int offsetX, int offsetY);

// TODO: Documentation.
double CompareEdges(const cv::Mat& a, const cv::Mat& b);

// TODO: Documentation.
std::string BoardToFEN();

// TODO: Documentation.
DWORD WINAPI ChessboardDetectionThread(LPVOID param);