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
#include <atomic>
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

// Colours a site paints over the squares of the last move, of a check, or of a
// hover. Learned rather than configured: after a move is accepted the tracker
// knows exactly which square was vacated, so whatever flat colour is sitting
// there is by definition this board's highlight.
//
// Read from both the detection thread and the interface, hence the lock.
extern std::vector<cv::Vec3b> g_highlightColors;
extern std::mutex g_highlightMutex;
extern std::vector<char> g_detectedLetters;
extern std::vector<std::string> g_boardGridRows;

extern int g_orientation;

extern std::filesystem::path g_tempDir;

extern std::vector<char> g_letterDrawQueue;
extern std::vector<char> g_prevLetterDrawQueue;

extern bool g_boardChanged;
extern std::mutex g_boardChangedMutex;

// Guards the state the detection thread produces and the render thread consumes:
// g_boardGridRows, g_detectedLetters, g_sfBestMoves and g_lastValidFEN. These are
// vectors and strings that the detection thread reallocates, so reading them from
// the render thread without holding this is undefined behaviour, not a stale read.
//
// Held only to copy in or out. Nothing slow happens inside it.
extern std::mutex g_analysisStateMutex;

extern bool g_noBoard;

extern std::string g_lastValidFEN;

// Side to move, taken from the tracked game rather than inferred from pixels.
extern char g_sideToMove;

// The tracked game, published for the interface.
extern std::string g_trackedFen;   // Position the engine is being asked about.
extern std::string g_lastMoveUci;  // Move that produced it, or empty.
extern int g_trackerPly;           // Plies played so far.
extern bool g_trackerInSync;       // False once the board has stopped making sense.
extern bool g_sfMovesAreOurs;      // Whether the suggestions belong to the side being played.

// Set by the interface when the board is detected again, so the thread starts a
// fresh game rather than trying to reconcile the new board against the old one.
extern bool g_trackerResetRequested;

// Whether the overlay draws an arrow for each suggestion beneath its number.
extern bool g_showMoveArrows;

// The evaluation as the search deepens, republished on every completed depth
// rather than once when the search ends. Atomics rather than the analysis mutex:
// these are written from inside the engine read loop, which already holds the
// engine lock, and a second lock taken there would fix an ordering between the
// two for no reason.
//
// Always from White's point of view, so the bar does not have to know whose turn
// it is, and so it reads the same way round for both players.
extern std::atomic<bool> g_liveEvalValid;
extern std::atomic<int> g_liveEvalCpWhite;
extern std::atomic<bool> g_liveEvalIsMate;
extern std::atomic<int> g_liveEvalMateInWhite;  // Positive when White is mating.
extern std::atomic<int> g_liveEvalDepth;

// What the last move cost whoever played it. Empty when the move was sound.
extern std::string g_lastMoveVerdict;      // "Blunder", "Mistake" or "Inaccuracy".
extern std::string g_lastMoveVerdictUci;   // The move being judged.
extern int g_lastMoveLossCp;               // Centipawns lost by the side that moved.
extern bool g_lastMoveVerdictByUs;         // Whether that side is the one being played.