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
extern int g_sfMoveDepth;
extern int g_sfNumberMoves;

// TODO: Documentation.
struct StockfishMove {
    std::string uci;   // Move in UCI format.
    int scoreCp;       // Score in centipawns.
    bool mate;         // True if this is a mate score.
    int mateIn;        // Number of moves to mate.
};

// TODO: Documentation.
void LaunchStockfish(const std::string& path);

// TODO: Documentation.
bool StockfishIsAlive();

// TODO: Documentation.
std::vector<StockfishMove> GetBestMoves(const std::string& fen, int elo, int topN, int depth);

// TODO: Documentation.
void ShutdownStockfish();

// TODO: Documentation.
bool IsFENValidWithStockfish(const std::string& fen);