"""Phase 1 tests: prove the engine actually *thinks*, not just moves legally.

We use fixed-depth search (time_limit=None) so results are deterministic and fast.
Tactics are the cleanest way to test a search: there's one right answer and the
engine must find it.
"""
import unittest

import chess

from src.engines.search_engine import AlphaBetaEngine


class SearchTests(unittest.TestCase):
    def test_takes_a_free_queen(self):
        # White queen on d1 can capture Black's undefended queen on d4. A thinking
        # engine must grab it. (Kings kept far apart so nothing else is going on.)
        board = chess.Board("4k3/8/8/8/3q4/8/8/3QK3 w - - 0 1")
        move = AlphaBetaEngine(time_limit=None, max_depth=2).choose_move(board)
        self.assertEqual(move.uci(), "d1d4")

    def test_finds_mate_in_one(self):
        # Classic back-rank mate: Re8 is checkmate. The engine must see it.
        board = chess.Board("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1")
        move = AlphaBetaEngine(time_limit=None, max_depth=2).choose_move(board)
        self.assertEqual(move.uci(), "e1e8")

    def test_avoids_hanging_its_own_queen(self):
        # White to move. Moving the queen to d5 would hang it to the c6 pawn.
        # The engine should NOT walk into that once it looks a move ahead.
        board = chess.Board("4k3/8/2p5/8/8/8/8/3QK3 w - - 0 1")
        move = AlphaBetaEngine(time_limit=None, max_depth=3).choose_move(board)
        self.assertNotEqual(move.uci(), "d1d5")

    def test_returns_legal_move_on_tiny_time_budget(self):
        # Even with an absurdly small clock, iterative deepening must return a legal move.
        board = chess.Board()
        move = AlphaBetaEngine(time_limit=0.001).choose_move(board)
        self.assertIn(move, board.legal_moves)


if __name__ == "__main__":
    unittest.main()
