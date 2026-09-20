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
 * @brief Makes the process per-monitor DPI aware and records the virtual screen bounds.
 *
 * Must run before any window is created. Without this the process is virtualised
 * on scaled displays: GetCursorPos and BitBlt report different pixel spaces, so
 * calibration clicks land on the wrong part of the captured image.
 */
void InitializeDisplayMetrics();

/**
 * @brief Captures a rectangle of the desktop in physical pixels.
 *
 * @param x,y     Top-left corner in screen coordinates. May be negative on a
 *                multi-monitor desktop whose secondary display sits left of or
 *                above the primary one.
 * @param width,height Size of the region to capture.
 *
 * @return cv::Mat BGR image of the region, or an empty Mat if capture failed.
 */
cv::Mat CaptureScreenRegion(int x, int y, int width, int height);

/**
 * @brief Captures the entire virtual desktop, across every monitor.
 *
 * The returned image's origin corresponds to g_virtualScreen's top-left, which
 * is the coordinate space every stored rectangle in Oracle uses.
 *
 * @return cv::Mat BGR image of the whole desktop.
 */
cv::Mat CaptureVirtualScreen();

// TODO: Documentation.
std::string PieceToUnicode(char piece);

// TODO: Documentation.
cv::Mat ApplyPaletteMasking(cv::Mat bgr);
