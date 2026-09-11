// A move packed into 16 bits. Compact moves keep move lists cache-friendly (important
// when we generate millions of them). Layout:
//   bits  0-5  : destination square (0..63)
//   bits  6-11 : origin square (0..63)
//   bits 12-13 : promotion piece, encoded as (piece - KNIGHT): 0=N,1=B,2=R,3=Q
//   bits 14-15 : move type: 0 normal, 1 promotion, 2 en passant, 3 castling
#pragma once
#include <cstdint>
#include <string>
#include "types.h"
#include "bitboard.h"

using Move = std::uint16_t;

enum MoveFlag : int {
    MF_NORMAL    = 0,
    MF_PROMO     = 1 << 14,
    MF_ENPASSANT = 2 << 14,
    MF_CASTLE    = 3 << 14,
};

constexpr Move MOVE_NONE = 0;

inline Move make_move(Square from, Square to, int flag = MF_NORMAL, PieceType promo = KNIGHT) {
    return Move(to | (from << 6) | ((promo - KNIGHT) << 12) | flag);
}
inline Square    move_to(Move m)    { return Square(m & 0x3F); }
inline Square    move_from(Move m)  { return Square((m >> 6) & 0x3F); }
inline int       move_flag(Move m)  { return m & (3 << 14); }
inline PieceType move_promo(Move m) { return PieceType(KNIGHT + ((m >> 12) & 3)); }

inline std::string move_to_uci(Move m) {
    if (m == MOVE_NONE) return "0000";
    std::string s = square_to_string(move_from(m)) + square_to_string(move_to(m));
    if (move_flag(m) == MF_PROMO) { const char* p = "nbrq"; s += p[(m >> 12) & 3]; }
    return s;
}

// A fixed-capacity move list (a position has at most ~218 legal moves).
struct MoveList {
    Move moves[256];
    int size = 0;
    void add(Move m) { moves[size++] = m; }
};
