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

extern RECT g_boardRect;

// Bounding box of the whole virtual desktop in physical pixels, spanning every
// monitor. The overlay window is placed here and every screen capture starts at
// this origin, so capture-image coordinates and overlay client coordinates are
// the same space. Screen coordinates from GetCursorPos are offset by .left/.top.
extern RECT g_virtualScreen;

extern std::vector<SAMPLE> g_debugSamples;
extern std::vector<SAMPLE> g_cropRects;

extern bool g_isConfiguringSamplePoints;
extern bool g_isConfiguringCropRegion;
extern bool g_cropRegionSet;
extern bool g_hasAnalysisStarted;
extern bool g_isRescanning;

extern std::vector<StockfishMove> g_sfBestMoves;
extern bool g_boardClicksReady;

extern int g_clickStage;
extern int g_debugPatchSize;
extern int g_debugOffsetX;
extern int g_debugOffsetY;
extern int g_debugOffsetX2;
extern int g_debugOffsetY2;
extern int g_debugOffsetX3;
extern int g_debugOffsetY3;
extern float g_matchThreshold;
extern int g_cropPatchSize;
extern int g_cropOffsetX;
extern int g_cropOffsetY;

extern std::pair<CLICK, CLICK> g_clicks;
extern CLICK g_viewFirstClick;
extern CLICK g_viewSecondClick;

extern cv::Mat g_userScreenshotGray;
extern cv::Mat g_userScreenshotColor;
extern bool g_userScreenshotReady;
extern bool g_samplePointsSet;
extern bool g_cropRegionSet;

// Reference colors in Grayscale.
extern int g_refBlackPiece;
extern int g_refWhitePiece;
extern int g_refBoardColor1;
extern int g_refBoardColor2;

// Reference colors in BGR.
extern cv::Vec3b g_refBlackPieceColor;
extern cv::Vec3b g_refWhitePieceColor;
extern cv::Vec3b g_refBoardColor1Color;
extern cv::Vec3b g_refBoardColor2Color;

extern int g_analysisTolerance;
extern std::vector<char> g_detectedLetters;
extern std::vector<std::string> g_boardGridRows;

extern int g_orientation;

extern std::filesystem::path g_tempDir;

extern std::vector<char> g_letterDrawQueue;
extern std::vector<char> g_prevLetterDrawQueue;

extern bool g_boardChanged;
extern std::mutex g_boardChangedMutex;

extern bool g_noBoard;

extern std::string g_lastValidFEN;

// Detected side to move ('w' or 'b').
extern char g_sideToMove;