// Sliding-piece attacks (bishop / rook / queen) via "magic bitboards".
//
// The problem: unlike a knight, a rook's reachable squares depend on what's in the way.
// Naively you'd loop along each direction until you hit a piece — correct but slow, and
// we do this millions of times per second.
//
// The magic trick: for a given square, only a handful of squares can possibly block the
// piece (the "relevant occupancy mask"). We take the occupied bits under that mask,
// multiply by a specially-chosen "magic" 64-bit number, and shift down. That scrambles the
// blocker pattern into a small, collision-free index into a precomputed table of answers.
// End result: rook attacks = one multiply, one shift, one array lookup. No loop.
#pragma once
#include "types.h"
#include "bitboard.h"

void init_magics();  // must be called once at startup (after init_bitboards)

Bitboard bishop_attacks(Square s, Bitboard occ);
Bitboard rook_attacks(Square s, Bitboard occ);
inline Bitboard queen_attacks(Square s, Bitboard occ) {
    return bishop_attacks(s, occ) | rook_attacks(s, occ);
}

// The slow, obviously-correct reference (loops along rays). Kept for verifying the magic
// tables and for readability. `bishop=true` for diagonal movers, false for straight movers.
Bitboard sliding_attacks_slow(Square s, Bitboard occ, bool bishop);
