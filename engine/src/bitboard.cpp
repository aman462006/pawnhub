#include "bitboard.h"

Bitboard KnightAttacks[SQUARE_NB];
Bitboard KingAttacks[SQUARE_NB];
Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];

namespace {

// Build an attack set by trying a list of (file, rank) offsets from a square and keeping
// only the ones that land on the board. Doing it with file/rank arithmetic (rather than
// bit shifts) makes edge-wrap bugs impossible — a knight on h1 can't "wrap" to a-file.
Bitboard leaper_attacks(Square s, const int offsets[][2], int n) {
    Bitboard attacks = 0;
    int f = file_of(s), r = rank_of(s);
    for (int i = 0; i < n; ++i) {
        int nf = f + offsets[i][0];
        int nr = r + offsets[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8)
            set_bit(attacks, make_square(nf, nr));
    }
    return attacks;
}

}  // namespace

void init_bitboards() {
    static const int knight[8][2] = {
        {+1, +2}, {+2, +1}, {+2, -1}, {+1, -2},
        {-1, -2}, {-2, -1}, {-2, +1}, {-1, +2},
    };
    static const int king[8][2] = {
        {+1, 0}, {+1, +1}, {0, +1}, {-1, +1},
        {-1, 0}, {-1, -1}, {0, -1}, {+1, -1},
    };
    // Pawns capture diagonally forward. "Forward" is +rank for White, -rank for Black.
    static const int white_pawn[2][2] = {{-1, +1}, {+1, +1}};
    static const int black_pawn[2][2] = {{-1, -1}, {+1, -1}};

    for (int s = 0; s < SQUARE_NB; ++s) {
        Square sq = Square(s);
        KnightAttacks[s] = leaper_attacks(sq, knight, 8);
        KingAttacks[s]   = leaper_attacks(sq, king, 8);
        PawnAttacks[WHITE][s] = leaper_attacks(sq, white_pawn, 2);
        PawnAttacks[BLACK][s] = leaper_attacks(sq, black_pawn, 2);
    }
}

std::string square_to_string(Square s) {
    if (s == SQ_NONE) return "-";
    return std::string(1, char('a' + file_of(s))) + char('1' + rank_of(s));
}

Square string_to_square(const std::string& s) {
    if (s.size() < 2 || s == "-") return SQ_NONE;
    return make_square(s[0] - 'a', s[1] - '1');
}

std::string bb_to_string(Bitboard b) {
    std::string out;
    for (int r = 7; r >= 0; --r) {           // rank 8 down to rank 1
        for (int f = 0; f < 8; ++f)
            out += get_bit(b, make_square(f, r)) ? " X" : " .";
        out += "\n";
    }
    return out;
}
