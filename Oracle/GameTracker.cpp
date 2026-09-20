#include "GameTracker.h"

#include <cstring>

namespace {

// How many times running an unexplained board has to look identical before the
// tracker gives up on its own game and adopts it. High enough that piece
// animations and a dialog sliding over the board never trigger it, low enough
// that a genuine desynchronisation recovers within a second or so.
constexpr int kAdoptAfterRepeats = 40;

// Squares a candidate may disagree on and still be accepted.
//
// Demanding all sixty-four match exactly sounds safer and is in fact useless:
// template matching misreads the odd square, and under an exact rule one wrong
// square means no move ever explains the board again.
//
// One is the only safe slack. A quiet move changes exactly two squares, so at a
// tolerance of two a move and a pair of misreads become the same thing: a new
// game reads as noise, and two plies in one frame read as the first of them.
// At one, a single misread square is absorbed and everything else still has to
// be explained properly.
constexpr int kSquareTolerance = 1;

// Counting is capped for speed, but the cap has to leave room to tell a good
// candidate from a merely plausible one, so it sits above the tolerance.
constexpr int kMismatchCountCap = kSquareTolerance + 3;

bool SameBoard(const char a[64], const char b[64]) {
    return std::memcmp(a, b, 64) == 0;
}

int MismatchCount(const ChessPosition& position, const char observed[64], int giveUpAt) {
    int mismatches = 0;
    for (int square = 0; square < 64; ++square) {
        if (position.PieceAt(square) == observed[square]) continue;
        if (++mismatches > giveUpAt) return mismatches; // No need for an exact count.
    }
    return mismatches;
}

} // namespace

GameTracker::GameTracker() {
    Reset();
}

void GameTracker::Reset() {
    m_position = ChessPosition::StartingPosition();
    m_positionHistory.assign(1, m_position);
    m_history.clear();
    m_lastApplied = 0;
    m_unreadableStreak = 0;
    m_pendingRepeats = 0;
    std::memset(m_pendingBoard, ' ', sizeof(m_pendingBoard));
}

TrackerOutcome GameTracker::Observe(const char observed[64]) {
    m_lastApplied = 0;

    // How badly the position already held disagrees with the frame. A couple of
    // squares of disagreement is ordinary detector noise, not a move.
    const int stayMismatch = MismatchCount(m_position, observed, kMismatchCountCap);

    // One ply. This is the overwhelmingly common case, and the reason the whole
    // approach works: about thirty candidates, and the one that was actually
    // played reproduces the board while the rest are nowhere near it.
    const std::vector<Move> legal = m_position.LegalMoves();

    const Move* bestMove = nullptr;
    ChessPosition bestPosition;
    int bestMismatch = kMismatchCountCap + 1;
    int secondMismatch = kMismatchCountCap + 1;

    for (const Move& move : legal) {
        const ChessPosition next = m_position.AfterMove(move);
        const int mismatch = MismatchCount(next, observed, kMismatchCountCap);

        if (mismatch < bestMismatch) {
            secondMismatch = bestMismatch;
            bestMismatch = mismatch;
            bestMove = &move;
            bestPosition = next;
        }
        else if (mismatch < secondMismatch) {
            secondMismatch = mismatch;
        }
    }

    // Standing still is preferred whenever it explains the frame at least as
    // well as any move does, so noise on a quiet board is never read as a move.
    if (stayMismatch <= kSquareTolerance && stayMismatch <= bestMismatch) {
        m_unreadableStreak = 0;
        m_pendingRepeats = 0;
        return TrackerOutcome::Unchanged;
    }

    // A move is taken only when it beats both standing still and every other
    // move. An exact reproduction needs no margin; anything less has to win
    // outright, since two candidates fitting equally well means the frame does
    // not actually say which was played.
    const bool decisive = (bestMismatch == 0) || (bestMismatch < secondMismatch);

    if (bestMove && bestMismatch <= kSquareTolerance && bestMismatch < stayMismatch && decisive) {
        m_history.push_back({ *bestMove, bestMove->ToUci(), m_position.SideToMove() });
        m_position = bestPosition;
        m_positionHistory.push_back(bestPosition);
        m_lastApplied = 1;
        m_unreadableStreak = 0;
        m_pendingRepeats = 0;
        return TrackerOutcome::Advanced;
    }

    // Two plies. Frames are missed when a reply is premoved, when both sides
    // move inside one capture interval, or simply when the machine is busy.
    for (const Move& first : legal) {
        const ChessPosition afterFirst = m_position.AfterMove(first);
        for (const Move& second : afterFirst.LegalMoves()) {
            const ChessPosition afterSecond = afterFirst.AfterMove(second);
            if (!afterSecond.SameBoard(observed)) continue;

            m_history.push_back({ first, first.ToUci(), m_position.SideToMove() });
            m_history.push_back({ second, second.ToUci(), afterFirst.SideToMove() });
            m_positionHistory.push_back(afterFirst);
            m_positionHistory.push_back(afterSecond);
            m_position = afterSecond;
            m_lastApplied = 2;
            m_unreadableStreak = 0;
            m_pendingRepeats = 0;
            return TrackerOutcome::Advanced;
        }
    }

    // A fresh game. Checked before the history search, since the starting
    // position is also the first entry there and this is the more useful reading.
    {
        const ChessPosition start = ChessPosition::StartingPosition();
        if (start.SameBoard(observed) && !m_history.empty()) {
            Reset();
            return TrackerOutcome::Restarted;
        }
    }

    // A takeback, or a game rewound to review it. The moves after the matching
    // position are dropped, which is what actually happened.
    for (int index = (int)m_positionHistory.size() - 2; index >= 0; --index) {
        if (!m_positionHistory[index].SameBoard(observed)) continue;

        m_position = m_positionHistory[index];
        m_positionHistory.resize((size_t)index + 1);
        m_history.resize((size_t)index);
        m_unreadableStreak = 0;
        m_pendingRepeats = 0;
        return TrackerOutcome::TookBack;
    }

    // Nothing explains it. Hold the position, and count how long this has been
    // true: a frame caught mid-animation looks like this for a moment, a game
    // loaded from a puzzle or a position set up by hand looks like it forever.
    ++m_unreadableStreak;

    if (m_pendingRepeats > 0 && SameBoard(m_pendingBoard, observed)) {
        ++m_pendingRepeats;
    }
    else {
        std::memcpy(m_pendingBoard, observed, sizeof(m_pendingBoard));
        m_pendingRepeats = 1;
    }

    if (m_pendingRepeats >= kAdoptAfterRepeats && TryAdopt(observed)) {
        return TrackerOutcome::Adopted;
    }

    return TrackerOutcome::Unreadable;
}

