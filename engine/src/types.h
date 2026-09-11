// Fundamental types for the engine.
//
// A "bitboard" is a 64-bit integer where each bit is one square of the board: bit 0 = a1,
// bit 1 = b1, ... bit 63 = h8. This layout matches python-chess exactly, which makes
// cross-checking against our Python oracle painless. One bitboard can represent "all the
// white pawns" or "every square a knight on e4 attacks" — and set operations (union,
// intersection) become single CPU instructions (| and &).
#pragma once
#include <cstdint>

using Bitboard = std::uint64_t;

enum Color : int { WHITE, BLACK, COLOR_NB = 2 };

// Order matters: it lines up with the values used in evaluation and move encoding.
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_TYPE_NB = 6 };

// Squares, a1..h8, with a1 = 0. Reading a rank left-to-right increments the index.
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NONE = -1, SQUARE_NB = 64
};

// Castling rights as bit flags, OR-ed together into one int.
enum CastlingRight : int {
    NO_CASTLING = 0,
    WHITE_OO = 1, WHITE_OOO = 2,   // White king-side / queen-side
    BLACK_OO = 4, BLACK_OOO = 8,
};

// --- tiny square helpers -----------------------------------------------------------
constexpr int file_of(Square s) { return s & 7; }         // 0=a .. 7=h  (low 3 bits)
constexpr int rank_of(Square s) { return s >> 3; }        // 0=rank1 .. 7=rank8
constexpr Square make_square(int file, int rank) { return Square(rank * 8 + file); }
constexpr Color operator~(Color c) { return Color(c ^ 1); }  // opposite color
