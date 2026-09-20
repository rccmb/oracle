#include "chess/ChessRules.h"

#include <cctype>
#include <sstream>

namespace {

inline int FileOf(int square) { return square % 8; }
inline int RankIndexOf(int square) { return square / 8; } // 0 is rank 8, 7 is rank 1.
inline int SquareAt(int file, int rankIndex) { return rankIndex * 8 + file; }
inline bool OnBoard(int file, int rankIndex) {
    return file >= 0 && file < 8 && rankIndex >= 0 && rankIndex < 8;
}

inline bool IsWhitePiece(char piece) { return piece >= 'A' && piece <= 'Z'; }
inline bool IsBlackPiece(char piece) { return piece >= 'a' && piece <= 'z'; }
inline bool IsEmpty(char piece) { return piece == ' '; }

inline bool BelongsTo(char piece, Color color) {
    return color == Color::White ? IsWhitePiece(piece) : IsBlackPiece(piece);
}

// Deltas are (file, rankIndex). A positive rank delta moves down the board,
// toward rank 1, because rank index 0 is rank 8.
const int kKnightDeltas[8][2] = { {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2} };
const int kKingDeltas[8][2] = { {1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1} };
const int kBishopDeltas[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
const int kRookDeltas[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };

const char kPromotionPieces[4] = { 'q', 'r', 'b', 'n' };

// Home squares, in the same index space.
const int kWhiteKingHome = 60; // e1
const int kBlackKingHome = 4;  // e8
const int kWhiteRookKingSide = 63;  // h1
const int kWhiteRookQueenSide = 56; // a1
const int kBlackRookKingSide = 7;   // h8
const int kBlackRookQueenSide = 0;  // a8

std::string SquareName(int square) {
    std::string name;
    name += (char)('a' + FileOf(square));
    name += (char)('8' - RankIndexOf(square));
    return name;
}

} // namespace

std::string Move::ToUci() const {
    if (from < 0 || to < 0) return std::string();
    std::string uci = SquareName(from) + SquareName(to);
    if (promotion) uci += (char)std::tolower((unsigned char)promotion);
    return uci;
}

ChessPosition::ChessPosition() {
    Clear();
}

void ChessPosition::Clear() {
    for (int i = 0; i < 64; ++i) m_board[i] = ' ';
    m_sideToMove = Color::White;
    m_castleWhiteKing = m_castleWhiteQueen = false;
    m_castleBlackKing = m_castleBlackQueen = false;
    m_epSquare = -1;
    m_halfmove = 0;
    m_fullmove = 1;
}

ChessPosition ChessPosition::StartingPosition() {
    return *FromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

std::optional<ChessPosition> ChessPosition::FromFen(const std::string& fen) {
    ChessPosition position;
    std::istringstream stream(fen);

    std::string placement, side, castling, enPassant;
    if (!(stream >> placement >> side)) return std::nullopt;

    int square = 0;
    for (char symbol : placement) {
        if (symbol == '/') continue;
        if (symbol >= '1' && symbol <= '8') {
            square += symbol - '0';
            continue;
        }
        if (square >= 64) return std::nullopt;
        if (!std::isalpha((unsigned char)symbol)) return std::nullopt;
        position.m_board[square++] = symbol;
    }
    if (square != 64) return std::nullopt;

    if (side == "w") position.m_sideToMove = Color::White;
    else if (side == "b") position.m_sideToMove = Color::Black;
    else return std::nullopt;

    // The remaining fields are optional: a FEN truncated after the side to move
    // is still enough to describe a position.
    if (stream >> castling) {
        position.m_castleWhiteKing = castling.find('K') != std::string::npos;
        position.m_castleWhiteQueen = castling.find('Q') != std::string::npos;
        position.m_castleBlackKing = castling.find('k') != std::string::npos;
        position.m_castleBlackQueen = castling.find('q') != std::string::npos;
    }

    if (stream >> enPassant && enPassant != "-" && enPassant.size() >= 2) {
        const int file = enPassant[0] - 'a';
        const int rankIndex = '8' - enPassant[1];
        if (OnBoard(file, rankIndex)) position.m_epSquare = SquareAt(file, rankIndex);
    }

    int halfmove = 0, fullmove = 1;
    if (stream >> halfmove) position.m_halfmove = halfmove;
    if (stream >> fullmove) position.m_fullmove = fullmove;

    return position;
}

std::string ChessPosition::ToFen() const {
    std::ostringstream fen;

    for (int rankIndex = 0; rankIndex < 8; ++rankIndex) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const char piece = m_board[SquareAt(file, rankIndex)];
            if (IsEmpty(piece)) { ++empty; continue; }
            if (empty > 0) { fen << empty; empty = 0; }
            fen << piece;
        }
        if (empty > 0) fen << empty;
        if (rankIndex < 7) fen << '/';
    }

