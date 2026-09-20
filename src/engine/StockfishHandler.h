#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <windows.h>

#include "Globals.h"

extern HANDLE g_sfInput;
extern HANDLE g_sfOutput;
extern std::mutex g_sfMutex;
extern bool g_sfRunning;

extern int g_sfElo;
extern bool g_sfPlayWhite;
extern bool g_sfLimitStrength;

// True when the last analysed position was checkmate or stalemate: legal, but
// with no move to make.
extern bool g_sfNoLegalMoves;
extern int g_sfMoveDepth;
extern int g_sfNumberMoves;

// StockfishMove is now in Structs.h

// TODO: Documentation.
void LaunchStockfish(const std::string& path);

// TODO: Documentation.
bool StockfishIsAlive();

// TODO: Documentation.
std::vector<StockfishMove> GetBestMoves(const std::string& fen, int elo, int topN, int depth);

// TODO: Documentation.
void ShutdownStockfish();