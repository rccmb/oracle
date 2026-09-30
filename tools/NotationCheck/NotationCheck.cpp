// Checks the standard algebraic notation Oracle shows moves in.
//
// Every expectation is written by hand, as a player would write the move, so a
// change that keeps the notation consistent with itself but not with chess still
// fails here. The cases are the ones SAN has rules for: captures by pawns and
// pieces, en passant, castling both ways for both sides, promotion with and
// without a capture, check and mate, and each way a piece can need telling
// apart from another, including the pinned piece that does not count.
#include "chess/ChessRules.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void Expect(const char* fen, const char* uci, const char* expected) {
    ++checks;
    const std::optional<ChessPosition> position = ChessPosition::FromFen(fen);
    if (!position) {
        ++failures;
        std::printf("FAIL  unparseable FEN %s\n", fen);
        return;
    }

    const std::string san = UciToSan(*position, uci);
    if (san != expected) {
        ++failures;
        std::printf("FAIL  %-6s gave %-8s expected %-8s in %s\n", uci, san.c_str(), expected, fen);
    }
    else {
        std::printf("ok    %-6s %s\n", uci, san.c_str());
    }
}

} // namespace

int main() {
    const char* start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // Plain moves.
    Expect(start, "e2e4", "e4");
    Expect(start, "g1f3", "Nf3");
    Expect(start, "b1c3", "Nc3");

    // Captures.
    Expect("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", "e4d5", "exd5");
    Expect("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3", "e5f6", "exf6");
    Expect("rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2", "f6e4", "Nxe4");

    // Castling, both sides, both ways.
    Expect("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1", "O-O");
    Expect("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1c1", "O-O-O");
    Expect("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8g8", "O-O");
    Expect("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8c8", "O-O-O");

    // Promotion, with check, without, and with a capture.
    Expect("8/P7/8/8/8/8/8/k6K w - - 0 1", "a7a8q", "a8=Q+");
    Expect("8/P7/8/8/8/8/8/k6K w - - 0 1", "a7a8n", "a8=N");
    Expect("1r5k/P7/8/8/8/8/8/K7 w - - 0 1", "a7b8q", "axb8=Q+");

    // Check and mate.
    Expect("4k3/8/8/8/8/8/8/4K2R w K - 0 1", "h1h8", "Rh8+");
    Expect("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2", "d8h4", "Qh4#");

    // Telling pieces apart: by file, by rank, and by both.
    Expect("4k3/8/8/8/8/8/8/R4RK1 w - - 0 1", "a1d1", "Rad1");
    Expect("4k3/8/8/8/8/8/8/R4RK1 w - - 0 1", "f1d1", "Rfd1");
    Expect("4k3/8/8/8/R7/8/8/R3K3 w - - 0 1", "a1a2", "R1a2");
    Expect("4k3/8/8/8/R7/8/8/R3K3 w - - 0 1", "a4a2", "R4a2");
    Expect("4k3/8/8/8/8/2N3N1/8/4K3 w - - 0 1", "c3e4", "Nce4");
    Expect("4k3/8/8/8/8/Q7/8/Q1Q1K3 w - - 0 1", "a1b2", "Qa1b2");

    // A pinned knight cannot go there, so it is not a rival and the move needs
    // no disambiguation.
    Expect("k3r3/8/8/8/8/8/2N1N3/4K3 w - - 0 1", "c2d4", "Nd4");

    // Something that is not a legal move comes back as it went in.
    Expect(start, "e2e5", "e2e5");

    std::printf("\n%d of %d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
