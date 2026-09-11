"""The dumbest possible engine: pick a legal move at random.

This is our baseline and our sanity check. If a future "smart" engine can't beat
this reliably, something is broken. It also proves the harness works end-to-end.
"""
from __future__ import annotations

import random

import chess

from .base import Engine


class RandomEngine(Engine):
    name = "Pawn"  # ladder rung ~0 Elo (provisional name)

    def __init__(self, seed: int | None = None) -> None:
        # A private RNG (optionally seeded) so games are reproducible when we want.
        self._rng = random.Random(seed)

    def choose_move(self, board: chess.Board) -> chess.Move:
        # board.legal_moves already excludes illegal moves and moves that leave the
        # king in check — this is exactly why we lean on python-chess as the source
        # of truth instead of tracking the board ourselves.
        return self._rng.choice(list(board.legal_moves))
