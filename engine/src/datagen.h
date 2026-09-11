// Self-play training-data generation.
//
// The engine plays games against itself at a fixed node budget, starting from randomised
// openings. Every quiet position is written out with TWO labels: the engine's search score,
// and the eventual game result (win/draw/loss). Training blends these — the score teaches
// fine judgement, the result teaches "what actually wins". This is the data the first, weak
// NNUE never had.
#pragma once
#include <string>
#include <cstdint>

// Writes CSV lines "fen,score_stm,result_white":
//   score_stm    = search score in centipawns from the side-to-move's view
//   result_white = game outcome for White (1.0 win, 0.5 draw, 0.0 loss)
void run_datagen(long long games, long long nodes, const std::string& out,
                 std::uint64_t seed, int opening_plies);
