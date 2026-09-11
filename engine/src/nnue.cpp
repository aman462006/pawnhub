#include "nnue.h"
#include "bitboard.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

namespace {

bool g_loaded = false;
bool g_enabled = false;
int  H = 0, QA = 0, QW = 0;
std::vector<std::int16_t> W1;       // 768 x H, row-major by feature (shared transformer)
std::vector<std::int32_t> AccBias;  // H  (b1)
std::vector<std::int16_t> W2;       // 2H (output weights)
std::int32_t OutBias = 0;           // centipawns (b2)

constexpr int MAXH = 512;

// One node's accumulators: v[perspective][hidden]. perspective 0 = White's view, 1 = Black's.
struct Acc { std::int32_t v[2][MAXH]; };

std::vector<Acc> g_stack;   // one Acc per ply of the search
int g_cur = 0;              // index of the current (top) accumulator

// Feature index for a piece as seen from `persp` (WHITE=0 / BLACK=1). Mirrors features.py.
inline int feat_index(int persp, int color, int pt, int sq) {
    int rel_color = (color == persp) ? 0 : 1;
    int rel_sq = (persp == WHITE) ? sq : (sq ^ 56);
    return (rel_color * 6 + pt) * 64 + rel_sq;
}

// Add / subtract a piece's contribution to BOTH perspective accumulators.
inline void acc_add(Acc& a, int color, int pt, int sq) {
    for (int P = 0; P < 2; ++P) {
        const std::int16_t* w = &W1[std::size_t(feat_index(P, color, pt, sq)) * H];
        for (int h = 0; h < H; ++h) a.v[P][h] += w[h];
    }
}
inline void acc_sub(Acc& a, int color, int pt, int sq) {
    for (int P = 0; P < 2; ++P) {
        const std::int16_t* w = &W1[std::size_t(feat_index(P, color, pt, sq)) * H];
        for (int h = 0; h < H; ++h) a.v[P][h] -= w[h];
    }
}

bool try_load(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "NNU2", 4) != 0) return false;
    std::int32_t h, qa, qw;
    f.read((char*)&h, 4); f.read((char*)&qa, 4); f.read((char*)&qw, 4);
    if (h > MAXH) return false;
    H = h; QA = qa; QW = qw;
    W1.resize(std::size_t(768) * H); f.read((char*)W1.data(), W1.size() * sizeof(std::int16_t));
    AccBias.resize(H);               f.read((char*)AccBias.data(), AccBias.size() * sizeof(std::int32_t));
    W2.resize(std::size_t(2) * H);   f.read((char*)W2.data(), W2.size() * sizeof(std::int16_t));
    f.read((char*)&OutBias, 4);
    g_stack.assign(300, Acc{});
    return (bool)f;
}

}  // namespace

bool nnue_load(const char* path) {
    const char* candidates[] = {
        path, "nets/nnue.bin", "../nets/nnue.bin", "../../nets/nnue.bin",
        "C:/PYTHON/chess/nets/nnue.bin",
    };
    for (const char* c : candidates)
        // The 14.9M-position net beats PeSTO by ~180 Elo, so NNUE is ON by default now.
        if (c && try_load(c)) { g_loaded = true; g_enabled = true; return true; }
    return false;
}

bool nnue_loaded() { return g_loaded; }
void nnue_set_enabled(bool on) { g_enabled = on; }
bool nnue_enabled() { return g_enabled; }

void nnue_refresh(const Position& pos) {
    Acc& a = g_stack[0];
    for (int P = 0; P < 2; ++P)
        for (int h = 0; h < H; ++h) a.v[P][h] = AccBias[h];
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt) {
            Bitboard b = pos.pieces[c][pt];
            while (b) acc_add(a, c, pt, pop_lsb(b));
        }
    g_cur = 0;
}

void nnue_make(Color mover, Move m, PieceType moving_pt, PieceType captured) {
    // Child accumulator = copy of parent, then apply only the few features this move changes.
    Acc& child = g_stack[g_cur + 1];
    const Acc& parent = g_stack[g_cur];
    std::memcpy(child.v[0], parent.v[0], sizeof(std::int32_t) * H);
    std::memcpy(child.v[1], parent.v[1], sizeof(std::int32_t) * H);
    ++g_cur;

    Square from = move_from(m), to = move_to(m);
    int flag = move_flag(m);
    Color opp = ~mover;

    acc_sub(child, mover, moving_pt, from);                  // the piece leaves 'from'
    if (flag == MF_PROMO) acc_add(child, mover, move_promo(m), to);
    else                  acc_add(child, mover, moving_pt, to);

    if (flag == MF_ENPASSANT) {
        acc_sub(child, opp, PAWN, make_square(file_of(to), rank_of(from)));
    } else if (captured != PIECE_TYPE_NB) {
        acc_sub(child, opp, captured, to);
    }

    if (flag == MF_CASTLE) {                                 // the rook also moves
        Square rf, rt;
        if (to == G1)      { rf = H1; rt = F1; }
        else if (to == C1) { rf = A1; rt = D1; }
        else if (to == G8) { rf = H8; rt = F8; }
        else               { rf = A8; rt = D8; }
        acc_sub(child, mover, ROOK, rf);
        acc_add(child, mover, ROOK, rt);
    }
}

void nnue_unmake() { --g_cur; }

namespace {
// Output layer applied to a given accumulator, read in (side-to-move, opponent) order.
int eval_acc(const Acc& a, Color stm) {
    const std::int32_t* own = a.v[stm];
    const std::int32_t* opp = a.v[~stm];
    long long out = 0;
    for (int h = 0; h < H; ++h) { int x = own[h] < 0 ? 0 : own[h]; out += (long long)W2[h] * x; }
    for (int h = 0; h < H; ++h) { int x = opp[h] < 0 ? 0 : opp[h]; out += (long long)W2[H + h] * x; }
    return int(out / ((long long)QA * QW)) + OutBias;
}
}  // namespace

int nnue_eval_stm(const Position& pos) {
    return eval_acc(g_stack[g_cur], pos.side_to_move);      // incremental: read current accumulator
}

int nnue_eval_recompute(const Position& pos) {
    // Full recompute into a local accumulator (doesn't touch the stack). Used to verify the
    // incremental version and as a safe fallback.
    Acc a;
    for (int P = 0; P < 2; ++P)
        for (int h = 0; h < H; ++h) a.v[P][h] = AccBias[h];
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt) {
            Bitboard b = pos.pieces[c][pt];
            while (b) acc_add(a, c, pt, pop_lsb(b));
        }
    return eval_acc(a, pos.side_to_move);
}
