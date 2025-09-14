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
extern std::vector<SAMPLE> g_userSamplePoints;

extern bool g_isConfiguringSamplePoints;
extern bool g_hasAnalysisStarted;
extern bool g_isRescanning;
extern bool g_boardClicksReady;

extern int g_clickStage;
extern int g_debugPatchSize;
extern int g_debugOffset;

extern std::pair<CLICK, CLICK> g_clicks;
extern CLICK g_firstClick;
extern CLICK g_secondClick;