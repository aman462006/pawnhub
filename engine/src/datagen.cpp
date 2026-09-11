#include "datagen.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "zobrist.h"
#include "bitboard.h"
#include <fstream>
#include <vector>
#include <utility>
#include <cstdio>
#include <chrono>

namespace {

const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct Rng {
    std::uint64_t s;
    std::uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1DULL; }
};

// Same light "can't force mate" draw test used in the search.
bool insufficient(const Position& pos) {
    if (pos.pieces[WHITE][PAWN] | pos.pieces[BLACK][PAWN]
        | pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK]
        | pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]) return false;
    int minors = popcount(pos.pieces[WHITE][KNIGHT] | pos.pieces[WHITE][BISHOP]
                        | pos.pieces[BLACK][KNIGHT] | pos.pieces[BLACK][BISHOP]);
    return minors <= 1;
}

bool in_check(const Position& pos) {
    return pos.is_attacked(pos.king_square(pos.side_to_move), ~pos.side_to_move);
}

}  // namespace

void run_datagen(long long games, long long nodes, const std::string& out,
                 std::uint64_t seed, int opening_plies) {
    Rng rng{ seed ? seed : 0x1234567ULL };
    std::ofstream f(out, std::ios::binary);

    long long written = 0, games_done = 0;
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::pair<std::string, int>> buf;   // (fen, score_stm) for the current game
    std::vector<std::uint64_t> keys;                // position keys, for threefold detection

    for (long long g = 0; g < games; ++g) {
        Position pos; pos.set_fen(START_FEN);
        buf.clear(); keys.clear();

        // Randomised opening for variety.
        bool aborted = false;
        for (int i = 0; i < opening_plies; ++i) {
            MoveList l; generate_legal(pos, l);
            if (l.size == 0) { aborted = true; break; }
            Undo u; do_move(pos, l.moves[rng.next() % l.size], u);
        }
        if (aborted) continue;

        double result_white = 0.5;   // 1.0 White win, 0.5 draw, 0.0 Black win
        int ply = 0;
        while (true) {
            MoveList l; generate_legal(pos, l);
            if (l.size == 0) {                                   // mate or stalemate
                result_white = in_check(pos) ? (pos.side_to_move == WHITE ? 0.0 : 1.0) : 0.5;
                break;
            }
            if (pos.halfmove_clock >= 100 || insufficient(pos)) { result_white = 0.5; break; }

            std::uint64_t k = compute_key(pos);
            int seen = 0; for (std::uint64_t kk : keys) if (kk == k) ++seen;
            if (seen >= 2) { result_white = 0.5; break; }        // threefold repetition
            keys.push_back(k);
            if (ply >= 500) { result_white = 0.5; break; }       // safety cap

            SearchLimits lim; lim.time_limit = 0; lim.max_depth = 64; lim.max_nodes = nodes;
            SearchResult r = search(pos, lim);
            if (r.best == MOVE_NONE) { result_white = 0.5; break; }

            // Record only quiet, undecided positions (skip checks and near-mate scores).
            int mag = r.score < 0 ? -r.score : r.score;
            if (!in_check(pos) && mag < 20000)
                buf.emplace_back(pos.to_fen(), r.score);

            Undo u; do_move(pos, r.best, u);
            ++ply;
        }

        for (auto& pr : buf) {
            f << pr.first << ',' << pr.second << ',' << result_white << '\n';
            ++written;
        }
        if (++games_done % 500 == 0) {
            double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::fprintf(stderr, "  %lld games, %lld positions (%.0f games/s)\n",
                         games_done, written, games_done / dt);
        }
    }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::fprintf(stderr, "datagen done: %lld games, %lld positions in %.0fs -> %s\n",
                 games_done, written, dt, out.c_str());
}
