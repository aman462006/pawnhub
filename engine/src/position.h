// The board: what's where, whose turn, and the little bits of state chess needs
// (castling rights, en passant, move clocks).
//
// We store the position as "one bitboard per piece kind" — pieces[WHITE][KNIGHT] has a bit
// set on every square holding a white knight. This redundant-looking layout is what makes
// move generation fast: "all white knights" is already a single number, ready for bitwise math.
#pragma once
#include <string>
#include "types.h"
#include "bitboard.h"

struct Position {
    Bitboard pieces[COLOR_NB][PIECE_TYPE_NB] = {};  // per color, per piece type
    Bitboard occupied[COLOR_NB] = {};               // all pieces of one color
    Bitboard all = 0;                               // every occupied square

    Color side_to_move = WHITE;
    int   castling_rights = NO_CASTLING;
    Square ep_square = SQ_NONE;      // the square a pawn could capture en passant onto
    int   halfmove_clock = 0;        // for the 50-move rule
    int   fullmove_number = 1;

    // Rebuild `occupied[]` and `all` from the per-piece bitboards (call after edits).
    void refresh_aggregates();

    // Load a position from Forsyth-Edwards Notation, the standard text format.
    // e.g. "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    bool set_fen(const std::string& fen);
    std::string to_fen() const;

    // What piece sits on a square? Returns PIECE_TYPE_NB if empty.
    PieceType piece_type_on(Square s) const;
    // Human-readable ASCII board (White at the bottom), for debugging.
    std::string to_board_string() const;

    // The square of `c`'s king.
    Square king_square(Color c) const { return lsb(pieces[c][KING]); }
    // Is square `s` attacked by any piece of color `by`? (Used for legality + castling.)
    bool is_attacked(Square s, Color by) const;
};
