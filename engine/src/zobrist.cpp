#include "zobrist.h"
#include "bitboard.h"

std::uint64_t ZPiece[COLOR_NB][PIECE_TYPE_NB][64];
std::uint64_t ZSide;
std::uint64_t ZCastle[16];
std::uint64_t ZEp[8];

namespace {
struct Rng {
    std::uint64_t s = 0x9E3779B97F4A7C15ULL;
    std::uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1DULL; }
};
}

void init_zobrist() {
    Rng rng;
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt)
            for (int s = 0; s < 64; ++s)
                ZPiece[c][pt][s] = rng.next();
    ZSide = rng.next();
    for (int i = 0; i < 16; ++i) ZCastle[i] = rng.next();
    for (int i = 0; i < 8; ++i)  ZEp[i] = rng.next();
}

std::uint64_t compute_key(const Position& pos) {
    std::uint64_t k = 0;
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt) {
            Bitboard b = pos.pieces[c][pt];
            while (b) k ^= ZPiece[c][pt][pop_lsb(b)];
        }
    if (pos.side_to_move == BLACK) k ^= ZSide;
    k ^= ZCastle[pos.castling_rights & 15];
    if (pos.ep_square != SQ_NONE) k ^= ZEp[file_of(pos.ep_square)];
    return k;
}
