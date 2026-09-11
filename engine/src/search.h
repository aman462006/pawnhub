// The search: given a position, find the best move by looking ahead. This is the C++ port of
// the Python "Bishop" engine (negamax + alpha-beta + transposition table + quiescence +
// move ordering + null-move + LMR + iterative deepening) — now running ~1000x faster.
#pragma once
#include <cstdint>
#include "position.h"
#include "move.h"

struct SearchLimits {
    int           max_depth  = 64;
    double        time_limit = 1.0;   // seconds per move; <= 0 means "no time limit"
    long long     max_nodes  = 0;     // node budget per move; 0 = unlimited (used by datagen)
};

struct SearchResult {
    Move          best  = MOVE_NONE;
    int           score = 0;      // centipawns, from side-to-move's view
    int           depth = 0;      // deepest completed iteration
    std::uint64_t nodes = 0;
};

void init_search();   // allocate the transposition table (call once at startup)
SearchResult search(Position& pos, const SearchLimits& limits);
