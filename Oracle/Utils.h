#pragma once

#include <windows.h>
#include <windowsx.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include "Globals.h"
#include "Structs.h"

/**
 * @brief Captures the client area of a window and converts it to an OpenCV Mat image. COLOR.
 *
 * @param hwnd Handle to the window whose client area is to be captured.
 * 
 * @return cv::Mat An OpenCV matrix containing the window's client area pixel data in grayscale.
 */
cv::Mat HWND2MAT(HWND hwnd);

// TODO: Documentation.
std::string PieceToUnicode(char piece);

// TODO: Documentation.
cv::Mat ApplyPaletteMasking(cv::Mat bgr);
