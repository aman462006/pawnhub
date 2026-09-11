"""The Engine interface.

Every engine we ever build (random, alpha-beta, NNUE...) is just a thing that,
given a legal position, returns one move to play. Keeping this contract tiny and
stable is what lets the rest of the project stay unchanged as the brain gets smarter.
"""
from __future__ import annotations

from abc import ABC, abstractmethod

import chess


class Engine(ABC):
    """Abstract base for all move-choosing engines."""

    #: Human-readable name, shown in the terminal and arena results.
    name: str = "Engine"

    @abstractmethod
    def choose_move(self, board: chess.Board) -> chess.Move:
        """Return one legal move for the side to move in `board`.

        The board is the authoritative game state (python-chess). Engines read it
        but must NOT mutate it — the harness owns pushing/popping moves.
        """
        raise NotImplementedError
