#pragma once

#include <string>
#include <vector>

#include "ChessRules.h"

// Follows a game rather than re-reading the board.
//
// The old approach treated every frame as an independent classification problem:
// sixty-four squares in, a FEN out, nothing connecting one frame to the next.
// That is why castling rights had to be invented, side to move was guessed from
// a pixel diff, and a single misread square produced a plausible wrong position
// the engine then answered confidently.
//
// This asks a much narrower question instead. Given the position already held
// and the thirty or so moves legal in it, which one explains what is on screen?
// Nearly always exactly one does, and when none does the frame is rejected
// rather than believed, so half-finished piece animations and squares briefly
// covered by a dialog cost nothing.

enum class TrackerOutcome {
    Unchanged,    // The board matches the position already held.
    Advanced,     // One or two legal moves account for the change.
    Restarted,    // The starting position: a new game.
    TookBack,     // An earlier position in this game; the moves after it are dropped.
    Adopted,      // Resynchronised onto a position that could not be reached.
    Unreadable,   // Nothing explains the frame. The held position is unchanged.
};

struct TrackedMove {
    Move move;
    std::string uci;
    Color playedBy = Color::White;
};

class GameTracker {
public:
    GameTracker();

    // Starts again from the initial position.
    void Reset();

    // Reconciles one observed board, given in FEN reading order with ' ' for an
    // empty square, against the game so far.
    TrackerOutcome Observe(const char observed[64]);

    const ChessPosition& Position() const { return m_position; }
    const std::vector<TrackedMove>& History() const { return m_history; }

    // Plies played so far, which also indexes the current position.
    int Ply() const { return (int)m_history.size(); }

    // Moves applied by the most recent Observe: 0, 1 or 2.
    int LastAppliedCount() const { return m_lastApplied; }

    // Consecutive frames that nothing could explain. A rising count means the
    // tracker has lost the thread, or the board is simply mid-animation.
    int UnreadableStreak() const { return m_unreadableStreak; }

private:
    bool TryAdopt(const char observed[64]);

    ChessPosition m_position;
    std::vector<ChessPosition> m_positionHistory; // Index 0 is the initial position.
    std::vector<TrackedMove> m_history;

    int m_lastApplied = 0;
    int m_unreadableStreak = 0;

    // The board seen during the current run of unexplained frames, and how many
    // times running it has looked the same.
    char m_pendingBoard[64];
    int m_pendingRepeats = 0;
};
