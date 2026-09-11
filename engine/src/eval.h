// Handcrafted evaluation (material + piece-square tables), ported from the Python "Bishop"
// engine. This is the baseline the NNUE (Phase 3) must beat. Scores are in centipawns.
#pragma once
#include "position.h"

extern const int PieceValue[PIECE_TYPE_NB];   // material worth of each piece type

int evaluate(const Position& pos);       // + = good for White
int evaluate_stm(const Position& pos);   // + = good for the side to move
