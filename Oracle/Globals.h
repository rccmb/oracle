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

extern std::vector<SAMPLE> g_debugSamples;
extern std::vector<SAMPLE> g_cropRects;

extern bool g_isConfiguringSamplePoints;
extern bool g_isConfiguringCropRegion;
extern bool g_hasAnalysisStarted;
extern bool g_isRescanning;
extern bool g_boardClicksReady;

extern int g_clickStage;
extern int g_debugPatchSize;
extern int g_debugOffsetX;
extern int g_debugOffsetY;
extern int g_cropPatchSize;
extern int g_cropOffsetX;
extern int g_cropOffsetY;

extern std::pair<CLICK, CLICK> g_clicks;
extern CLICK g_viewFirstClick;
extern CLICK g_viewSecondClick;

extern cv::Mat g_userScreenshotGray;
extern bool g_userScreenshotReady;
extern bool g_samplePointsSet;
extern bool g_cropRegionSet;

extern int g_refBlackPiece;
extern int g_refWhitePiece;
extern int g_refBoardColor1;
extern int g_refBoardColor2;

extern int g_analysisTolerance;
extern std::vector<char> g_detectedLetters;
extern std::vector<std::string> g_boardGridRows;

extern int g_orientation;

// Paths
extern std::filesystem::path g_tempDir;