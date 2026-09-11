// Move generation, make/unmake, and perft.
#pragma once
#include <cstdint>
#include "position.h"
#include "move.h"

void init_movegen();  // fills castling-rights masks; call once at startup

// Everything that looks like a move by the rules of piece movement, but which might leave
// our own king in check (that's filtered next).
void generate_pseudo_legal(const Position& pos, MoveList& list);

// Only fully-legal moves (own king not left in check).
void generate_legal(Position& pos, MoveList& list);

// Information needed to undo a move — the bits of state that a move can silently change.
struct Undo {
    int      castling_rights;
    Square   ep_square;
    int      halfmove_clock;
    int      fullmove_number;
    PieceType captured;   // PIECE_TYPE_NB if the move was not a capture
};

void do_move(Position& pos, Move m, Undo& u);
void undo_move(Position& pos, Move m, const Undo& u);

// perft: count leaf nodes of the move tree to `depth`. The universal move-gen correctness test.
std::uint64_t perft(Position& pos, int depth);

// All pieces (either color) that attack square `s`, given occupancy `occ` (occ lets us reveal
// X-ray attackers as pieces are removed during a capture sequence).
Bitboard attackers_to(const Position& pos, Square s, Bitboard occ);

// Static Exchange Evaluation: the net material a capture wins or loses, assuming both sides
// keep recapturing on the target square with their least valuable attacker. Negative = the
// capture loses material. Call only on capture moves.
int see(const Position& pos, Move m);
