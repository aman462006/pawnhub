// Self-test harness for the engine foundation.
//
// This isn't the final engine — it's a scaffold that PROVES the pieces we've built so far
// are correct, using known facts (a knight on e4 attacks exactly 8 squares, the start
// position round-trips through FEN, etc.). Once move generation exists, perft takes over as
// the real correctness gate; for now these checks catch mistakes immediately.
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include "nnue.h"
#include "datagen.h"
#include "bitboard.h"
#include "position.h"
#include "magic.h"
#include "movegen.h"
#include "zobrist.h"
#include "eval.h"
#include "search.h"
#include "move.h"
#include "uci.h"

static int g_failures = 0;

static void check(const std::string& name, bool ok) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
    if (!ok) g_failures++;
}

static int run_selftests() {
    std::puts("== Leaper attack tables ==");
    check("knight on e4 attacks 8 squares", popcount(KnightAttacks[E4]) == 8);
    check("knight on a1 attacks 2 squares", popcount(KnightAttacks[A1]) == 2);
    check("knight on h8 attacks 2 squares", popcount(KnightAttacks[H8]) == 2);
    check("king on e1 attacks 5 squares",   popcount(KingAttacks[E1]) == 5);
    check("king on e4 attacks 8 squares",   popcount(KingAttacks[E4]) == 8);
    check("king on a1 attacks 3 squares",   popcount(KingAttacks[A1]) == 3);
    check("white pawn e2 captures d3 & f3",
          PawnAttacks[WHITE][E2] == (square_bb(D3) | square_bb(F3)));
    check("black pawn e7 captures d6 & f6",
          PawnAttacks[BLACK][E7] == (square_bb(D6) | square_bb(F6)));
    check("white pawn a2 captures only b3",
          PawnAttacks[WHITE][A2] == square_bb(B3));

    std::puts("\n== FEN parsing / round-trip ==");
    const std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Position pos;
    check("start position parses", pos.set_fen(start));
    check("start position has 32 pieces", popcount(pos.all) == 32);
    check("start FEN round-trips exactly", pos.to_fen() == start);

    const std::string sicilian = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2";
    Position pos2;
    check("mid-game FEN parses", pos2.set_fen(sicilian));
    check("mid-game FEN round-trips exactly", pos2.to_fen() == sicilian);
    check("en passant square read as c6", pos2.ep_square == C6);

    std::puts("\n== Sliding pieces (magic bitboards) ==");
    check("rook on a1 (empty board) attacks 14 squares", popcount(rook_attacks(A1, 0)) == 14);
    check("rook on e4 (empty board) attacks 14 squares", popcount(rook_attacks(E4, 0)) == 14);
    check("bishop on a1 (empty board) attacks 7 squares", popcount(bishop_attacks(A1, 0)) == 7);
    check("bishop on e4 (empty board) attacks 13 squares", popcount(bishop_attacks(E4, 0)) == 13);
    check("queen on d4 (empty board) attacks 27 squares", popcount(queen_attacks(D4, 0)) == 27);
    // A rook on a1 with a friendly/enemy piece on a4 should stop there (attacks a2,a3,a4).
    check("rook on a1 blocked at a4 stops at a4",
          rook_attacks(A1, square_bb(A4)) == (square_bb(A2) | square_bb(A3) | square_bb(A4)
                                              | square_bb(B1) | square_bb(C1) | square_bb(D1)
                                              | square_bb(E1) | square_bb(F1) | square_bb(G1)
                                              | square_bb(H1)));

    // The gold standard: for EVERY square and EVERY possible blocker layout on its relevant
    // squares, the fast magic lookup must equal the slow reference. If this passes, the magic
    // tables are provably correct.
    std::puts("-- exhaustive magic vs slow verification --");
    long long tested = 0; bool exhaustive_ok = true;
    for (int s = 0; s < 64 && exhaustive_ok; ++s) {
        for (int bishop = 0; bishop < 2 && exhaustive_ok; ++bishop) {
            // Re-derive the relevant mask to enumerate its subsets.
            Bitboard mask = 0;
            {   // subset enumeration needs the mask; recompute via slow-attack footprint trick:
                // easier: brute force all 64-bit not feasible, so rebuild mask like magic.cpp.
                int f = file_of(Square(s)), r = rank_of(Square(s));
                const int rd[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};
                const int bd[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                for (int d = 0; d < 4; ++d) {
                    int df = bishop ? bd[d][0] : rd[d][0];
                    int dr = bishop ? bd[d][1] : rd[d][1];
                    int nf = f + df, nr = r + dr;
                    while (nf + df >= 0 && nf + df < 8 && nr + dr >= 0 && nr + dr < 8) {
                        set_bit(mask, make_square(nf, nr)); nf += df; nr += dr;
                    }
                }
            }
            int n = popcount(mask);
            for (int idx = 0; idx < (1 << n); ++idx) {
                // Rebuild this subset as an occupancy.
                Bitboard occ = 0, m = mask;
                for (int i = 0; i < n; ++i) { Square sq = pop_lsb(m); if (idx & (1 << i)) set_bit(occ, sq); }
                Bitboard fast = bishop ? bishop_attacks(Square(s), occ) : rook_attacks(Square(s), occ);
                Bitboard ref  = sliding_attacks_slow(Square(s), occ, bishop);
                ++tested;
                if (fast != ref) { exhaustive_ok = false; break; }
            }
        }
    }
    check("exhaustive magic==slow over " + std::to_string(tested) + " cases", exhaustive_ok);

    std::puts("\n== PERFT (move generator correctness gate) ==");
    struct PerftCase { const char* name; const char* fen; int depth; std::uint64_t expected; };
    // The standard test suite. These positions are chosen to stress every rule: castling,
    // en passant, promotions, pins, and discovered checks. Reference counts are well-known.
    const PerftCase cases[] = {
        {"startpos d5",  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
        {"kiwipete d4",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
        {"position3 d5", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
        {"position4 d4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333},
        {"position5 d4", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487},
    };
    std::uint64_t total_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (const auto& c : cases) {
        Position p; p.set_fen(c.fen);
        std::uint64_t got = perft(p, c.depth);
        total_nodes += got;
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s = %llu (expected %llu)",
                      c.name, (unsigned long long)got, (unsigned long long)c.expected);
        check(buf, got == c.expected);
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::printf("  perft speed: %.0f k nodes/sec over %llu nodes in %.2fs\n",
                total_nodes / secs / 1000.0, (unsigned long long)total_nodes, secs);

    std::puts("\n== SEE (static exchange evaluation) ==");
    {
        auto see_of = [](const char* fen, const std::string& uci) {
            Position p; p.set_fen(fen);
            MoveList ml; generate_legal(p, ml);
            for (int i = 0; i < ml.size; ++i)
                if (move_to_uci(ml.moves[i]) == uci) return see(p, ml.moves[i]);
            return -999999;
        };
        // Pawn takes an undefended pawn: wins a pawn (+100).
        check("SEE exd5 (free pawn) = +100", see_of("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1", "e4d5") == 100);
        // Rook takes a knight defended by the e6 pawn: loses the exchange (knight 320 - rook 500 = -180).
        check("SEE Rxd5 (defended) = -180", see_of("4k3/8/4p3/3n4/8/8/8/3RK3 w - - 0 1", "d1d5") == -180);
    }

    std::puts("\n== Search (tactics the engine must find) ==");
    {
        auto fixed = [](int d) { SearchLimits l; l.time_limit = 0; l.max_depth = d; return l; };

        Position p;
        p.set_fen("4k3/8/8/8/3q4/8/8/3QK3 w - - 0 1");           // free black queen on d4
        SearchResult r = search(p, fixed(3));
        check("takes the free queen (d1d4)", move_to_uci(r.best) == "d1d4");

        p.set_fen("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1");          // back-rank mate in 1
        r = search(p, fixed(3));
        check("finds mate in 1 (Re8#)", move_to_uci(r.best) == "e1e8" && r.score >= 999000);

        p.set_fen("6k1/8/6K1/8/8/8/8/7Q w - - 0 1");             // K+Q mate in 2
        r = search(p, fixed(5));
        check("sees forced mate in 2 (score is mate)", r.score >= 999000);

        p.set_fen("4k3/8/2p5/8/8/8/8/3QK3 w - - 0 1");           // must not hang queen on d5
        r = search(p, fixed(4));
        check("does not hang its queen (avoids d1d5)", move_to_uci(r.best) != "d1d5");
    }

    std::puts("\n== NNUE (C++ inference must match Python export) ==");
    if (!nnue_loaded()) {
        check("nnue.bin loaded", false);
    } else {
        std::ifstream vf("nets/verify.csv");
        if (!vf) {
            check("verify.csv found", false);
        } else {
            std::string vline; int checked = 0; bool all_match = true;
            while (std::getline(vf, vline)) {
                if (vline.empty()) continue;
                auto comma = vline.rfind(',');
                std::string fen = vline.substr(0, comma);
                int expected = std::stoi(vline.substr(comma + 1));
                Position vp; vp.set_fen(fen);
                int got = nnue_eval_recompute(vp);
                if (got != expected) { all_match = false;
                    std::printf("    mismatch: %s  got %d expected %d\n", fen.c_str(), got, expected); }
                ++checked;
            }
            check("C++ NNUE matches Python on " + std::to_string(checked) + " positions", all_match);
        }

        // Incremental accumulator must always agree with a full recompute. Play sequences that
        // exercise captures, castling, en passant, and promotion; check after every move.
        auto play_and_check = [](const std::string& fen, const std::vector<std::string>& ucis) {
            Position p; p.set_fen(fen);
            nnue_refresh(p);
            for (const std::string& uci : ucis) {
                MoveList ml; generate_legal(p, ml);
                Move m = MOVE_NONE;
                for (int i = 0; i < ml.size; ++i) if (move_to_uci(ml.moves[i]) == uci) m = ml.moves[i];
                if (m == MOVE_NONE) { std::printf("    move not found/legal: %s\n", uci.c_str()); return false; }
                Color mover = p.side_to_move;
                PieceType mp = p.piece_type_on(move_from(m));
                Undo u; do_move(p, m, u); nnue_make(mover, m, mp, u.captured);
                if (nnue_eval_stm(p) != nnue_eval_recompute(p)) {
                    std::printf("    diverge after %s: incremental %d vs recompute %d\n",
                                uci.c_str(), nnue_eval_stm(p), nnue_eval_recompute(p));
                    return false;
                }
            }
            return true;
        };
        bool inc_ok = true;
        // Captures + kingside castling.
        inc_ok &= play_and_check("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            {"e2e4","c7c5","g1f3","d7d6","f1b5","c8d7","b5d7","d8d7","e1g1","g8f6"});
        // En passant + promotion (black king off the promotion lines, so no accidental checks).
        inc_ok &= play_and_check("8/P6p/8/3pP1k1/8/8/8/4K3 w - d6 0 1",
            {"e5d6","h7h6","a7a8q","g5g4","e1e2"});
        check("incremental accumulator matches full recompute", inc_ok);
    }

    std::puts("\n== Search speed (start position, 2.0s) ==");
    {
        Position p; p.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        SearchLimits l; l.time_limit = 2.0;
        auto s0 = std::chrono::steady_clock::now();
        SearchResult r = search(p, l);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - s0).count();
        std::printf("  best=%s  depth=%d  score=%d  nodes=%llu  (%.1f M nodes/sec)\n",
                    move_to_uci(r.best).c_str(), r.depth, r.score,
                    (unsigned long long)r.nodes, r.nodes / secs / 1e6);
    }

    std::puts("\n== Visual sanity (eyeball these) ==");
    std::printf("Start position:\n%s\n", pos.to_board_string().c_str());
    std::printf("Knight attacks from e4:\n%s\n", bb_to_string(KnightAttacks[E4]).c_str());

    std::printf("\n%s\n", g_failures == 0 ? "ALL CHECKS PASSED"
                                          : (std::to_string(g_failures) + " CHECK(S) FAILED").c_str());
    return g_failures == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    // Every mode needs the tables initialized first.
    init_bitboards();
    init_magics();
    init_movegen();
    init_zobrist();
    init_search();
    nnue_load();   // loads nets/nnue.bin if present; engine falls back to handcrafted eval if not

    // Default: behave as a UCI engine (so GUIs / python-chess can drive us).
    // `engine.exe --selftest` runs the internal correctness/tactics suite instead.
    if (argc > 1 && std::string(argv[1]) == "--selftest")
        return run_selftests();

    // Self-play data generation:
    //   engine.exe datagen <games> <nodes> <out.csv> <seed>
    if (argc > 1 && std::string(argv[1]) == "datagen") {
        long long games = argc > 2 ? std::atoll(argv[2]) : 1000;
        long long nodes = argc > 3 ? std::atoll(argv[3]) : 5000;
        std::string out = argc > 4 ? argv[4] : "data/selfplay.csv";
        std::uint64_t seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 1;
        run_datagen(games, nodes, out, seed, 8);
        return 0;
    }

    uci_loop();
    return 0;
}
