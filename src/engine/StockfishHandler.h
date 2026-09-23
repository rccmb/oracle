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

// Which engine to run.
//
// Nothing here is specific to Stockfish. Oracle speaks plain UCI over a pipe:
// uci, isready, setoption, position fen, go depth, quit, and it reads back the
// info lines and bestmove. Any engine that speaks that will work, and Stockfish
// is simply what is bundled.
//
// MultiPV is worth supporting, since it is what fills in the ranked suggestions,
// but an engine without it still works and returns one move. UCI_LimitStrength
// and UCI_Elo are only sent when strength limiting is switched on, and an engine
// that does not know them is required by the protocol to ignore them.
extern std::string g_sfEngineRequested;  // What was asked for, as typed or passed.
extern std::string g_sfEngineResolved;   // Where it was actually found on disk.
extern std::string g_sfEngineName;       // The engine's own "id name", once it has spoken.
extern std::string g_sfEngineError;      // Why the last launch failed, if it did.

/**
 * @brief Starts an engine and completes the UCI handshake.
 *
 * The path may be absolute, relative to the working directory, or relative to
 * the executable; it is searched for outward from the executable so that a
 * launch from a debugger, a shortcut or another directory all behave the same.
 *
 * Any engine already running is shut down first, so this doubles as "switch to
 * a different engine".
 *
 * @param path Engine executable. Empty means the bundled Stockfish.
 */
void LaunchStockfish(const std::string& path);

/**
 * @brief Whether the engine is up, restarting it if it has died.
 *
 * Cached between probes: both the render loop and the detection loop ask every
 * pass, and a child process does not stop existing at that rate.
 */
bool StockfishIsAlive();

/**
 * @brief Asks the engine for its best moves in a position.
 *
 * @param fen   Position to search.
 * @param elo   Target strength, used only when limiting is enabled.
 * @param topN  MultiPV width.
 * @param depth Search depth.
 *
 * @return Moves in the engine's own ranking, best first. Empty at checkmate or
 *         stalemate, where g_sfNoLegalMoves is set instead.
 */
std::vector<StockfishMove> GetBestMoves(const std::string& fen, int elo, int topN, int depth);

/**
 * @brief Sends "quit" and releases the engine's handles.
 */
void ShutdownStockfish();