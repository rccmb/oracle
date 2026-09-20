#include "StockfishHandler.h"

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

// How long a liveness answer stays good for before the engine is probed again.
static constexpr DWORD kLivenessProbeIntervalMs = 2000;

// TODO: Documentation.
static void SendCommand(const std::string& cmd) {
    if (!g_sfInput) return;
    DWORD written = 0;
    WriteFile(g_sfInput, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
}

// TODO: Documentation.
static void ParseInfoLine(const std::string& line, StockfishMove* mv) {
    if (line.find(" pv ") == std::string::npos) return;

    mv->mate = false;
    mv->scoreCp = 0;
    mv->mateIn = 0;

    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == "score") {
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
}

void LaunchStockfish(const std::string& path = "stockfish/stockfish.exe") {
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
    if (!g_sfRunning) return false;

    // Both the render loop and the detection loop ask this every pass, which was
    // an isready and a blocking read of up to half a second each time, on the
    // same pipe and mutex the search uses. The engine's liveness does not change
    // at that rate, so the answer is cached between probes.
    static DWORD lastProbeTick = 0;
    static bool lastResult = false;

    const DWORD now = GetTickCount();
    if (lastProbeTick != 0 && (now - lastProbeTick) < kLivenessProbeIntervalMs) {
        return lastResult;
    }
    lastProbeTick = now;
    lastResult = false;

    std::lock_guard<std::mutex> lock(g_sfMutex);
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
    if (!g_sfRunning || (g_sfPlayWhite && g_sideToMove == 'b') || (!g_sfPlayWhite && g_sideToMove == 'w')) return g_sfPreviousMoves;

    std::lock_guard<std::mutex> lock(g_sfMutex);
    SendCommand("setoption name UCI_LimitStrength value " +
        std::string(g_sfLimitStrength ? "true" : "false") + "\n");
    if (g_sfLimitStrength) {
        SendCommand("setoption name UCI_Elo value " + std::to_string(elo) + "\n");
    }
    SendCommand("setoption name MultiPV value " + std::to_string(topN) + "\n");

    SendCommand("position fen " + fen + "\n");

    // An "eval" round-trip used to sit here, scanning up to two seconds for the
    // "Final evaluation" line and then discarding what it read. It delayed every
    // position and, because it consumed from the same pipe, could swallow info
    // lines belonging to the search that followed.
    SendCommand("go depth " + std::to_string(depth) + "\n");

    char buffer[512];
    DWORD bytesRead = 0;
    std::string response;

    DWORD startTick = GetTickCount();
    while (GetTickCount() - startTick < 5000) {
        if (ReadFile(g_sfOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;

            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("info depth " + std::to_string(depth)) != std::string::npos && line.find(" pv ") != std::string::npos) {
                    StockfishMove parsed;
                    ParseInfoLine(line, &parsed);

                    auto it = std::find_if(moves.begin(), moves.end(), [&](const StockfishMove& m) { return m.uci == parsed.uci; });
                    if (it == moves.end()) {
                        moves.push_back(parsed);
                    }
                    else {
                        *it = parsed;
                    }
                }
                if (line.find("bestmove") != std::string::npos) {
                    g_sfPreviousMoves = moves;
                    return moves;
                }
            }
        }
        else {
            Sleep(10);
        }
    }

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