#pragma once

#include <optional>
#include <string>
#include <vector>

// The rules of chess, independent of anything Oracle does with a screen.
//
// Squares are indexed in FEN reading order: 0 is a8, 7 is h8, 56 is a1, 63 is
// h1. That matches the order BoardToFEN already walks the board in, so a
// detected grid maps across without a second coordinate convention.

enum class Color { White, Black };

inline Color Opposite(Color color) {
    return color == Color::White ? Color::Black : Color::White;
}

struct Move {
    int from = -1;
    int to = -1;
    char promotion = 0;      // 'q', 'r', 'b' or 'n'; 0 when the move is not a promotion.
    bool isCastle = false;
    bool isEnPassant = false;
    bool isCapture = false;

    // Long algebraic, the form UCI speaks: "e2e4", "e7e8q", "e1g1" for castling.
    std::string ToUci() const;

    bool operator==(const Move& other) const {
        return from == other.from && to == other.to && promotion == other.promotion;
    }
};

class ChessPosition {
public:
    ChessPosition();

    static ChessPosition StartingPosition();
    static std::optional<ChessPosition> FromFen(const std::string& fen);

    std::string ToFen() const;

    // Every move that is legal here: pseudo-legal moves with the ones that would
    // leave, or leave standing, one's own king in check removed.
    std::vector<Move> LegalMoves() const;

    ChessPosition AfterMove(const Move& move) const;

    bool IsInCheck(Color color) const;
    bool IsSquareAttacked(int square, Color byColor) const;

    Color SideToMove() const { return m_sideToMove; }
    char PieceAt(int square) const { return m_board[square]; }
    int HalfmoveClock() const { return m_halfmove; }
    int FullmoveNumber() const { return m_fullmove; }

    // Compares only the pieces, ignoring side to move, castling rights and the
    // clocks. That is all a captured frame can ever tell us.
    bool SameBoard(const char other[64]) const;

    void CopyBoard(char out[64]) const;

private:
    void Clear();
    void AddPawnMoves(int from, std::vector<Move>& out) const;
    void AddStepMoves(int from, const int* deltas, int count, bool sliding, std::vector<Move>& out) const;
    void AddCastlingMoves(std::vector<Move>& out) const;
    int FindKing(Color color) const;

    char m_board[64];
    Color m_sideToMove = Color::White;
    bool m_castleWhiteKing = false;
    bool m_castleWhiteQueen = false;
    bool m_castleBlackKing = false;
    bool m_castleBlackQueen = false;
    int m_epSquare = -1;   // Square a pawn may capture onto, or -1.
    int m_halfmove = 0;
    int m_fullmove = 1;
};

// Counts leaf nodes at a fixed depth. The standard way to prove a move generator
// correct: the numbers are published for well known positions, so a single wrong
// rule shows up as a mismatch rather than as a subtly wrong game months later.
unsigned long long Perft(const ChessPosition& position, int depth);
