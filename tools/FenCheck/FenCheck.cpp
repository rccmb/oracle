// Drives the real BoardToFEN over synthetic boards and checks the FEN it emits.
//
// Guards the crash described in README.md: claiming a castling right for a side
// with no rook segfaults Stockfish, which is what killed the engine in endgames.
#include "ChessboardDetection.h"
#include <cstdio>

// Rows are given as seen on screen, top row first, using FEN letters and '.' for
// an empty square. Orientation 0 means white is at the bottom.
static void SetBoard(const char* rows[8], int orientation, char sideToMove) {
    g_orientation = orientation;
    g_sideToMove = sideToMove;
    g_boardGridRows.assign(8, std::string(8, ' '));
    for (int r = 0; r < 8; ++r) {
        std::string row;
        for (int c = 0; c < 8; ++c) {
            char ch = rows[r][c];
            row.push_back(ch == '.' ? ' ' : ch);
        }
        g_boardGridRows[r] = row;
    }
}

static int failures = 0;

static void Check(const char* name, const char* rows[8], int orientation,
                  char sideToMove, const char* expectCastling) {
    SetBoard(rows, orientation, sideToMove);
    const std::string fen = BoardToFEN();

    if (expectCastling == nullptr) {
        const bool ok = fen.empty();
        std::printf("%-44s %s  %s\n", name, ok ? "PASS" : "FAIL",
            ok ? "(rejected, as expected)" : fen.c_str());
        if (!ok) ++failures;
        return;
    }

    if (fen.empty()) {
        std::printf("%-44s FAIL  (rejected, expected castling '%s')\n", name, expectCastling);
        ++failures;
        return;
    }

    // Castling is the fourth space separated field.
    std::string fields[6];
    int n = 0;
    size_t pos = 0;
    while (n < 6 && pos <= fen.size()) {
        size_t sp = fen.find(' ', pos);
        if (sp == std::string::npos) sp = fen.size();
        fields[n++] = fen.substr(pos, sp - pos);
        pos = sp + 1;
    }

    const bool ok = (fields[2] == expectCastling);
    std::printf("%-44s %s  castling='%s'%s\n", name, ok ? "PASS" : "FAIL",
        fields[2].c_str(), ok ? "" : (std::string(" expected '") + expectCastling + "'").c_str());
    if (!ok) ++failures;
    std::printf("    FEN: %s\n", fen.c_str());
}

int main() {
    const char* start[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    Check("start position", start, 0, 'w', "KQkq");

    // Same position seen from black's side: every row and file reversed.
    const char* startFlipped[8] = {
        "RNBKQBNR", "PPPPPPPP", "........", "........",
        "........", "........", "pppppppp", "rnbkqbnr"
    };
    Check("start position, board flipped", startFlipped, 1, 'w', "KQkq");

    const char* kingMoved[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPKPPP", "RNBQ.BNR"
    };
    Check("white king off e1 (the crash case)", kingMoved, 0, 'w', "kq");

    const char* castledShort[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQ.RK."
    };
    Check("white has castled short", castledShort, 0, 'w', "kq");

    const char* h1RookGone[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBN."
    };
    Check("white h1 rook captured", h1RookGone, 0, 'w', "Qkq");

    const char* noWhiteRooks[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", ".NBQKBN."
    };
    Check("white has no rooks (the other crash case)", noWhiteRooks, 0, 'w', "kq");

    const char* endgame[8] = {
        "....k...", "........", "........", "........",
        "........", "........", "....K...", "........"
    };
    Check("bare kings endgame", endgame, 0, 'w', "-");

    // The two positions that used to kill the engine. Written as KQkq they
    // segfault Stockfish outright, and they are ordinary ways for a game to end,
    // which is why the engine seemed to die whenever a game reached mate.
    const char* queenMate[8] = {
        ".......k", "........", "........", "........",
        "........", "........", "......Q.", "......K."
    };
    Check("king and queen against king", queenMate, 0, 'w', "-");

    const char* rookMate[8] = {
        ".......k", "........", "........", "........",
        "........", "........", "........", "R.....K."
    };
    Check("king and rook against king", rookMate, 0, 'w', "-");

    const char* pawnOnBackRank[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "pNBQKBNR"
    };
    Check("pawn on rank 1", pawnOnBackRank, 0, 'w', nullptr);

    const char* twoKings[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "...K....", "PPPPPPPP", "RNBQKBNR"
    };
    Check("two white kings", twoKings, 0, 'w', nullptr);

    const char* adjacentKings[8] = {
        "........", "........", "........", "...kK...",
        "........", "........", "........", "........"
    };
    Check("kings adjacent", adjacentKings, 0, 'w', nullptr);

    const char* noKing[8] = {
        "rnbq.bnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    Check("black king missing (board obscured)", noKing, 0, 'w', nullptr);

    std::printf("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
