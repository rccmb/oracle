#include "StockfishHandler.h"

#include <map>

HANDLE g_sfInput = nullptr;
HANDLE g_sfOutput = nullptr;
std::mutex g_sfMutex;
bool g_sfRunning = false;

std::vector<StockfishMove> g_sfPreviousMoves;

int g_sfElo = 1320;
bool g_sfPlayWhite = true;

// Full strength by default. Oracle used to ship UCI_LimitStrength on at the
// engine's floor of 1320 Elo with a depth of 1, so out of the box it asked a
// deliberately weakened Stockfish for a one ply search.
bool g_sfLimitStrength = false;
int g_sfMoveDepth = 15;
int g_sfNumberMoves = 3;

static PROCESS_INFORMATION g_sfProcInfo = { 0 };
static HANDLE g_sfThread = nullptr;
static bool g_sfStopThread = false;

// Where the engine was launched from, so it can be started again if it dies.
static std::string g_sfPath = "stockfish/stockfish.exe";

// How long a liveness answer stays good for before the engine is probed again.
static constexpr DWORD kLivenessProbeIntervalMs = 2000;

// Minimum gap between restart attempts, so an engine that cannot start is not
// spawned in a tight loop.
static constexpr DWORD kRestartBackoffMs = 3000;

bool g_sfNoLegalMoves = false;

// TODO: Documentation.
static void SendCommand(const std::string& cmd) {
    if (!g_sfInput) return;
    DWORD written = 0;
    WriteFile(g_sfInput, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
}

// Pulls the score, the engine's ranking and the first move of the principal
// variation out of one "info" line. Returns false if the line carries no move.
static bool ParseInfoLine(const std::string& line, StockfishMove* mv) {
    if (line.find(" pv ") == std::string::npos) return false;

    mv->mate = false;
    mv->scoreCp = 0;
    mv->mateIn = 0;
    mv->multipv = 1; // Stockfish omits it when MultiPV is 1.
    mv->uci.clear();

    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == "multipv") {
            iss >> mv->multipv;
        }
        else if (token == "score") {
            iss >> token;
            if (token == "cp") {
                iss >> mv->scoreCp;
            }
            else if (token == "mate") {
                mv->mate = true;
                iss >> mv->mateIn;
            }
        }
        else if (token == "pv") {
            iss >> mv->uci;
            break;
        }
    }

    return !mv->uci.empty();
}

// Depth an "info" line reports, or 0 when it carries none.
static int ParseDepth(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            int depth = 0;
            if (iss >> depth) return depth;
            return 0;
        }
    }
    return 0;
}

// Releases the handles of an engine that has gone away, so a relaunch starts
// from a clean slate rather than writing into a broken pipe.
static void ReleaseStockfishHandles() {
    if (g_sfInput) { CloseHandle(g_sfInput); g_sfInput = nullptr; }
    if (g_sfOutput) { CloseHandle(g_sfOutput); g_sfOutput = nullptr; }
    if (g_sfProcInfo.hProcess) { CloseHandle(g_sfProcInfo.hProcess); g_sfProcInfo.hProcess = nullptr; }
    if (g_sfProcInfo.hThread) { CloseHandle(g_sfProcInfo.hThread); g_sfProcInfo.hThread = nullptr; }
    g_sfRunning = false;
}

void LaunchStockfish(const std::string& path) {
    ReleaseStockfishHandles();
    g_sfPath = path;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
    HANDLE hStdInRead = nullptr, hStdInWrite = nullptr;

    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0);
    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.hStdInput = hStdInRead;

    if (!CreateProcessA(path.c_str(), nullptr, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &g_sfProcInfo)) {
        std::cerr << "[ERROR] Failed to launch Stockfish.\n";
        return;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdInRead);

    g_sfInput = hStdInWrite;
    g_sfOutput = hStdOutRead;
    g_sfRunning = true;

    SendCommand("uci\n");
}

