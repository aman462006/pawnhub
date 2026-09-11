// Zobrist hashing: a 64-bit fingerprint of a position, used as the transposition-table key.
//
// Idea: give every (piece, color, square) combination a fixed random 64-bit number, plus
// numbers for side-to-move / castling rights / en-passant file. XOR together the ones that
// apply to a position. Two positions that are identical get the same key; different ones
// almost never collide. XOR is reversible, so this can also be updated incrementally later.
#pragma once
#include <cstdint>
#include "types.h"
#include "position.h"

extern std::uint64_t ZPiece[COLOR_NB][PIECE_TYPE_NB][64];
extern std::uint64_t ZSide;
extern std::uint64_t ZCastle[16];
extern std::uint64_t ZEp[8];

void init_zobrist();
std::uint64_t compute_key(const Position& pos);