    fen << ' ' << (m_sideToMove == Color::White ? 'w' : 'b') << ' ';

    std::string castling;
    if (m_castleWhiteKing) castling += 'K';
    if (m_castleWhiteQueen) castling += 'Q';
    if (m_castleBlackKing) castling += 'k';
    if (m_castleBlackQueen) castling += 'q';
    fen << (castling.empty() ? "-" : castling) << ' ';

    fen << (m_epSquare >= 0 ? SquareName(m_epSquare) : "-");
    fen << ' ' << m_halfmove << ' ' << m_fullmove;

    return fen.str();
}

void ChessPosition::CopyBoard(char out[64]) const {
    for (int i = 0; i < 64; ++i) out[i] = m_board[i];
}

bool ChessPosition::SameBoard(const char other[64]) const {
    for (int i = 0; i < 64; ++i) {
        if (m_board[i] != other[i]) return false;
    }
    return true;
}

int ChessPosition::FindKing(Color color) const {
    const char king = (color == Color::White) ? 'K' : 'k';
    for (int i = 0; i < 64; ++i) {
        if (m_board[i] == king) return i;
    }
    return -1;
}

bool ChessPosition::IsSquareAttacked(int square, Color byColor) const {
    if (square < 0 || square >= 64) return false;

    const int file = FileOf(square);
    const int rankIndex = RankIndexOf(square);

    // Pawns. A white pawn captures toward rank 8, so the ones able to reach this
    // square sit one rank below it.
    {
        const int pawnRank = (byColor == Color::White) ? rankIndex + 1 : rankIndex - 1;
        const char pawn = (byColor == Color::White) ? 'P' : 'p';
        for (int side = -1; side <= 1; side += 2) {
            const int pawnFile = file + side;
            if (!OnBoard(pawnFile, pawnRank)) continue;
            if (m_board[SquareAt(pawnFile, pawnRank)] == pawn) return true;
        }
    }

    // Knights.
    {
        const char knight = (byColor == Color::White) ? 'N' : 'n';
        for (const auto& delta : kKnightDeltas) {
            const int f = file + delta[0];
            const int r = rankIndex + delta[1];
            if (!OnBoard(f, r)) continue;
            if (m_board[SquareAt(f, r)] == knight) return true;
        }
    }

    // Kings.
    {
        const char king = (byColor == Color::White) ? 'K' : 'k';
        for (const auto& delta : kKingDeltas) {
            const int f = file + delta[0];
            const int r = rankIndex + delta[1];
            if (!OnBoard(f, r)) continue;
            if (m_board[SquareAt(f, r)] == king) return true;
        }
    }

    // Sliding pieces: walk out until something blocks, then ask what it was.
    auto scan = [&](const int deltas[][2], int count, char straight, char queen) {
        for (int i = 0; i < count; ++i) {
            int f = file + deltas[i][0];
            int r = rankIndex + deltas[i][1];
            while (OnBoard(f, r)) {
                const char piece = m_board[SquareAt(f, r)];
                if (!IsEmpty(piece)) {
                    if (piece == straight || piece == queen) return true;
                    break;
                }
                f += deltas[i][0];
                r += deltas[i][1];
            }
        }
        return false;
    };

    const char bishop = (byColor == Color::White) ? 'B' : 'b';
    const char rook = (byColor == Color::White) ? 'R' : 'r';
    const char queen = (byColor == Color::White) ? 'Q' : 'q';

    if (scan(kBishopDeltas, 4, bishop, queen)) return true;
    if (scan(kRookDeltas, 4, rook, queen)) return true;

    return false;
}

bool ChessPosition::IsInCheck(Color color) const {
    const int king = FindKing(color);
    if (king < 0) return false;
    return IsSquareAttacked(king, Opposite(color));
}

