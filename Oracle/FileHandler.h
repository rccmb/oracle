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
#include "Structs.h"
#include "Globals.h"

// TODO: Documentation.
cv::Mat LoadWithImdecode(const std::filesystem::path& p);

// TODO: Documentation.
void SaveReferencePiece(const cv::Mat& gray, const cv::Rect& roi, const std::string& path);

// TODO: Documentation.
std::map<std::string, cv::Mat> LoadReferencePieces(const std::filesystem::path& tempDir);