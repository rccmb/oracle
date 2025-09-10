#pragma once

#include <windows.h>
#include <windowsx.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

struct CLICK {
    int x;
    int y;
    uchar grayscaleValue;
};

/**
 * @brief Captures the client area of a window and converts it to an OpenCV Mat image.
 *
 * @param hwnd Handle to the window whose client area is to be captured.
 * 
 * @return cv::Mat An OpenCV matrix containing the window's client area pixel data in grayscale.
 */
cv::Mat HWND2MAT(HWND hwnd);

// TODO: Documentation.
cv::Mat CropHWND2MAT(HWND hwnd, int x, int y, int width, int height);

// TODO: Documentation.
std::pair<CLICK, CLICK> GetChessboardColorCoding(HWND hwnd);