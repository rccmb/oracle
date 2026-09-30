#pragma once

#include <atomic>
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
//
// Written by whichever thread launches the engine. Read them through
// GetEngineStatus from any other thread.
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

// Counts successful launches. The analysis loop searches the position on the
// board again when this changes, so a newly loaded engine answers at once.
extern std::atomic<int> g_sfEngineGeneration;

/**
 * @brief Starts an engine on a background thread and returns at once.
 *
 * What the menu uses. A launch waits for any search in progress to finish and
 * then for the UCI handshake, which together can take seconds, and the menu is
 * drawn by the same thread that presents every frame of the overlay.
 */
void LaunchStockfishAsync(const std::string& path);

/// True while LaunchStockfishAsync is still starting an engine.
bool StockfishIsLoading();

/**
 * @brief Whether the engine is up, restarting it if it has died.
 *
 * Cached between probes, and blocks while a search holds the engine. For the
 * analysis loop; the interface uses StockfishLooksAlive.
 */
bool StockfishIsAlive();

/**
 * @brief Whether the engine is up, without ever waiting.
 *
 * While a search holds the engine the answer is what is already known, since a
 * search only runs on a live engine. Never restarts anything.
 */
bool StockfishLooksAlive();

// A consistent copy of what the engine layer knows about the engine, safe to
// take from any thread.
struct EngineStatus {
    std::string requested;  // As typed or passed.
    std::string resolved;   // Where it was found on disk.
    std::string name;       // Its own "id name", once it has answered.
    std::string error;      // Why the last launch failed, if it did.
    bool loading = false;   // A background launch is still under way.
};

EngineStatus GetEngineStatus();

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