void ChessPosition::AddPawnMoves(int from, std::vector<Move>& out) const {
    const bool white = (m_sideToMove == Color::White);
    const int forward = white ? -1 : 1;           // In rank index space.
    const int startRank = white ? 6 : 1;
    const int promotionRank = white ? 0 : 7;

    const int file = FileOf(from);
    const int rankIndex = RankIndexOf(from);

    auto emit = [&](int to, bool capture, bool enPassant) {
        if (RankIndexOf(to) == promotionRank) {
            for (char piece : kPromotionPieces) {
                Move move;
                move.from = from;
                move.to = to;
                move.promotion = piece;
                move.isCapture = capture;
                out.push_back(move);
            }
            return;
        }
        Move move;
        move.from = from;
        move.to = to;
        move.isCapture = capture;
        move.isEnPassant = enPassant;
        out.push_back(move);
    };

    // Single and double push.
    const int oneRank = rankIndex + forward;
    if (OnBoard(file, oneRank)) {
        const int one = SquareAt(file, oneRank);
        if (IsEmpty(m_board[one])) {
            emit(one, false, false);

            const int twoRank = rankIndex + 2 * forward;
            if (rankIndex == startRank && OnBoard(file, twoRank)) {
                const int two = SquareAt(file, twoRank);
                if (IsEmpty(m_board[two])) emit(two, false, false);
            }
        }
    }

    // Captures, including en passant.
    for (int side = -1; side <= 1; side += 2) {
        const int captureFile = file + side;
        if (!OnBoard(captureFile, oneRank)) continue;

        const int target = SquareAt(captureFile, oneRank);
        const char piece = m_board[target];

        if (!IsEmpty(piece) && !BelongsTo(piece, m_sideToMove)) {
            emit(target, true, false);
        }
        else if (target == m_epSquare && IsEmpty(piece)) {
            emit(target, true, true);
        }
    }
}

void ChessPosition::AddStepMoves(int from, const int* deltas, int count, bool sliding,
                                 std::vector<Move>& out) const {
    const int file = FileOf(from);
    const int rankIndex = RankIndexOf(from);

    for (int i = 0; i < count; ++i) {
        const int deltaFile = deltas[i * 2];
        const int deltaRank = deltas[i * 2 + 1];

        int f = file + deltaFile;
        int r = rankIndex + deltaRank;

        while (OnBoard(f, r)) {
            const int target = SquareAt(f, r);
            const char piece = m_board[target];

            if (IsEmpty(piece)) {
                Move move;
                move.from = from;
                move.to = target;
                out.push_back(move);
            }
            else {
                if (!BelongsTo(piece, m_sideToMove)) {
                    Move move;
                    move.from = from;
                    move.to = target;
                    move.isCapture = true;
                    out.push_back(move);
                }
                break;
            }

            if (!sliding) break;
            f += deltaFile;
            r += deltaRank;
        }
    }
}

void ChessPosition::AddCastlingMoves(std::vector<Move>& out) const {
    const bool white = (m_sideToMove == Color::White);
    const Color enemy = Opposite(m_sideToMove);

    const int kingHome = white ? kWhiteKingHome : kBlackKingHome;
    const char king = white ? 'K' : 'k';
    const char rook = white ? 'R' : 'r';

    if (m_board[kingHome] != king) return;
    if (IsSquareAttacked(kingHome, enemy)) return; // Cannot castle out of check.

    const bool kingSide = white ? m_castleWhiteKing : m_castleBlackKing;
    const bool queenSide = white ? m_castleWhiteQueen : m_castleBlackQueen;

    if (kingSide) {
        const int rookSquare = white ? kWhiteRookKingSide : kBlackRookKingSide;
        const int through = kingHome + 1;
        const int destination = kingHome + 2;
        if (m_board[rookSquare] == rook &&
            IsEmpty(m_board[through]) && IsEmpty(m_board[destination]) &&
            !IsSquareAttacked(through, enemy) && !IsSquareAttacked(destination, enemy)) {
            Move move;
            move.from = kingHome;
            move.to = destination;
            move.isCastle = true;
            out.push_back(move);
        }
    }

    if (queenSide) {
        const int rookSquare = white ? kWhiteRookQueenSide : kBlackRookQueenSide;
        const int through = kingHome - 1;
        const int destination = kingHome - 2;
        const int rookPath = kingHome - 3; // b1 or b8 only has to be empty.
        if (m_board[rookSquare] == rook &&
            IsEmpty(m_board[through]) && IsEmpty(m_board[destination]) && IsEmpty(m_board[rookPath]) &&
            !IsSquareAttacked(through, enemy) && !IsSquareAttacked(destination, enemy)) {
            Move move;
            move.from = kingHome;
            move.to = destination;
            move.isCastle = true;
            out.push_back(move);
        }
    }
}

