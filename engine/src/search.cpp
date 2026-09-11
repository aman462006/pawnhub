#include "search.h"
#include "movegen.h"
#include "eval.h"
#include "nnue.h"
#include "zobrist.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace {

constexpr int MATE  = 1000000;
constexpr int MATE_THRESHOLD = MATE - 1000;   // scores beyond this mean a forced mate
constexpr int INF   = 2000000;
constexpr int MAX_PLY = 128;

enum Bound : std::uint8_t { B_EXACT, B_LOWER, B_UPPER };

struct TTEntry {
    std::uint64_t key = 0;
    Move   move  = MOVE_NONE;
    std::int16_t score = 0;
    std::int8_t  depth = -1;
    std::uint8_t flag  = B_EXACT;
};

// Transposition table sized to a power of two so indexing is a cheap bitmask.
constexpr std::size_t TT_BITS = 22;             // 2^22 entries (~64 MB)
constexpr std::size_t TT_SIZE = std::size_t(1) << TT_BITS;
constexpr std::size_t TT_MASK = TT_SIZE - 1;
std::vector<TTEntry> TT;

// Per-search state.
std::uint64_t g_nodes;
bool          g_stop;
bool          g_nnue_on;   // is the NNUE accumulator being maintained this search?
long long     g_node_limit; // per-move node budget (0 = unlimited)
bool          g_use_deadline;
std::chrono::steady_clock::time_point g_deadline;
Move g_killers[MAX_PLY][2];
int  g_history[COLOR_NB][64][64];
std::uint64_t g_path_key[MAX_PLY];   // Zobrist key at each ply of the current search line

inline bool time_up() {
    return g_use_deadline && std::chrono::steady_clock::now() >= g_deadline;
}

inline bool is_capture(const Position& pos, Move m) {
    return move_flag(m) == MF_ENPASSANT || pos.piece_type_on(move_to(m)) != PIECE_TYPE_NB;
}

bool has_non_pawn(const Position& pos) {
    Color c = pos.side_to_move;
    return pos.pieces[c][KNIGHT] | pos.pieces[c][BISHOP] |
           pos.pieces[c][ROOK]   | pos.pieces[c][QUEEN];
}

// Very light draw check: no pawns/rooks/queens and at most one minor => can't force mate.
bool insufficient_material(const Position& pos) {
    for (int c = 0; c < COLOR_NB; ++c)
        if (pos.pieces[c][PAWN] | pos.pieces[c][ROOK] | pos.pieces[c][QUEEN]) return false;
    int minors = popcount(pos.pieces[WHITE][KNIGHT] | pos.pieces[WHITE][BISHOP] |
                          pos.pieces[BLACK][KNIGHT] | pos.pieces[BLACK][BISHOP]);
    return minors <= 1;
}

inline bool in_check(const Position& pos) {
    return pos.is_attacked(pos.king_square(pos.side_to_move), ~pos.side_to_move);
}

// Evaluation dispatch: use the neural net if it's loaded and enabled, else the handcrafted
// eval. This one indirection is what lets us A/B test NNUE vs handcrafted with the same binary.
inline int eval_stm(const Position& pos) {
    return g_nnue_on ? nnue_eval_stm(pos) : evaluate_stm(pos);
}

// Descend into a move while keeping the NNUE accumulator in sync. Must be paired with
// leave_move() around every REAL search descent (not the legality-test do/undo in movegen).
inline void enter_move(Position& pos, Move m, Undo& u) {
    Color mover = pos.side_to_move;
    PieceType moving_pt = pos.piece_type_on(move_from(m));
    do_move(pos, m, u);
    if (g_nnue_on) nnue_make(mover, m, moving_pt, u.captured);
}
inline void leave_move(Position& pos, Move m, const Undo& u) {
    if (g_nnue_on) nnue_unmake();
    undo_move(pos, m, u);
}