bool StockfishIsAlive() {
    // Both the render loop and the detection loop ask this every pass, which was
    // an isready and a blocking read of up to half a second each time, on the
    // same pipe and mutex the search uses. The engine's liveness does not change
    // at that rate, so the answer is cached between probes.
    static DWORD lastProbeTick = 0;
    static DWORD lastRestartTick = 0;
    static bool lastResult = false;

    std::lock_guard<std::mutex> lock(g_sfMutex);

    const DWORD now = GetTickCount();
    if (lastProbeTick != 0 && (now - lastProbeTick) < kLivenessProbeIntervalMs) {
        return lastResult;
    }
    lastProbeTick = now;

    // A crashed engine leaves a signalled process handle behind. Noticing that
    // is what makes a restart possible: until this existed, one bad position
    // took the engine down for the rest of the session and every later command
    // was written into a pipe with nothing on the other end.
    if (g_sfRunning && g_sfProcInfo.hProcess &&
        WaitForSingleObject(g_sfProcInfo.hProcess, 0) == WAIT_OBJECT_0) {
        std::cerr << "[WARN] Stockfish exited; restarting.\n";
        ReleaseStockfishHandles();
    }

    if (!g_sfRunning) {
        lastResult = false;
        if (lastRestartTick != 0 && (now - lastRestartTick) < kRestartBackoffMs) {
            return false;
        }
        lastRestartTick = now;

        // LaunchStockfish does not take the mutex, so calling it here is safe.
        LaunchStockfish(g_sfPath);
        g_sfPreviousMoves.clear();
        if (!g_sfRunning) return false;
    }

    lastResult = false;
    SendCommand("isready\n");

    std::string response;
    char buffer[128];
    DWORD bytesRead = 0;
    DWORD startTick = GetTickCount();

    while (GetTickCount() - startTick < 500) {
        if (ReadFile(g_sfOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
            if (response.find("readyok") != std::string::npos) {
                lastResult = true;
                return true;
            }
        }
        else {
            Sleep(5);
        }
    }

    return false;
}

std::vector<StockfishMove> GetBestMoves(const std::string& fen, int elo, int topN = 5, int depth = 15) {
    std::vector<StockfishMove> moves;
    // Every tracked position is analysed, whoever is to move. The interface
    // decides whether to offer the moves as suggestions; an evaluation of the
    // opponent's turn is what makes it possible to say what their move cost.
    if (!g_sfRunning) return g_sfPreviousMoves;

    g_sfNoLegalMoves = false;

    std::lock_guard<std::mutex> lock(g_sfMutex);
    SendCommand("setoption name UCI_LimitStrength value " +
        std::string(g_sfLimitStrength ? "true" : "false") + "\n");
    if (g_sfLimitStrength) {
        SendCommand("setoption name UCI_Elo value " + std::to_string(elo) + "\n");
    }
    SendCommand("setoption name MultiPV value " + std::to_string(topN) + "\n");

    // The published evaluation is always from White's point of view. Working
    // that out once here saves every reader from having to know whose turn it is,
    // and stops the bar reading backwards for whoever is playing black.
    bool whiteToMove = true;
    {
        const size_t space = fen.find(' ');
        if (space != std::string::npos && space + 1 < fen.size()) {
            whiteToMove = (fen[space + 1] != 'b');
        }
    }

    SendCommand("position fen " + fen + "\n");

    // An "eval" round-trip used to sit here, scanning up to two seconds for the
    // "Final evaluation" line and then discarding what it read. It delayed every
    // position and, because it consumed from the same pipe, could swallow info
    // lines belonging to the search that followed.
    SendCommand("go depth " + std::to_string(depth) + "\n");

    // Keyed by the engine's own MultiPV index, so later and deeper lines replace
    // earlier ones and the final set comes out in ranked order. Matching on
    // "info depth N" instead, as this used to, produced nothing at all whenever
    // the search stopped short of the requested depth, which is exactly what it
    // does when it finds a forced mate.
    std::map<int, StockfishMove> byRank;

    char buffer[512];
    DWORD bytesRead = 0;
    std::string response;
    size_t consumed = 0;

    DWORD startTick = GetTickCount();
    while (GetTickCount() - startTick < 5000) {
        if (ReadFile(g_sfOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;

            // Only walk what has arrived since the last pass. The old code
            // re-parsed the whole accumulated response on every read.
            size_t newline;
            while ((newline = response.find('\n', consumed)) != std::string::npos) {
                std::string line = response.substr(consumed, newline - consumed);
                consumed = newline + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (line.rfind("info", 0) == 0) {
                    StockfishMove parsed;
                    if (!ParseInfoLine(line, &parsed)) continue;
                    byRank[parsed.multipv] = parsed;

                    // Publish the running evaluation as each depth lands, rather
                    // than once when the search finishes. This is what lets the
                    // bar climb toward the answer instead of jumping to it.
                    if (parsed.multipv == 1) {
                        g_liveEvalCpWhite.store(whiteToMove ? parsed.scoreCp : -parsed.scoreCp);
                        g_liveEvalIsMate.store(parsed.mate);
                        g_liveEvalMateInWhite.store(whiteToMove ? parsed.mateIn : -parsed.mateIn);
                        g_liveEvalDepth.store(ParseDepth(line));
                        g_liveEvalValid.store(true);
                    }
                    continue;
                }

                if (line.rfind("bestmove", 0) == 0) {
                    // "bestmove (none)" is checkmate or stalemate: a legal
                    // position with nothing to play. Not a failure, and worth
                    // saying so rather than showing an empty list.
                    g_sfNoLegalMoves = (line.find("(none)") != std::string::npos);

                    for (const auto& entry : byRank) moves.push_back(entry.second);
                    g_sfPreviousMoves = moves;
                    return moves;
                }
            }
        }
        else {
            Sleep(10);
        }
    }

    for (const auto& entry : byRank) moves.push_back(entry.second);
    g_sfPreviousMoves = moves;
    return moves;
}

void ShutdownStockfish() {
    if (g_sfRunning) {
        SendCommand("quit\n");
        WaitForSingleObject(g_sfProcInfo.hProcess, 2000);
        CloseHandle(g_sfProcInfo.hProcess);
        CloseHandle(g_sfProcInfo.hThread);
        g_sfRunning = false;
    }
}