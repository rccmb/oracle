#pragma once

#include <windows.h>
#include <opencv2/core.hpp>

/**
 * @brief Captures the client area of a window and converts it to an OpenCV Mat image.
 *
 * @param hwnd Handle to the window whose client area is to be captured.
 * @return cv::Mat An OpenCV matrix containing the window's client area pixel data in RGBA format (CV_8UC4).
 */
cv::Mat hwnd2mat(HWND hwnd);