// Mate scores are stored relative to the mating distance so a TT entry reused at a different
// ply still reports the correct "mate in N".
inline int score_to_tt(int s, int ply)   { return s >=  MATE_THRESHOLD ? s + ply
                                                 : s <= -MATE_THRESHOLD ? s - ply : s; }
inline int score_from_tt(int s, int ply) { return s >=  MATE_THRESHOLD ? s - ply
                                                 : s <= -MATE_THRESHOLD ? s + ply : s; }

int move_score(const Position& pos, Move m, Move tt_move, int ply) {
    if (m == tt_move) return 1 << 30;
    if (is_capture(pos, m)) {
        PieceType victim = (move_flag(m) == MF_ENPASSANT) ? PAWN : pos.piece_type_on(move_to(m));
        PieceType attacker = pos.piece_type_on(move_from(m));
        return 1000000 + PieceValue[victim] * 10 - PieceValue[attacker];
    }
    if (move_flag(m) == MF_PROMO) return 900000 + PieceValue[move_promo(m)];
    if (ply < MAX_PLY) {
        if (m == g_killers[ply][0]) return 800000;
        if (m == g_killers[ply][1]) return 700000;
    }
    return g_history[pos.side_to_move][move_from(m)][move_to(m)];
}

void order_moves(const Position& pos, MoveList& list, Move tt_move, int ply) {
    std::pair<int, Move> scored[256];
    for (int i = 0; i < list.size; ++i)
        scored[i] = { move_score(pos, list.moves[i], tt_move, ply), list.moves[i] };
    std::sort(scored, scored + list.size,
              [](const auto& a, const auto& b) { return a.first > b.first; });
    for (int i = 0; i < list.size; ++i) list.moves[i] = scored[i].second;
}

void record_killer(int ply, Move m) {
    if (ply >= MAX_PLY) return;
    if (g_killers[ply][0] != m) { g_killers[ply][1] = g_killers[ply][0]; g_killers[ply][0] = m; }
}

