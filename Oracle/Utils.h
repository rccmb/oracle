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
 * @brief Captures the client area of a window and converts it to an OpenCV Mat image.
 *
 * @param hwnd Handle to the window whose client area is to be captured.
 * 
 * @return cv::Mat An OpenCV matrix containing the window's client area pixel data in grayscale.
 */
cv::Mat HWND2MAT(HWND hwnd);

/**
 * @brief Captures the client area of a window and converts it to an OpenCV Mat image. Crops the image to the specified rectangle.
 *
 * @param hwnd Handle to the window whose client area is to be captured.
 * @param x The x-coordinate of the top-left corner of the cropping rectangle.
 * @param y The y-coordinate of the top-left corner of the cropping rectangle.
 * @param width The width of the cropping rectangle.
 * @param height The height of the cropping rectangle.
 *
 * @return cv::Mat An OpenCV matrix containing the window's client area pixel data in grayscale.
 */
cv::Mat CropHWND2MAT(HWND hwnd, int x, int y, int width, int height);