bool GameTracker::TryAdopt(const char observed[64]) {
    // A board alone does not say whose turn it is. It does rule one out: the
    // side not to move cannot be standing in check, because the move that left
    // them there would have been illegal. Where both readings survive, White is
    // taken, which is the convention for a position given without context.
    ChessPosition candidate;
    bool found = false;

    for (int attempt = 0; attempt < 2; ++attempt) {
        const Color side = (attempt == 0) ? Color::White : Color::Black;

        std::string placement;
        for (int rankIndex = 0; rankIndex < 8; ++rankIndex) {
            int empty = 0;
            for (int file = 0; file < 8; ++file) {
                const char piece = observed[rankIndex * 8 + file];
                if (piece == ' ') { ++empty; continue; }
                if (empty > 0) { placement += std::to_string(empty); empty = 0; }
                placement += piece;
            }
            if (empty > 0) placement += std::to_string(empty);
            if (rankIndex < 7) placement += '/';
        }

        // Castling rights are read off the board the same conservative way the
        // FEN builder does: a right only where both the king and that rook are
        // still home.
        std::string castling;
        if (observed[60] == 'K') {
            if (observed[63] == 'R') castling += 'K';
            if (observed[56] == 'R') castling += 'Q';
        }
        if (observed[4] == 'k') {
            if (observed[7] == 'r') castling += 'k';
            if (observed[0] == 'r') castling += 'q';
        }
        if (castling.empty()) castling = "-";

        const std::string fen = placement + (side == Color::White ? " w " : " b ") +
                                castling + " - 0 1";

        const std::optional<ChessPosition> parsed = ChessPosition::FromFen(fen);
        if (!parsed) continue;
        if (parsed->IsInCheck(Opposite(side))) continue;

        candidate = *parsed;
        found = true;
        break;
    }

    if (!found) return false;

    m_position = candidate;
    m_positionHistory.assign(1, candidate);
    m_history.clear();
    m_lastApplied = 0;
    m_unreadableStreak = 0;
    m_pendingRepeats = 0;
    return true;
}
