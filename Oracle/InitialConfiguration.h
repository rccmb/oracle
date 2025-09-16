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
#include "FileHandler.h"

/**
 * @brief Records user mouse clicks on the desktop to define two corners of the chessboard.
 *
 * @param hwndDesktop Handle to the desktop window.
 */
void SetBoardClicks(HWND hwndDesktop);

/**
 * @brief Updates the debug sample regions for all 64 chessboard cells based on the current configuration.
 */
void UpdateDebugSamples();

/**
 * @brief Validates if a given rectangle contains a chessboard pattern by checking intensity similarities at grid intersections.
 *
 * @param gray Grayscale image of the screenshot.
 * @param roi ROI of the candidate chessboard area.
 * @param debugImg Output image for debug visualizations.
 *
 * @return A Rect containing the detected chessboard area if valid, or std::nullopt if not valid.
 */
std::optional<cv::Rect> ValidateChessboard(const cv::Mat& gray, const cv::Rect& roi, cv::Mat& debugImg);

/**
 * @brief Detects the grayscale intensity reference values for black and white chess pieces based on sampled regions of the chessboard screenshot.
 *
 * @param screenshot The input image containing the chessboard.
 * @param cellWidth The width of one chessboard cell in pixels.
 * @param cellHeight The height of one chessboard cell in pixels.
 * 
 * @return int Returns 1 if piece color coding was successfully detected, 0 otherwise.
 */
int DetectPieceColorCoding(int cellWidth, int cellHeight);

/**
 * @brief Detects the dimensions of the chessboard in a screenshot or user-provided image.
 *
 * @return int Returns 1 if the chessboard was successfully detected, 0 otherwise.
 */
int DetectBoardDimensions();

// TODO: Documentation.
void UpdateCropRects();

// TODO: Documentation.
void GenerateReferencePieceCrops(const cv::Mat& gray, int cellWidth, int cellHeight);