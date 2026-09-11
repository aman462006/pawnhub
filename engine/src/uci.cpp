#include "uci.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "move.h"
#include "nnue.h"
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

namespace {

const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Convert a UCI move string ("e2e4", "e7e8q", "e1g1") into our internal Move by matching it
// against the legal moves. This automatically gets the special-move flags right (a king
// e1->g1 becomes a castle, e5->d6 becomes en passant, etc.) because the generated move
// already carries them.
Move parse_uci_move(Position& pos, const std::string& s) {
    MoveList list;
    generate_legal(pos, list);
    for (int i = 0; i < list.size; ++i)
        if (move_to_uci(list.moves[i]) == s) return list.moves[i];
    return MOVE_NONE;
}

void handle_position(Position& pos, std::istringstream& iss) {
    std::string token;
    iss >> token;
    if (token == "startpos") {
        pos.set_fen(START_FEN);
        iss >> token;                       // maybe "moves"
    } else if (token == "fen") {
        std::string fen, part;
        for (int i = 0; i < 6 && iss >> part; ++i) fen += part + " ";
        pos.set_fen(fen);
        iss >> token;                       // maybe "moves"
    }
    if (token == "moves") {
        std::string mv;
        while (iss >> mv) {
            Move m = parse_uci_move(pos, mv);
            if (m == MOVE_NONE) break;      // malformed / illegal: stop applying
            Undo u; do_move(pos, m, u);     // apply permanently (undo discarded)
        }
    }
}

void handle_go(Position& pos, std::istringstream& iss) {
    double movetime = -1, wtime = -1, btime = -1, winc = 0, binc = 0;
    int depth = -1;
    std::string token;
    while (iss >> token) {
        if      (token == "movetime") { long x; iss >> x; movetime = x / 1000.0; }
        else if (token == "depth")    { iss >> depth; }
        else if (token == "wtime")    { long x; iss >> x; wtime = x / 1000.0; }
        else if (token == "btime")    { long x; iss >> x; btime = x / 1000.0; }
        else if (token == "winc")     { long x; iss >> x; winc = x / 1000.0; }
        else if (token == "binc")     { long x; iss >> x; binc = x / 1000.0; }
    }

    SearchLimits lim;
    if (depth > 0) {                        // fixed-depth search
        lim.max_depth = depth;
        lim.time_limit = 0;
    } else if (movetime > 0) {              // exact time per move
        lim.time_limit = movetime;
    } else if (wtime > 0 || btime > 0) {    // clock-based: spend a slice of remaining time
        double rem = (pos.side_to_move == WHITE) ? wtime : btime;
        double inc = (pos.side_to_move == WHITE) ? winc : binc;
        double budget = rem / 30.0 + inc * 0.8;
        // Safety cap: never spend more than ~40% of the remaining clock on one move (and keep a
        // small emergency reserve), so we can't flag in low-time scrambles.
        if (budget > rem * 0.4) budget = rem * 0.4;
        lim.time_limit = std::max(0.02, budget);
    } else {
        lim.time_limit = 1.0;               // sensible default
    }

    SearchResult r = search(pos, lim);
    std::cout << "info depth " << r.depth << " nodes " << r.nodes
              << " score cp " << r.score << "\n"
              << "bestmove " << move_to_uci(r.best) << "\n" << std::flush;
}

}  // namespace

void uci_loop() {
    Position pos;
    pos.set_fen(START_FEN);
    std::string line;
    while (std::getline(std::cin, line)) {
        // Robustness: strip a UTF-8 BOM (some shells prepend one) and any trailing CR
        // from Windows-style line endings, so command matching isn't thrown off.
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF
            && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            line.erase(0, 3);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "uci") {
            std::cout << "id name CppChess 0.1\n"
                      << "id author chess-engine-project\n"
                      << "option name UseNNUE type check default true\n"
                      << "uciok\n" << std::flush;
        } else if (cmd == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (cmd == "setoption") {
            // Format: setoption name <Name> value <Value>
            std::string tok, name, value;
            iss >> tok >> name >> tok >> value;   // "name", <Name>, "value", <Value>
            if (name == "UseNNUE")
                nnue_set_enabled(value == "true" || value == "1");
        } else if (cmd == "ucinewgame") {
            pos.set_fen(START_FEN);
        } else if (cmd == "position") {
            handle_position(pos, iss);
        } else if (cmd == "go") {
            handle_go(pos, iss);
        } else if (cmd == "quit") {
            break;
        }
        // Unknown commands are silently ignored, as the UCI spec recommends.
    }
}
