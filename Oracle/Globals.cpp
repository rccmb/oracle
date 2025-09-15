#include "Globals.h"

RECT g_boardRect = { 0, 0, 0, 0 };

std::vector<SAMPLE> g_debugSamples = {};

bool g_isConfiguringSamplePoints = true;
bool g_hasAnalysisStarted = false;
bool g_isRescanning = false;
bool g_boardClicksReady = false;

// Current click stage, 0 = waiting for first click, 1 = waiting for second click, 2 = ready to detect.
int g_clickStage = 0; 

// Slider values for configuring sample points. WHERE THE COLOR SAMPLES ARE TAKEN FROM EACH CELL.
int g_debugPatchSize = 5;
int g_debugOffsetX = 0; // X offset for sample patch.
int g_debugOffsetY = 0; // Y offset for sample patch.

// For storing the two clicks defining the chessboard corners.
std::pair<CLICK, CLICK> g_clicks = {
    {-1, -1, 0},
    {-1, -1, 0}
};

// For viewing clicks in the menu before confirming. 
CLICK g_viewFirstClick = { -1, -1, 0 };
CLICK g_viewSecondClick = { -1, -1, 0 };

cv::Mat g_userScreenshotGray;
bool g_userScreenshotReady = false;
bool g_samplePointsSet = false;