std::vector<Move> ChessPosition::LegalMoves() const {
    std::vector<Move> pseudo;
    pseudo.reserve(64);

    for (int square = 0; square < 64; ++square) {
        const char piece = m_board[square];
        if (IsEmpty(piece) || !BelongsTo(piece, m_sideToMove)) continue;

        switch (std::tolower((unsigned char)piece)) {
        case 'p': AddPawnMoves(square, pseudo); break;
        case 'n': AddStepMoves(square, &kKnightDeltas[0][0], 8, false, pseudo); break;
        case 'b': AddStepMoves(square, &kBishopDeltas[0][0], 4, true, pseudo); break;
        case 'r': AddStepMoves(square, &kRookDeltas[0][0], 4, true, pseudo); break;
        case 'q': AddStepMoves(square, &kKingDeltas[0][0], 8, true, pseudo); break;
        case 'k': AddStepMoves(square, &kKingDeltas[0][0], 8, false, pseudo); break;
        default: break;
        }
    }

    AddCastlingMoves(pseudo);

    // A move is legal when it does not leave one's own king attacked. Castling
    // has already checked the squares the king crosses.
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    for (const Move& move : pseudo) {
        const ChessPosition next = AfterMove(move);
        if (!next.IsInCheck(m_sideToMove)) legal.push_back(move);
    }

    return legal;
}

ChessPosition ChessPosition::AfterMove(const Move& move) const {
    ChessPosition next = *this;

    const char piece = m_board[move.from];
    const char captured = m_board[move.to];
    const bool white = (m_sideToMove == Color::White);
    const char lower = (char)std::tolower((unsigned char)piece);

    next.m_board[move.from] = ' ';
    next.m_board[move.to] = piece;

    if (move.isEnPassant) {
        // The captured pawn stands beside the moving pawn, not on the square it
        // lands on.
        const int capturedSquare = white ? move.to + 8 : move.to - 8;
        if (capturedSquare >= 0 && capturedSquare < 64) next.m_board[capturedSquare] = ' ';
    }

    if (move.promotion) {
        next.m_board[move.to] = white
            ? (char)std::toupper((unsigned char)move.promotion)
            : (char)std::tolower((unsigned char)move.promotion);
    }

    if (move.isCastle) {
        // The king has already moved; bring the rook round it.
        if (move.to > move.from) { // Kingside.
            const int rookFrom = move.from + 3;
            const int rookTo = move.from + 1;
            next.m_board[rookTo] = next.m_board[rookFrom];
            next.m_board[rookFrom] = ' ';
        }
        else { // Queenside.
            const int rookFrom = move.from - 4;
            const int rookTo = move.from - 1;
            next.m_board[rookTo] = next.m_board[rookFrom];
            next.m_board[rookFrom] = ' ';
        }
    }

    // A king or rook leaving home ends the rights that depend on it, and a rook
    // captured on its own corner ends the opponent's.
    if (lower == 'k') {
        if (white) { next.m_castleWhiteKing = false; next.m_castleWhiteQueen = false; }
        else { next.m_castleBlackKing = false; next.m_castleBlackQueen = false; }
    }
    if (move.from == kWhiteRookKingSide || move.to == kWhiteRookKingSide) next.m_castleWhiteKing = false;
    if (move.from == kWhiteRookQueenSide || move.to == kWhiteRookQueenSide) next.m_castleWhiteQueen = false;
    if (move.from == kBlackRookKingSide || move.to == kBlackRookKingSide) next.m_castleBlackKing = false;
    if (move.from == kBlackRookQueenSide || move.to == kBlackRookQueenSide) next.m_castleBlackQueen = false;

    // A double push leaves a square the opponent may capture onto.
    next.m_epSquare = -1;
    if (lower == 'p') {
        const int rankDelta = RankIndexOf(move.to) - RankIndexOf(move.from);
        if (rankDelta == 2 || rankDelta == -2) {
            next.m_epSquare = (move.from + move.to) / 2;
        }
    }

    // Reset on a pawn move or a capture, count up otherwise.
    const bool captureMade = !IsEmpty(captured) || move.isEnPassant;
    next.m_halfmove = (lower == 'p' || captureMade) ? 0 : m_halfmove + 1;

    if (!white) next.m_fullmove = m_fullmove + 1;
    next.m_sideToMove = Opposite(m_sideToMove);

    return next;
}

unsigned long long Perft(const ChessPosition& position, int depth) {
    if (depth <= 0) return 1;

    const std::vector<Move> moves = position.LegalMoves();
    if (depth == 1) return moves.size();

    unsigned long long nodes = 0;
    for (const Move& move : moves) {
        nodes += Perft(position.AfterMove(move), depth - 1);
    }
    return nodes;
}
