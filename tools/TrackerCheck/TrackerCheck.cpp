// Drives GameTracker through the situations a real game puts it in.
//
// Boards are handed to the tracker the way the vision pipeline hands them over:
// sixty-four characters, nothing else. Expectations are written as FEN strings
// by hand, so a change that makes the tracker agree with itself but disagree
// with chess still fails here.
#include "chess/GameTracker.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int failures = 0;

const char* OutcomeName(TrackerOutcome outcome) {
    switch (outcome) {
    case TrackerOutcome::Unchanged:  return "Unchanged";
    case TrackerOutcome::Advanced:   return "Advanced";
    case TrackerOutcome::Restarted:  return "Restarted";
    case TrackerOutcome::TookBack:   return "TookBack";
    case TrackerOutcome::Adopted:    return "Adopted";
    case TrackerOutcome::Unreadable: return "Unreadable";
    }
    return "?";
}

// Builds the board a FEN describes, which is all the tracker ever receives.
void BoardFromFen(const std::string& fen, char out[64]) {
    const std::optional<ChessPosition> position = ChessPosition::FromFen(fen);
    if (!position) {
        std::printf("  (test bug: unparseable FEN %s)\n", fen.c_str());
        std::memset(out, ' ', 64);
        return;
    }
    position->CopyBoard(out);
}

void Expect(const char* label, bool condition, const std::string& detail) {
    std::printf("  %-46s %s%s%s\n", label, condition ? "PASS" : "FAIL",
        detail.empty() ? "" : "  ", detail.c_str());
    if (!condition) ++failures;
}

// Shows the tracker a position and checks what it made of it.
void Step(GameTracker& tracker, const char* label, const std::string& boardFen,
          TrackerOutcome expectedOutcome, const std::string& expectedFen) {
    char board[64];
    BoardFromFen(boardFen, board);

    const TrackerOutcome outcome = tracker.Observe(board);
    const std::string actualFen = tracker.Position().ToFen();

    const bool outcomeOk = (outcome == expectedOutcome);
    const bool fenOk = expectedFen.empty() || (actualFen == expectedFen);

    std::string detail;
    if (!outcomeOk) detail += std::string("got ") + OutcomeName(outcome);
    if (!fenOk) detail += std::string(!detail.empty() ? "; " : "") + "fen " + actualFen;

    Expect(label, outcomeOk && fenOk, detail);
}

} // namespace

