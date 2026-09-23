// Checks that Oracle can drive a given UCI engine.
//
// Oracle speaks plain UCI over a pipe and nothing about Stockfish in
// particular, so any engine that answers the protocol will work. This runs the
// same code the overlay does, against whichever engine you point it at, so you
// can find out before a game rather than during one.
//
//   EngineCheck.exe                     the bundled engine
//   EngineCheck.exe path\to\engine.exe  your own
#include "engine/StockfishHandler.h"

#include <cstdio>

int main(int argc, char** argv) {
    const std::string requested = (argc > 1) ? argv[1] : std::string();

    std::printf("requested : %s\n", requested.empty() ? "(bundled)" : requested.c_str());

    LaunchStockfish(requested);

    if (!g_sfEngineError.empty()) {
        std::printf("FAIL: %s\n", g_sfEngineError.c_str());
        return 1;
    }

    std::printf("resolved  : %s\n", g_sfEngineResolved.c_str());
    std::printf("id name   : %s\n", g_sfEngineName.c_str());

    if (!StockfishIsAlive()) {
        std::printf("FAIL: the engine did not answer isready\n");
        return 1;
    }
    std::printf("isready   : ok\n");

    // The starting position, three lines deep enough to be meaningful.
    const std::string startPosition =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    const std::vector<StockfishMove> moves = GetBestMoves(startPosition, 1500, 3, 10);
    if (moves.empty()) {
        std::printf("FAIL: no moves came back from the opening position\n");
        ShutdownStockfish();
        return 1;
    }

    std::printf("moves     :\n");
    for (size_t i = 0; i < moves.size(); ++i) {
        const StockfishMove& move = moves[i];
        if (move.mate) {
            std::printf("  %zu. %-6s mate in %d\n", i + 1, move.uci.c_str(), move.mateIn);
        }
        else {
            std::printf("  %zu. %-6s %+.2f\n", i + 1, move.uci.c_str(), move.scoreCp / 100.0f);
        }
    }

    if (moves.size() == 1) {
        std::printf("note      : only one line came back, so this engine either lacks\n"
                    "            MultiPV or ignores it. Oracle works, but it will only\n"
                    "            ever show one suggestion.\n");
    }

    ShutdownStockfish();
    std::printf("PASS: Oracle can drive this engine\n");
    return 0;
}
