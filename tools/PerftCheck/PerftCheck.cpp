// Proves the move generator against the published perft suite.
//
// Perft counts leaf nodes at a fixed depth. The numbers below are the standard
// reference values, and every one of them is sensitive to a different corner of
// the rules: en passant, promotion, castling through check, pinned pieces,
// castling rights lost to a captured rook. A generator that gets all six
// positions right is not subtly wrong.
#include "chess/ChessRules.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Case {
    const char* name;
    const char* fen;
    std::vector<unsigned long long> expected; // Index 0 is depth 1.
};

int failures = 0;
int checks = 0;

void Run(const Case& testCase) {
    std::printf("%s\n  %s\n", testCase.name, testCase.fen);

    const std::optional<ChessPosition> position = ChessPosition::FromFen(testCase.fen);
    if (!position) {
        std::printf("  FAIL: could not parse the FEN\n\n");
        ++failures;
        return;
    }

    // Round trip: a position that cannot rewrite its own FEN would quietly send
    // the engine something other than what it searched.
    const std::string rewritten = position->ToFen();
    if (rewritten != testCase.fen) {
        std::printf("  FAIL: FEN round trip differs\n        got %s\n", rewritten.c_str());
        ++failures;
    }
    ++checks;

    for (size_t i = 0; i < testCase.expected.size(); ++i) {
        const int depth = (int)i + 1;
        const unsigned long long nodes = Perft(*position, depth);
        const unsigned long long want = testCase.expected[i];
        const bool ok = (nodes == want);

        std::printf("  depth %d: %-10llu %s\n", depth, nodes,
            ok ? "ok" : ("MISMATCH, expected " + std::to_string(want)).c_str());

        ++checks;
        if (!ok) ++failures;
    }
    std::printf("\n");
}

} // namespace

int main() {
    const std::vector<Case> cases = {
        { "starting position",
          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
          { 20, 400, 8902, 197281, 4865609 } },

        { "kiwipete (castling, pins, promotions)",
          "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
          { 48, 2039, 97862, 4085603 } },

        { "endgame (en passant, discovered check)",
          "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
          { 14, 191, 2812, 43238, 674624 } },

        { "promotion heavy, castling rights",
          "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
          { 6, 264, 9467, 422333 } },

        { "no en passant, tight position",
          "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
          { 44, 1486, 62379, 2103487 } },

        { "middlegame",
          "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
          { 46, 2079, 89890, 3894594 } },
    };

    for (const Case& testCase : cases) Run(testCase);

    std::printf("%s: %d of %d checks failed\n",
        failures == 0 ? "ALL PASS" : "FAILURES", failures, checks);
    return failures == 0 ? 0 : 1;
}
