#include "magic.h"
#include <vector>

namespace {

// Ray directions as (file, rank) steps.
const int ROOK_DIRS[4][2]   = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
const int BISHOP_DIRS[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

// Slow reference: walk each direction, adding squares until (and including) the first
// blocker. This is the "ground truth" the magic tables must match exactly.
Bitboard slow(Square s, Bitboard occ, bool bishop) {
    Bitboard attacks = 0;
    int f = file_of(s), r = rank_of(s);
    const auto& dirs = bishop ? BISHOP_DIRS : ROOK_DIRS;
    for (auto& d : dirs) {
        int nf = f + d[0], nr = r + d[1];
        while (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
            Square t = make_square(nf, nr);
            set_bit(attacks, t);
            if (occ & square_bb(t)) break;   // blocked here; stop this ray
            nf += d[0]; nr += d[1];
        }
    }
    return attacks;
}

// The "relevant occupancy" squares: interior squares of each ray (edges excluded, because a
// blocker on the final edge square doesn't change anything behind it — there's nothing behind).
Bitboard relevant_mask(Square s, bool bishop) {
    Bitboard mask = 0;
    int f = file_of(s), r = rank_of(s);
    const auto& dirs = bishop ? BISHOP_DIRS : ROOK_DIRS;
    for (auto& d : dirs) {
        int nf = f + d[0], nr = r + d[1];
        // Include this square only if there's a further square beyond it (i.e. not an edge).
        while (nf + d[0] >= 0 && nf + d[0] < 8 && nr + d[1] >= 0 && nr + d[1] < 8) {
            set_bit(mask, make_square(nf, nr));
            nf += d[0]; nr += d[1];
        }
    }
    return mask;
}

// Turn an integer 0..2^n-1 into one particular blocker arrangement on the mask's bits.
// Enumerating all integers thus enumerates every possible blocker configuration.
Bitboard index_to_occupancy(int index, Bitboard mask) {
    Bitboard occ = 0;
    int n = popcount(mask);
    for (int i = 0; i < n; ++i) {
        Square sq = pop_lsb(mask);           // consumes the lowest bit of a local copy
        if (index & (1 << i)) set_bit(occ, sq);
    }
    return occ;
}

// A small, fast PRNG. Fixed seed => the same magics are found every run (reproducible).
struct Rng {
    std::uint64_t s = 0x2545F4914F6CDD1DULL;
    std::uint64_t next() {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 0x2545F4914F6CDD1DULL;
    }
    std::uint64_t sparse() { return next() & next() & next(); }  // few bits set: better magics
};

struct Magic {
    Bitboard mask = 0;
    Bitboard magic = 0;
    unsigned shift = 0;
    Bitboard* table = nullptr;   // points into the big attack tables below
};

// Attack tables. Rook needs up to 2^12 entries per square, bishop up to 2^9.
Bitboard RookTable[64][4096];
Bitboard BishopTable[64][512];
Magic RookM[64];
Magic BishopM[64];

// Search for a magic number that maps every blocker configuration to a distinct slot
// (or to slots that happen to hold the same attack set — a harmless collision).
void find_magic(Square s, bool bishop, Rng& rng) {
    Bitboard mask = relevant_mask(s, bishop);
    int n = popcount(mask);
    int size = 1 << n;
    unsigned shift = 64 - n;
    Bitboard* table = bishop ? BishopTable[s] : RookTable[s];

    std::vector<Bitboard> occ(size), ref(size);
    for (int i = 0; i < size; ++i) {
        occ[i] = index_to_occupancy(i, mask);
        ref[i] = slow(s, occ[i], bishop);
    }

    std::vector<Bitboard> used(size);
    std::vector<int> epoch(size, 0);
    int current = 0;

    while (true) {
        Bitboard magic = rng.sparse();
        // Quick reject: a good magic spreads the top bits of mask*magic well.
        if (popcount((mask * magic) >> 56) < 6) continue;

        ++current;
        bool ok = true;
        for (int i = 0; i < size && ok; ++i) {
            unsigned idx = unsigned((occ[i] * magic) >> shift);
            if (epoch[idx] != current) {         // slot unused this attempt
                epoch[idx] = current;
                used[idx] = ref[i];
            } else if (used[idx] != ref[i]) {    // real collision -> this magic fails
                ok = false;
            }
        }
        if (ok) {
            for (int i = 0; i < size; ++i)
                table[(occ[i] * magic) >> shift] = ref[i];
            Magic& m = bishop ? BishopM[s] : RookM[s];
            m = {mask, magic, shift, table};
            return;
        }
    }
}

}  // namespace

void init_magics() {
    Rng rng;
    for (int s = 0; s < 64; ++s) {
        find_magic(Square(s), /*bishop=*/false, rng);
        find_magic(Square(s), /*bishop=*/true,  rng);
    }
}

Bitboard rook_attacks(Square s, Bitboard occ) {
    const Magic& m = RookM[s];
    return m.table[((occ & m.mask) * m.magic) >> m.shift];
}

Bitboard bishop_attacks(Square s, Bitboard occ) {
    const Magic& m = BishopM[s];
    return m.table[((occ & m.mask) * m.magic) >> m.shift];
}

Bitboard sliding_attacks_slow(Square s, Bitboard occ, bool bishop) {
    return slow(s, occ, bishop);
}
