#include "Globals.h"

RECT g_boardRect = { 0, 0, 0, 0 };
RECT g_virtualScreen = { 0, 0, 0, 0 };

std::vector<SAMPLE> g_debugSamples = {};
std::vector<SAMPLE> g_cropRects = {};

bool g_isConfiguringSamplePoints = true;
bool g_isConfiguringCropRegion = false;
bool g_hasAnalysisStarted = false;
bool g_isRescanning = false;

std::vector<StockfishMove> g_sfBestMoves;
bool g_boardClicksReady = false;

int g_clickStage = 0; 

int g_debugPatchSize = 5;
int g_debugOffsetX = 0; 
int g_debugOffsetY = 0; 
int g_debugOffsetX2 = 0;
int g_debugOffsetY2 = 0;
int g_debugOffsetX3 = 0;
int g_debugOffsetY3 = 0;
float g_matchThreshold = 0.70f;
int g_cropPatchSize = 10;
int g_cropOffsetX = 0;
int g_cropOffsetY = 0;

std::pair<CLICK, CLICK> g_clicks = {
    {-1, -1, 0},
    {-1, -1, 0}
};

CLICK g_viewFirstClick = { -1, -1, 0 };
CLICK g_viewSecondClick = { -1, -1, 0 };

cv::Mat g_userScreenshotGray;
cv::Mat g_userScreenshotColor;
bool g_userScreenshotReady = false;
bool g_samplePointsSet = false;
bool g_cropRegionSet = false;

int g_refBlackPiece = -1;
int g_refWhitePiece = -1;
int g_refBoardColor1 = -1;
int g_refBoardColor2 = -1;

cv::Vec3b g_refBlackPieceColor = {0,0,0};
cv::Vec3b g_refWhitePieceColor = {0,0,0};
cv::Vec3b g_refBoardColor1Color = {0,0,0};
cv::Vec3b g_refBoardColor2Color = {0,0,0};

int g_analysisTolerance = 5;
std::vector<char> g_detectedLetters(64, ' ');
std::vector<std::string> g_boardGridRows(8, std::string(8, ' '));
std::filesystem::path g_tempDir = std::filesystem::path("temp");

int g_orientation = -1;

std::vector<char> g_letterDrawQueue;
std::vector<char> g_prevLetterDrawQueue;

bool g_boardChanged;
std::mutex g_boardChangedMutex;

bool g_noBoard = true;

std::string g_lastValidFEN = "";

char g_sideToMove = 'w';