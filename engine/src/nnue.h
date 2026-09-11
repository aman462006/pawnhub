// NNUE evaluation in C++: load the quantized net and score positions with integer math.
//
// This first version recomputes the accumulator from scratch each call (correctness first).
// The "efficiently updatable" incremental version (update only changed features per move)
// is a later optimization. The integer math here MUST match tools/export_net.py exactly.
#pragma once
#include "position.h"
#include "move.h"

bool nnue_load(const char* path = nullptr);  // load nets/nnue.bin; true on success
bool nnue_loaded();
void nnue_set_enabled(bool on);
bool nnue_enabled();

// Evaluation reads the CURRENT accumulator (top of the stack). Call nnue_refresh() first for a
// standalone position; the search keeps the accumulator current via nnue_make/nnue_unmake.
int nnue_eval_stm(const Position& pos);    // centipawns, side-to-move's perspective (reads accumulator)
int nnue_eval_recompute(const Position& pos);  // full recompute (stack-free); for tests/fallback

// Incremental accumulator management (the "efficiently updatable" core):
void nnue_refresh(const Position& pos);    // recompute both accumulators from scratch (stack base)
void nnue_make(Color mover, Move m, PieceType moving_pt, PieceType captured);  // push child accumulator
void nnue_unmake();                        // pop back to parent accumulator
