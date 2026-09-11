"""Phase 1 engine: minimax + alpha-beta pruning + iterative deepening.

The idea in one breath: to pick a move, imagine playing it, then imagine the
opponent's best reply, then our best reply to that... a few moves deep. Score the
resulting positions with our handcrafted evaluation and back the scores up the tree,
assuming both sides always play their best. Return the move that leads to the best
guaranteed outcome for us.

Three techniques stacked together:
  * Negamax — a tidy way to write minimax where we always maximise from the mover's
    perspective and just flip the sign at each level (because your gain is my loss).
  * Alpha-beta pruning — skip branches that can't possibly change the decision. Same
    answer as brute force, but explores far fewer positions, so we can look deeper.
  * Iterative deepening — search depth 1, then 2, then 3... until the clock runs out,
    always keeping the best move from the last fully finished depth. This is how the
    engine respects a time budget without ever returning a half-baked answer.
"""
from __future__ import annotations

import time

import chess

from ..evaluation import PIECE_VALUES, evaluate_stm
from .base import Engine

# A "checkmate" score, far larger than any material swing so mate always dominates.
# We subtract the distance-to-mate so the engine prefers mating sooner (and, when
# being mated, delaying it as long as possible).
MATE_SCORE = 1_000_000
INF = 2_000_000


class _TimeUp(Exception):
    """Raised deep in the recursion to bail out the instant our time is spent."""


class AlphaBetaEngine(Engine):
    name = "Knight"  # ladder rung ~1500 Elo (provisional name); FROZEN — do not add Phase 2+ features here

    def __init__(self, time_limit: float | None = 1.0, max_depth: int = 64) -> None:
        # time_limit: seconds allowed per move. Set to None to search a fixed depth
        # (used by tests for deterministic, repeatable results).
        self.time_limit = time_limit
        self.max_depth = max_depth
        self.nodes = 0
        self._deadline = float("inf")

    # ------------------------------------------------------------------ public API
    def choose_move(self, board: chess.Board) -> chess.Move:
        self.nodes = 0
        self._deadline = (
            time.time() + self.time_limit if self.time_limit is not None else float("inf")
        )

        # Always hold a legal fallback so we never return None, even on a tiny budget.
        best_move = next(iter(board.legal_moves))

        # Iterative deepening: each pass reuses nothing yet (that comes in Phase 2 with
        # a transposition table), but it gives us time control and a good move ordering
        # hint for free — the previous best move tends to still be best.
        for depth in range(1, self.max_depth + 1):
            try:
                move, score = self._search_root(board, depth)
            except _TimeUp:
                break  # discard this unfinished depth; keep the last complete one
            if move is not None:
                best_move = move
            # If we've found a forced mate, no deeper search will change our mind.
            if abs(score) >= MATE_SCORE - 100:
                break
        return best_move

    # ------------------------------------------------------------------ search core
    def _search_root(self, board: chess.Board, depth: int) -> tuple[chess.Move | None, int]:
        best_move: chess.Move | None = None
        alpha, beta = -INF, INF
        best_score = -INF
        for move in self._ordered_moves(board):
            board.push(move)
            try:
                score = -self._negamax(board, depth - 1, -beta, -alpha, ply=1)
            finally:
                # Always undo, even if a time-up exception fires mid-search, so the
                # real board is never left corrupted.
                board.pop()
            if score > best_score:
                best_score, best_move = score, move
            alpha = max(alpha, score)
        return best_move, best_score

    def _negamax(self, board: chess.Board, depth: int, alpha: int, beta: int, ply: int) -> int:
        self.nodes += 1
        # Check the clock occasionally (not every node — that syscall isn't free).
        if self.nodes % 2048 == 0 and time.time() > self._deadline:
            raise _TimeUp

        # Cheap draw: neither side has enough material to mate. Score it dead equal.
        if board.is_insufficient_material():
            return 0
        # A position that repeats is a draw; treating the first repetition as 0 stops
        # the engine from "winning" by shuffling pieces forever.
        if ply > 0 and board.is_repetition(2):
            return 0

        if depth == 0:
            return evaluate_stm(board)

        moves = list(board.legal_moves)
        if not moves:
            # No legal move means the game is over right here:
            #   in check  -> checkmate (worst possible, adjusted so nearer mates win)
            #   not check -> stalemate (a draw)
            return -MATE_SCORE + ply if board.is_check() else 0

        best = -INF
        for move in self._order(board, moves):
            board.push(move)
            try:
                score = -self._negamax(board, depth - 1, -beta, -alpha, ply + 1)
            finally:
                board.pop()  # match every push, even on a time-up unwind
            if score > best:
                best = score
            if best > alpha:
                alpha = best
            if alpha >= beta:
                # Beta cutoff: the opponent already has a reply good enough to avoid
                # this whole branch, so there's no point examining the rest of it.
                break
        return best

    # -------------------------------------------------------------- move ordering
    def _ordered_moves(self, board: chess.Board):
        return self._order(board, list(board.legal_moves))

    def _order(self, board: chess.Board, moves: list[chess.Move]) -> list[chess.Move]:
        """Try likely-good moves first so alpha-beta prunes more.

        Full move ordering (killers, history, TT move) arrives in Phase 2. For now a
        simple rule does most of the work: look at captures first, and among captures
        prefer grabbing a big piece with a small one (MVV-LVA — Most Valuable Victim,
        Least Valuable Attacker).
        """
        def key(move: chess.Move) -> int:
            score = 0
            if board.is_capture(move):
                victim = board.piece_type_at(move.to_square)
                victim_val = PIECE_VALUES[victim] if victim else PIECE_VALUES[chess.PAWN]
                attacker_val = PIECE_VALUES[board.piece_type_at(move.from_square)]
                score += 10_000 + victim_val - attacker_val // 10
            if move.promotion:
                score += PIECE_VALUES.get(move.promotion, 0)
            return score

        return sorted(moves, key=key, reverse=True)
