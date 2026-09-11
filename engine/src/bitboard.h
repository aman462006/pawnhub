// Bitboard utilities and precomputed attack tables for the "leaper" pieces.
//
// Leapers (knight, king, pawn) always attack the same shape relative to their square,
// so we precompute their attack sets once at startup and just look them up. Sliding
// pieces (bishop/rook/queen) depend on blockers and get the "magic bitboard" treatment
// in a later step.
#pragma once
#include <bit>
#include <string>
#include "types.h"

// A one-bit bitboard for a single square.
constexpr Bitboard square_bb(Square s) { return 1ULL << s; }

// Set / test / clear a square in a bitboard.
inline bool  get_bit(Bitboard b, Square s) { return b & square_bb(s); }
inline void  set_bit(Bitboard& b, Square s) { b |= square_bb(s); }
inline void  clear_bit(Bitboard& b, Square s) { b &= ~square_bb(s); }

// popcount = number of set bits = "how many pieces / squares". Hardware instruction.
inline int popcount(Bitboard b) { return std::popcount(b); }

// Least-significant set bit = lowest-index occupied square.
inline Square lsb(Bitboard b) { return Square(std::countr_zero(b)); }

// Return the lowest square AND remove it from the bitboard — the standard way to loop
// over every piece in a bitboard: `while (b) { Square s = pop_lsb(b); ... }`.
inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;              // clears the lowest set bit
    return s;
}

// Precomputed attack tables (filled by init_bitboards()).
extern Bitboard KnightAttacks[SQUARE_NB];
extern Bitboard KingAttacks[SQUARE_NB];
extern Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];   // squares a pawn *captures* to

// Must be called once at program start before any lookups.
void init_bitboards();

// Debug helpers.
std::string bb_to_string(Bitboard b);          // 8x8 picture, rank 8 on top
std::string square_to_string(Square s);        // e.g. "e4"
Square      string_to_square(const std::string& s);
