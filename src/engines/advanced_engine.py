"""Phase 2 engine ("Bishop", ~2000 Elo target).

Same evaluation as Phase 1 — all the new strength comes from a much smarter SEARCH.
Five classic techniques, each with a one-line reason:

  * Transposition table (TT): the same position is reached by many move orders
    ("transpositions"). We cache results keyed by a Zobrist hash so we never re-solve
    a position we've already understood, and we reuse the stored best move to order.
  * Quiescence search: never stop counting material in the MIDDLE of a trade. At the
    leaves we keep searching captures until the position is "quiet", curing the
    horizon effect that made Phase 1 miscount exchanges.
  * Better move ordering (TT move -> captures MVV-LVA -> killer moves -> history):
    alpha-beta prunes far more when good moves come first, so this is a pure speedup.
  * Null-move pruning: let the opponent move twice; if we're STILL winning easily,
    this line is so good we can prune it early.
  * Late Move Reductions (LMR): search unlikely late/quiet moves shallower first, and
    only re-search at full depth if one surprises us. Spends depth where it matters.

The search takes its evaluation function as a dependency, so Phase 3 can drop in the
NNUE without touching any of this code.
"""
from __future__ import annotations

import time

import chess
import chess.polyglot

from ..evaluation import PIECE_VALUES, evaluate_stm
from .base import Engine

MATE_SCORE = 1_000_000
# Scores at or beyond this magnitude represent a forced mate (not just material).
MATE_THRESHOLD = MATE_SCORE - 1000
INF = 2_000_000
MAX_PLY = 256

# Transposition-table entry flags: was the stored score exact, or just a bound?
EXACT, LOWER, UPPER = 0, 1, 2


class _TimeUp(Exception):
    """Bail out of the recursion the instant our time budget is spent."""


