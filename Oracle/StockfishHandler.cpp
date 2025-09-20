#include "StockfishHandler.h"

HANDLE g_sfInput = nullptr;
HANDLE g_sfOutput = nullptr;
std::mutex g_sfMutex;
bool g_sfRunning = false;

std::vector<StockfishMove> g_sfPreviousMoves;

int g_sfElo = 100;
bool g_sfPlayWhite = true;
int g_sfMoveDepth = 1;
int g_sfNumberMoves = 1;

static PROCESS_INFORMATION g_sfProcInfo = { 0 };
static HANDLE g_sfThread = nullptr;
static bool g_sfStopThread = false;

// TODO: Documentation.
static void SendCommand(const std::string& cmd) {
    if (!g_sfInput) return;
    DWORD written = 0;
    WriteFile(g_sfInput, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
}

// TODO: Documentation.
static void ParseInfoLine(const std::string& line, std::vector<StockfishMove>& moves) {
    if (line.find(" pv ") == std::string::npos) return;

    StockfishMove mv{};
    mv.mate = false;
    mv.scoreCp = 0;
    mv.mateIn = 0;

    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == "score") {
            iss >> token;
            if (token == "cp") {
                iss >> mv.scoreCp;
            }
            else if (token == "mate") {
                mv.mate = true;
                iss >> mv.mateIn;
            }
        }
        else if (token == "pv") {
            iss >> mv.uci; // first move in PV line
            break;
        }
    }

    if (!mv.uci.empty())
        moves.push_back(mv);
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
    SendCommand("setoption name UCI_LimitStrength value true\n");
    SendCommand("setoption name UCI_Elo value " + std::to_string(elo) + "\n");
    SendCommand("setoption name MultiPV value " + std::to_string(topN) + "\n");
    
    SendCommand("position fen " + fen + "\n");

    SendCommand("eval\n");
    {
        char bufferEval[512];
        DWORD bytesReadEval = 0;
        std::string responseEval;

        DWORD startTickEval = GetTickCount();
        bool evalDone = false;

        while (GetTickCount() - startTickEval < 2000 && !evalDone) {
            if (ReadFile(g_sfOutput, bufferEval, sizeof(bufferEval) - 1, &bytesReadEval, nullptr) && bytesReadEval > 0) {
                bufferEval[bytesReadEval] = '\0';
                responseEval.append(bufferEval);

                std::string line;
                size_t pos;
                while ((pos = responseEval.find('\n')) != std::string::npos) {
                    line = responseEval.substr(0, pos);
                    responseEval.erase(0, pos + 1);

                    std::cout << "[Stockfish Eval] " << line << std::endl;

                    // TODO: Improve upon this.
                    if (line.rfind("Final evaluation:", 0) == 0) {
                        std::cout << "[Stockfish Eval] King is in check, skipping move generation." << std::endl;
                        return g_sfPreviousMoves;
                    }
                    if (line.rfind("Final evaluation", 0) == 0) {
                        evalDone = true;
                        break;
                    }
                }
            }
            else {
                Sleep(10);
            }
        }
    }

	std::cout << "[Stockfish] Sending go depth " << depth << " command." << std::endl;
   
    SendCommand("go depth " + std::to_string(depth) + "\n");

    char buffer[512];
    DWORD bytesRead = 0;
    std::string response;

	std::cout << "[Stockfish] Waiting for bestmove..." << std::endl;

    DWORD startTick = GetTickCount();
    while (GetTickCount() - startTick < 5000) {
        if (ReadFile(g_sfOutput, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;

            std::istringstream iss(response);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("info depth") != std::string::npos && line.find(" pv ") != std::string::npos) {
                    ParseInfoLine(line, moves);
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