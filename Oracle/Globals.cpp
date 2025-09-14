#include "Globals.h"

RECT g_boardRect = { 0, 0, 0, 0 };

std::vector<SAMPLE> g_debugSamples = {};
std::vector<SAMPLE> g_userSamplePoints = {};

bool g_isConfiguringSamplePoints = false;
bool g_hasAnalysisStarted = false;
bool g_isRescanning = false;
bool g_boardClicksReady = false;

int g_clickStage = 0;
int g_debugPatchSize = 0;
int g_debugOffset = 0;

std::pair<CLICK, CLICK> g_clicks = {
    {-1, -1, 0},
    {-1, -1, 0}
};

CLICK g_firstClick = { -1, -1, 0 };
CLICK g_secondClick = { -1, -1, 0 };