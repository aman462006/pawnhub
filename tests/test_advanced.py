"""Phase 2 tests for the Bishop engine.

Same tactical sanity as Phase 1, PLUS a mate-in-two to prove the deeper, quiescence-
backed search actually sees forced mate sequences. We check `last_score` (the search's
own verdict) so the test doesn't depend on which of several mating moves it picks.
"""
import unittest

import chess

from src.engines.advanced_engine import AdvancedEngine, MATE_THRESHOLD


class AdvancedTests(unittest.TestCase):
    def test_takes_a_free_queen(self):
        board = chess.Board("4k3/8/8/8/3q4/8/8/3QK3 w - - 0 1")
        move = AdvancedEngine(time_limit=None, max_depth=3).choose_move(board)
        self.assertEqual(move.uci(), "d1d4")

    def test_finds_mate_in_one(self):
        board = chess.Board("6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1")
        eng = AdvancedEngine(time_limit=None, max_depth=3)
        move = eng.choose_move(board)
        self.assertEqual(move.uci(), "e1e8")
        self.assertGreaterEqual(eng.last_score, MATE_THRESHOLD)

    def test_finds_forced_mate_in_two(self):
        # King+Queen vs King: 1.Qh7+ Kf8 2.Qf7# is forced. Needs a 3-ply search, which
        # Phase 1 at shallow depth would miss but Bishop should nail.
        board = chess.Board("6k1/8/6K1/8/8/8/8/7Q w - - 0 1")
        eng = AdvancedEngine(time_limit=None, max_depth=5)
        eng.choose_move(board)
        self.assertGreaterEqual(eng.last_score, MATE_THRESHOLD)

    def test_avoids_hanging_its_own_queen(self):
        board = chess.Board("4k3/8/2p5/8/8/8/8/3QK3 w - - 0 1")
        move = AdvancedEngine(time_limit=None, max_depth=3).choose_move(board)
        self.assertNotEqual(move.uci(), "d1d5")

    def test_returns_legal_move_on_tiny_time_budget(self):
        board = chess.Board()
        move = AdvancedEngine(time_limit=0.001).choose_move(board)
        self.assertIn(move, board.legal_moves)


if __name__ == "__main__":
    unittest.main()
