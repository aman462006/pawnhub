"""Handcrafted board evaluation (Phase 1).

Turns a position into a single number in *centipawns* (100 = one pawn), from
White's point of view. Positive = good for White, negative = good for Black.

Two ingredients:
  1. Material — how much wood each side has (a queen is worth ~9 pawns, etc.).
  2. Piece-Square Tables (PST) — a bonus/penalty for WHERE each piece sits.
     A knight in the centre is worth more than one in the corner; pawns are
     rewarded for advancing; the king is nudged to stay tucked away.

These tables are the classic "Simplified Evaluation Function" values (Michniewski)
— a well-known educational starting point. In Phase 3 this whole file gets replaced
by a trained neural network (NNUE); this handcrafted version is the honest baseline
we must beat.
"""
from __future__ import annotations

import chess

# Material value of each piece type, in centipawns. King is 0 for material because
# both sides always have exactly one — counting it would just cancel out, and we
# never want it tangled up with checkmate scoring.
PIECE_VALUES: dict[int, int] = {
    chess.PAWN: 100,
    chess.KNIGHT: 320,
    chess.BISHOP: 330,
    chess.ROOK: 500,
    chess.QUEEN: 900,
    chess.KING: 0,
}

# Each table has 64 entries written the way you'd READ a board: the first row is
# rank 8 (Black's back rank), the last row is rank 1 (White's back rank), files a->h.
# We translate to python-chess square indices below.
_PAWN = (
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0,
)
_KNIGHT = (
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
)
_BISHOP = (
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
)
_ROOK = (
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0,
)
_QUEEN = (
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
)
# King table for the middlegame: hide in a corner behind pawns, stay off the centre.
_KING = (
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
)

_PST: dict[int, tuple[int, ...]] = {
    chess.PAWN: _PAWN,
    chess.KNIGHT: _KNIGHT,
    chess.BISHOP: _BISHOP,
    chess.ROOK: _ROOK,
    chess.QUEEN: _QUEEN,
    chess.KING: _KING,
}


def evaluate(board: chess.Board) -> int:
    """Static evaluation of `board` in centipawns, from White's perspective.

    'Static' means we just look at the position as-is — no searching ahead.
    """
    score = 0
    for square, piece in board.piece_map().items():
        value = PIECE_VALUES[piece.piece_type]
        table = _PST[piece.piece_type]
        if piece.color == chess.WHITE:
            # Our tables are written from White's reading view (rank 8 first), which
            # lines up with python-chess squares after a vertical flip (square_mirror).
            score += value + table[chess.square_mirror(square)]
        else:
            # Black is the mirror image: use the square directly and subtract, since
            # what's good for Black is bad for White.
            score -= value + table[square]
    return score


def evaluate_stm(board: chess.Board) -> int:
    """Evaluation from the side-to-move's perspective (positive = current player is better).

    Search works most cleanly when every node is scored 'from the mover's eyes', so
    we flip the sign when it's Black to move.
    """
    score = evaluate(board)
    return score if board.turn == chess.WHITE else -score