class AdvancedEngine(Engine):
    name = "Bishop"  # ladder rung ~2000 Elo (provisional name)

    def __init__(self, time_limit: float | None = 1.0, max_depth: int = 64,
                 evaluate=evaluate_stm) -> None:
        self.time_limit = time_limit
        self.max_depth = max_depth
        self._evaluate = evaluate  # injectable: handcrafted now, NNUE in Phase 3
        self.nodes = 0
        self.last_score = 0    # score (centipawns, side-to-move) of the last completed search
        self.last_depth = 0    # deepest fully-completed iteration on the last move
        self._deadline = float("inf")
        # Search memory (reset each move for simplicity/correctness):
        self.tt: dict[int, tuple[int, int, int, chess.Move | None]] = {}
        self.killers: list[list[chess.Move | None]] = []
        self.history: dict[tuple[bool, int, int], int] = {}

    # ------------------------------------------------------------------ public API
    def choose_move(self, board: chess.Board) -> chess.Move:
        self.nodes = 0
        self._deadline = (
            time.time() + self.time_limit if self.time_limit is not None else float("inf")
        )
        self.tt.clear()
        self.killers = [[None, None] for _ in range(MAX_PLY)]
        self.history.clear()

        best_move = next(iter(board.legal_moves))
        for depth in range(1, self.max_depth + 1):
            try:
                move, score = self._search_root(board, depth)
            except _TimeUp:
                break
            if move is not None:
                best_move = move
                self.last_score = score
                self.last_depth = depth
            if abs(score) >= MATE_THRESHOLD:  # forced mate found; going deeper is moot
                break
        return best_move

    # ------------------------------------------------------------------ root
    def _search_root(self, board: chess.Board, depth: int) -> tuple[chess.Move | None, int]:
        alpha, beta = -INF, INF
        best_move, best_score = None, -INF
        tt_move = self._tt_move(board)
        for move in self._order(board, list(board.legal_moves), tt_move, ply=0):
            board.push(move)
            try:
                score = -self._negamax(board, depth - 1, -beta, -alpha, ply=1)
            finally:
                board.pop()
            if score > best_score:
                best_score, best_move = score, move
            alpha = max(alpha, score)
        # Seed the TT with the root's best move so the next (deeper) iteration orders it first.
        self.tt[chess.polyglot.zobrist_hash(board)] = (depth, best_score, EXACT, best_move)
        return best_move, best_score

    # ------------------------------------------------------------------ main search
    def _negamax(self, board: chess.Board, depth: int, alpha: int, beta: int, ply: int) -> int:
        self.nodes += 1
        if self.nodes % 2048 == 0 and time.time() > self._deadline:
            raise _TimeUp

        # Draw detection (only below the root, so we never "draw away" a winning root move).
        if ply > 0 and (board.is_repetition(2)
                        or board.is_insufficient_material()
                        or board.halfmove_clock >= 100):
            return 0

        in_check = board.is_check()
        key = chess.polyglot.zobrist_hash(board)
        alpha_orig = alpha

        # --- Transposition table probe -------------------------------------------
        entry = self.tt.get(key)
        tt_move = None
        if entry is not None:
            e_depth, e_score, e_flag, e_move = entry
            tt_move = e_move
            if e_depth >= depth:
                s = self._score_from_tt(e_score, ply)
                if e_flag == EXACT:
                    return s
                if e_flag == LOWER:
                    alpha = max(alpha, s)
                elif e_flag == UPPER:
                    beta = min(beta, s)
                if alpha >= beta:
                    return s

        if depth <= 0:
            return self._quiescence(board, alpha, beta, ply)

        # --- Null-move pruning ---------------------------------------------------
        # "Pass" the turn to the opponent; if our position is so strong that even
        # giving them a free move leaves us above beta, we can prune. Disabled when
        # in check or with only pawns left (where passing would be a lie — zugzwang).
        if (not in_check and depth >= 3 and beta < MATE_THRESHOLD
                and self._has_non_pawn(board)):
            board.push(chess.Move.null())
            try:
                score = -self._negamax(board, depth - 3, -beta, -beta + 1, ply + 1)
            finally:
                board.pop()
            if score >= beta:
                return beta

        moves = list(board.legal_moves)
        if not moves:
            # No legal move: checkmate (if in check) or stalemate (a draw).
            return -MATE_SCORE + ply if in_check else 0

        ordered = self._order(board, moves, tt_move, ply)
        best_score, best_move = -INF, None

        for i, move in enumerate(ordered):
            gives_check = board.gives_check(move)
            is_quiet = not board.is_capture(move) and move.promotion is None and not gives_check

            board.push(move)
            try:
                if depth >= 3 and i >= 4 and is_quiet and not in_check:
                    # LMR: try this late-and-quiet move at reduced depth first.
                    score = -self._negamax(board, depth - 2, -alpha - 1, -alpha, ply + 1)
                    if score > alpha:  # it surprised us — verify at full depth.
                        score = -self._negamax(board, depth - 1, -beta, -alpha, ply + 1)
                else:
                    score = -self._negamax(board, depth - 1, -beta, -alpha, ply + 1)
            finally:
                board.pop()

            if score > best_score:
                best_score, best_move = score, move
            if best_score > alpha:
                alpha = best_score
            if alpha >= beta:
                # Cutoff. Reward this quiet move so we try it earlier elsewhere.
                if is_quiet:
                    self._record_killer(ply, move)
                    self.history[(board.turn, move.from_square, move.to_square)] = \
                        self.history.get((board.turn, move.from_square, move.to_square), 0) + depth * depth
                break

        # --- Store result in the TT (with the right bound flag) -------------------
        if best_score <= alpha_orig:
            flag = UPPER            # failed low: real value is at most best_score
        elif best_score >= beta:
            flag = LOWER            # failed high: real value is at least best_score
        else:
            flag = EXACT
        self.tt[key] = (depth, self._score_to_tt(best_score, ply), flag, best_move)
        return best_score

    # ------------------------------------------------------------------ quiescence
    def _quiescence(self, board: chess.Board, alpha: int, beta: int, ply: int) -> int:
        self.nodes += 1
        if self.nodes % 2048 == 0 and time.time() > self._deadline:
            raise _TimeUp

        if board.is_check():
            # In check we must consider ALL escapes, not just captures (could be mate).
            moves = list(board.legal_moves)
            if not moves:
                return -MATE_SCORE + ply
        else:
            # "Stand pat": we may simply choose not to capture. If just standing here
            # already beats beta, the opponent won't allow this line — prune.
            stand = self._evaluate(board)
            if stand >= beta:
                return beta
            if stand > alpha:
                alpha = stand
            moves = [m for m in board.legal_moves if board.is_capture(m) or m.promotion]

        moves.sort(key=lambda m: self._capture_score(board, m), reverse=True)
        for move in moves:
            board.push(move)
            try:
                score = -self._quiescence(board, -beta, -alpha, ply + 1)
            finally:
                board.pop()
            if score >= beta:
                return beta
            if score > alpha:
                alpha = score
        return alpha

    # ------------------------------------------------------------------ helpers
    def _tt_move(self, board: chess.Board) -> chess.Move | None:
        entry = self.tt.get(chess.polyglot.zobrist_hash(board))
        return entry[3] if entry else None

    def _capture_score(self, board: chess.Board, move: chess.Move) -> int:
        # MVV-LVA: prefer taking a big piece with a small one.
        victim = board.piece_type_at(move.to_square) or chess.PAWN  # None => en passant pawn
        attacker = board.piece_type_at(move.from_square)
        return PIECE_VALUES[victim] * 10 - PIECE_VALUES[attacker]

    def _order(self, board: chess.Board, moves: list[chess.Move],
               tt_move: chess.Move | None, ply: int) -> list[chess.Move]:
        killer0, killer1 = self.killers[ply] if ply < len(self.killers) else (None, None)

        def key(move: chess.Move) -> int:
            if move == tt_move:
                return 10_000_000
            if board.is_capture(move):
                return 1_000_000 + self._capture_score(board, move)
            if move.promotion:
                return 900_000 + PIECE_VALUES[move.promotion]
            if move == killer0:
                return 800_000
            if move == killer1:
                return 700_000
            return self.history.get((board.turn, move.from_square, move.to_square), 0)

        return sorted(moves, key=key, reverse=True)

    def _record_killer(self, ply: int, move: chess.Move) -> None:
        if self.killers[ply][0] != move:
            self.killers[ply][1] = self.killers[ply][0]
            self.killers[ply][0] = move

    def _has_non_pawn(self, board: chess.Board) -> bool:
        c = board.turn
        return bool(board.pieces_mask(chess.KNIGHT, c) | board.pieces_mask(chess.BISHOP, c)
                    | board.pieces_mask(chess.ROOK, c) | board.pieces_mask(chess.QUEEN, c))

    # Mate scores must be stored relative to the mating distance, or the same TT entry
    # reused at a different depth would report the wrong "mate in N".
    def _score_to_tt(self, score: int, ply: int) -> int:
        if score >= MATE_THRESHOLD:
            return score + ply
        if score <= -MATE_THRESHOLD:
            return score - ply
        return score

    def _score_from_tt(self, score: int, ply: int) -> int:
        if score >= MATE_THRESHOLD:
            return score - ply
        if score <= -MATE_THRESHOLD:
            return score + ply
        return score
