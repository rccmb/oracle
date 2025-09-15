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

#include "Structs.h"
#include "Globals.h"
#include "Utils.h"

/**
 * @brief Validates if a given rectangle contains a chessboard pattern by checking intensity similarities at grid intersections.
 *
 * @param gray Grayscale image of the screenshot.
 * @param rect Bounding rectangle of the candidate chessboard.
 * @param debugImg Output image for debug visualizations.
 *
 * @return A Rect containing the detected chessboard area if valid, or std::nullopt if not valid.
 */
std::optional<cv::Rect> ValidateChessboard(const cv::Mat& gray, const cv::Rect& roi, cv::Mat& debugImg);

cv::Rect GetBoardROI(const cv::Mat& img);

int DetectBoardDimensions();