// Search only "loud" moves (captures/promotions) until the position is quiet, so we never
// evaluate in the middle of a trade. Cures the horizon effect.
int quiescence(Position& pos, int alpha, int beta, int ply) {
    ++g_nodes;
    if (g_node_limit && (long long)g_nodes >= g_node_limit) { g_stop = true; return 0; }
    if ((g_nodes & 2047) == 0 && time_up()) { g_stop = true; return 0; }

    bool check = in_check(pos);
    MoveList list;

    if (check) {
        generate_legal(pos, list);
        if (list.size == 0) return -MATE + ply;   // checkmate
    } else {
        int stand = eval_stm(pos);
        if (stand >= beta) return beta;
        if (stand > alpha) alpha = stand;
        generate_legal(pos, list);
    }

    order_moves(pos, list, MOVE_NONE, ply);
    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        if (!check && !is_capture(pos, m) && move_flag(m) != MF_PROMO) continue;  // quiet: skip
        // Prune losing captures: if the exchange loses material, searching it in quiescence is
        // usually a waste (the opponent wouldn't let us profit). Skip when not in check / not a promo.
        if (!check && move_flag(m) != MF_PROMO && is_capture(pos, m) && see(pos, m) < 0) continue;
        Undo u; enter_move(pos, m, u);
        int score = -quiescence(pos, -beta, -alpha, ply + 1);
        leave_move(pos, m, u);
        if (g_stop) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int negamax(Position& pos, int depth, int alpha, int beta, int ply) {
    ++g_nodes;
    if (g_node_limit && (long long)g_nodes >= g_node_limit) { g_stop = true; return 0; }
    if ((g_nodes & 2047) == 0 && time_up()) { g_stop = true; return 0; }

    if (ply > 0 && (pos.halfmove_clock >= 100 || insufficient_material(pos)))
        return 0;

    bool check = in_check(pos);
    std::uint64_t key = compute_key(pos);

    // Repetition draw: if this exact position already occurred earlier on the search line,
    // the line is a draw. Repetitions can only be with positions since the last irreversible
    // move (pawn move / capture), i.e. within the halfmove clock, and only every 2 plies (same
    // side to move). This is what stops the engine from "winning" or escaping loss by shuffling.
    if (ply > 0 && ply < MAX_PLY) {
        g_path_key[ply] = key;
        int stop = ply - pos.halfmove_clock;
        if (stop < 0) stop = 0;
        for (int p = ply - 2; p >= stop; p -= 2)
            if (g_path_key[p] == key) return 0;
    }

    // Check extension: search one ply deeper when in check. Forcing lines shouldn't be cut
    // short, and we never want to statically evaluate a position where our king is attacked.
    if (check) ++depth;

    TTEntry& e = TT[key & TT_MASK];
    Move tt_move = MOVE_NONE;
    if (e.key == key) {
        tt_move = e.move;
        if (e.depth >= depth) {
            int s = score_from_tt(e.score, ply);
            if (e.flag == B_EXACT) return s;
            if (e.flag == B_LOWER && s >= beta) return s;
            if (e.flag == B_UPPER && s <= alpha) return s;
        }
    }

    if (depth <= 0) return quiescence(pos, alpha, beta, ply);

    int static_eval = check ? 0 : eval_stm(pos);   // used by the pruning heuristics below

    // Reverse futility pruning: at shallow depth, if our static eval is comfortably above beta,
    // trust it and prune — the opponent won't walk into a line this good for us.
    if (!check && depth <= 6 && static_eval - 80 * depth >= beta
        && beta < MATE_THRESHOLD && beta > -MATE_THRESHOLD)
        return static_eval;

    // Null-move pruning: give the opponent a free move; if we're still >= beta, prune.
    if (!check && depth >= 3 && beta < MATE_THRESHOLD && has_non_pawn(pos)) {
        Square saved_ep = pos.ep_square;
        pos.ep_square = SQ_NONE;
        pos.side_to_move = ~pos.side_to_move;
        int score = -negamax(pos, depth - 3, -beta, -beta + 1, ply + 1);
        pos.side_to_move = ~pos.side_to_move;
        pos.ep_square = saved_ep;
        if (g_stop) return 0;
        if (score >= beta) return beta;
    }

    MoveList list;
    generate_legal(pos, list);
    if (list.size == 0) return check ? -MATE + ply : 0;   // checkmate or stalemate

    order_moves(pos, list, tt_move, ply);

    int alpha_orig = alpha;
    int best = -INF;
    Move best_move = MOVE_NONE;
    int moves_searched = 0;

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        bool quiet = !is_capture(pos, m) && move_flag(m) != MF_PROMO;

        // Forward-prune quiet moves once we already have a non-losing fallback (never in check).
        if (!check && quiet && moves_searched > 0 && best > -MATE_THRESHOLD) {
            // Late move pruning: at low depth, stop trying quiet moves after enough of them.
            if (depth <= 3 && moves_searched >= 4 + depth * depth) continue;
            // Futility pruning: a quiet move that can't lift a low static eval up to alpha.
            if (depth <= 4 && static_eval + 120 * depth <= alpha) continue;
        }

        Undo u; enter_move(pos, m, u);
        int score;
        if (moves_searched == 0) {
            score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);   // principal variation: full window
        } else {
            // Late Move Reduction for quiet, late moves, plus PVS null-window scout.
            int reduction = (depth >= 3 && moves_searched >= 4 && quiet && !check) ? 1 : 0;
            score = -negamax(pos, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && reduction)   // reduced search beat alpha — re-search at full depth
                score = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta) // scout says it's a new PV — re-search with full window
                score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
        }
        leave_move(pos, m, u);
        if (g_stop) return 0;
        ++moves_searched;

        if (score > best) { best = score; best_move = m; }
        if (best > alpha) alpha = best;
        if (alpha >= beta) {
            if (quiet) {
                record_killer(ply, m);
                g_history[pos.side_to_move][move_from(m)][move_to(m)] += depth * depth;
            }
            break;
        }
    }

    e.key = key;
    e.move = best_move;
    e.score = (std::int16_t)score_to_tt(best, ply);
    e.depth = (std::int8_t)depth;
    e.flag = best <= alpha_orig ? B_UPPER : best >= beta ? B_LOWER : B_EXACT;
    return best;
}