int main() {
    std::printf("opening moves\n");
    {
        GameTracker tracker;

        Step(tracker, "1. e4",
             "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
             TrackerOutcome::Advanced,
             "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

        Step(tracker, "same frame again is not a move",
             "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
             TrackerOutcome::Unchanged, "");

        Step(tracker, "1... c5",
             "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
             TrackerOutcome::Advanced,
             "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");

        // The en passant square and the halfmove clock are both carried here,
        // and neither is visible on the board.
        Expect("halfmove clock counted", tracker.Position().HalfmoveClock() == 0, "");
        Expect("two plies recorded", tracker.Ply() == 2, "");
        Expect("last move was c7c5",
               tracker.History().back().uci == "c7c5", tracker.History().back().uci);
    }

    std::printf("\ntwo plies in one frame\n");
    {
        GameTracker tracker;
        Step(tracker, "1. e4 e5 seen as a single change",
             "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
             TrackerOutcome::Advanced,
             "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
        Expect("both plies applied", tracker.LastAppliedCount() == 2, "");
    }

    std::printf("\ncastling\n");
    {
        GameTracker tracker;
        // 1. Nf3 Nf6 2. g3 g6 3. Bg2 Bg7, then castle.
        const char* line[] = {
            "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1",
            "rnbqkb1r/pppppppp/5n2/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 2 2",
            "rnbqkb1r/pppppppp/5n2/8/8/5NP1/PPPPPP1P/RNBQKB1R b KQkq - 0 2",
            "rnbqkb1r/pppppp1p/5np1/8/8/5NP1/PPPPPP1P/RNBQKB1R w KQkq - 0 3",
            "rnbqkb1r/pppppp1p/5np1/8/8/5NP1/PPPPPPBP/RNBQK2R b KQkq - 1 3",
            "rnbqk2r/ppppppbp/5np1/8/8/5NP1/PPPPPPBP/RNBQK2R w KQkq - 2 4",
        };
        for (const char* fen : line) {
            char board[64];
            BoardFromFen(fen, board);
            tracker.Observe(board);
        }
        Expect("six plies reached", tracker.Ply() == 6, std::to_string(tracker.Ply()));

        // Two pieces move at once here, which no single move explains. Only the
        // rules say this is one move rather than a misread frame.
        Step(tracker, "white castles short",
             "rnbqk2r/ppppppbp/5np1/8/8/5NP1/PPPPPPBP/RNBQ1RK1 b kq - 3 4",
             TrackerOutcome::Advanced,
             "rnbqk2r/ppppppbp/5np1/8/8/5NP1/PPPPPPBP/RNBQ1RK1 b kq - 3 4");
        Expect("castling recorded as e1g1",
               tracker.History().back().uci == "e1g1", tracker.History().back().uci);
        Expect("white rights are gone",
               tracker.Position().ToFen().find(" kq ") != std::string::npos, "");
    }

    std::printf("\nen passant\n");
    {
        GameTracker tracker;
        const char* line[] = {
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
            "rnbqkbnr/ppp2ppp/8/3pp3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq d6 0 3",
            "rnbqkbnr/ppp2ppp/8/3Pp3/8/5N2/PPPP1PPP/RNBQKB1R b KQkq - 0 3",
            "rnbqkbnr/pp3ppp/8/2pPp3/8/5N2/PPPP1PPP/RNBQKB1R w KQkq c6 0 4",
        };
        for (const char* fen : line) {
            char board[64];
            BoardFromFen(fen, board);
            tracker.Observe(board);
        }
        Step(tracker, "white captures en passant",
             "rnbqkbnr/pp3ppp/2P5/4p3/8/5N2/PPPP1PPP/RNBQKB1R b KQkq - 0 4",
             TrackerOutcome::Advanced,
             "rnbqkbnr/pp3ppp/2P5/4p3/8/5N2/PPPP1PPP/RNBQKB1R b KQkq - 0 4");
        Expect("recorded as d5c6",
               tracker.History().back().uci == "d5c6", tracker.History().back().uci);
    }

    std::printf("\ntakeback and restart\n");
    {
        GameTracker tracker;
        const char* line[] = {
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",
            "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2",
        };
        for (const char* fen : line) {
            char board[64];
            BoardFromFen(fen, board);
            tracker.Observe(board);
        }
        Expect("three plies", tracker.Ply() == 3, "");

        Step(tracker, "board rewound two plies",
             "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
             TrackerOutcome::TookBack,
             "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
        Expect("history truncated", tracker.Ply() == 1, "");

        Step(tracker, "a new game",
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             TrackerOutcome::Restarted,
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        Expect("history cleared", tracker.Ply() == 0, "");
    }

    std::printf("\na misread square does not stop the game being followed\n");
    {
        // The reason this matters: template matching gets the odd square wrong,
        // and under an exact-match rule one wrong square meant no move ever
        // explained the board again and tracking froze for the whole session.
        GameTracker tracker;

        char board[64];
        BoardFromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", board);
        board[1] = ' '; // b8 knight dropped, nowhere near the move that was played.

        const TrackerOutcome outcome = tracker.Observe(board);
        Expect("1. e4 read with a square missing", outcome == TrackerOutcome::Advanced,
               OutcomeName(outcome));
        Expect("and the move recorded is still e2e4",
               !tracker.History().empty() && tracker.History().back().uci == "e2e4",
               tracker.History().empty() ? "none" : tracker.History().back().uci);
        Expect("the tracked board is the real one, not the misread one",
               tracker.Position().ToFen().rfind("rnbqkbnr/pppppppp", 0) == 0,
               tracker.Position().ToFen());

        // Noise on a board where nothing moved must not be read as a move.
        char quiet[64];
        BoardFromFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", quiet);
        quiet[6] = ' '; // g8 knight dropped this time.

        const TrackerOutcome second = tracker.Observe(quiet);
        Expect("a dropped square alone is not a move", second == TrackerOutcome::Unchanged,
               OutcomeName(second));
        Expect("still one ply in", tracker.Ply() == 1, std::to_string(tracker.Ply()));
    }

    std::printf("\nframes that explain nothing are rejected\n");
    {
        GameTracker tracker;
        // A board with pieces teleported around: exactly what a frame caught
        // mid-animation, or with a dialog over it, looks like.
        char nonsense[64];
        BoardFromFen("rnbqkbnr/pppppppp/8/3Q4/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", nonsense);

        const TrackerOutcome outcome = tracker.Observe(nonsense);
        Expect("held, not believed", outcome == TrackerOutcome::Unreadable, OutcomeName(outcome));
        Expect("position untouched",
               tracker.Position().ToFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "");

        // Held only while it looks like noise. A board that persists is real.
        TrackerOutcome last = TrackerOutcome::Unreadable;
        for (int i = 0; i < 60 && last != TrackerOutcome::Adopted; ++i) {
            last = tracker.Observe(nonsense);
        }
        Expect("adopted once it persists", last == TrackerOutcome::Adopted, OutcomeName(last));
        Expect("adopted with the observed pieces",
               tracker.Position().ToFen().rfind("rnbqkbnr/pppppppp/8/3Q4/8/8/PPPPPPPP/RNB1KBNR", 0) == 0,
               tracker.Position().ToFen());
    }

    std::printf("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