int search_root(Position& pos, int depth, int alpha, int beta, Move& best_move_out) {
    MoveList list;
    generate_legal(pos, list);
    std::uint64_t key = compute_key(pos);
    Move tt_move = (TT[key & TT_MASK].key == key) ? TT[key & TT_MASK].move : MOVE_NONE;
    order_moves(pos, list, tt_move, 0);

    int best = -INF;
    Move best_move = list.size ? list.moves[0] : MOVE_NONE;
    int moves_searched = 0;
    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        Undo u; enter_move(pos, m, u);
        int score;
        if (moves_searched == 0) {
            score = -negamax(pos, depth - 1, -beta, -alpha, 1);
        } else {
            score = -negamax(pos, depth - 1, -alpha - 1, -alpha, 1);          // PVS scout
            if (score > alpha && score < beta)
                score = -negamax(pos, depth - 1, -beta, -alpha, 1);           // re-search full window
        }
        leave_move(pos, m, u);
        if (g_stop) break;
        ++moves_searched;
        if (score > best) { best = score; best_move = m; }
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    best_move_out = best_move;

    TTEntry& e = TT[key & TT_MASK];
    e.key = key; e.move = best_move; e.score = (std::int16_t)best;
    e.depth = (std::int8_t)depth; e.flag = B_EXACT;
    return best;
}

}  // namespace

void init_search() {
    TT.assign(TT_SIZE, TTEntry{});
}

SearchResult search(Position& pos, const SearchLimits& limits) {
    g_nodes = 0;
    g_stop = false;
    g_node_limit = limits.max_nodes;
    g_nnue_on = nnue_enabled() && nnue_loaded();
    if (g_nnue_on) nnue_refresh(pos);   // build the root accumulator; make/unmake keep it current
    g_path_key[0] = compute_key(pos);   // root position, for repetition detection within search
    g_use_deadline = limits.time_limit > 0;
    g_deadline = std::chrono::steady_clock::now() +
                 std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                     std::chrono::duration<double>(limits.time_limit));
    std::memset(g_killers, 0, sizeof g_killers);
    std::memset(g_history, 0, sizeof g_history);

    SearchResult res;
    MoveList list; generate_legal(pos, list);
    if (list.size) res.best = list.moves[0];

    int score = 0;
    for (int depth = 1; depth <= limits.max_depth; ++depth) {
        // Aspiration windows: from depth 4 on, search a narrow window around the previous score
        // (most iterations barely change), widening only if we fall outside it. Faster deep search.
        int alpha = -INF, beta = INF, window = 30;
        if (depth >= 4) { alpha = score - window; beta = score + window; }
        Move bm = res.best;
        while (true) {
            int s = search_root(pos, depth, alpha, beta, bm);
            if (g_stop) break;
            if (s <= alpha) { alpha = (alpha - window > -INF) ? alpha - window : -INF; window *= 2; }
            else if (s >= beta) { beta = (beta + window < INF) ? beta + window : INF; window *= 2; }
            else { score = s; break; }      // inside the window — accepted
        }
        if (g_stop) break;                  // discard an unfinished depth
        res.best = bm;
        res.score = score;
        res.depth = depth;
        res.nodes = g_nodes;
        if (std::abs(score) >= MATE_THRESHOLD) break;   // forced mate found
    }
    res.nodes = g_nodes;
    return res;
}
