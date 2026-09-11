"""Phase 0 smoke tests: prove the harness works end-to-end without a human.

Run with:  py -m pytest   (or)   py -m unittest discover tests
These are deliberately minimal — they just assert "nothing is on fire."
"""
import unittest

import chess

from src.arena import play_game, play_match
from src.engines.random_engine import RandomEngine
from src.gameio import parse_human_move


class SmokeTests(unittest.TestCase):
    def test_random_engine_returns_legal_move(self):
        board = chess.Board()
        move = RandomEngine(seed=0).choose_move(board)
        self.assertIn(move, board.legal_moves)

    def test_full_game_terminates(self):
        board = play_game(RandomEngine(seed=1), RandomEngine(seed=2))
        self.assertTrue(board.is_game_over() or board.ply() >= 400)

    def test_match_tally_adds_up(self):
        res = play_match(RandomEngine(seed=1), RandomEngine(seed=2), games=6)
        self.assertEqual(res.wins_white + res.wins_black + res.draws, 6)

    def test_move_parser_accepts_san_and_uci(self):
        board = chess.Board()
        self.assertIsNotNone(parse_human_move(board, "e4"))     # SAN
        self.assertIsNotNone(parse_human_move(board, "g1f3"))   # UCI
        self.assertIsNone(parse_human_move(board, "e5"))        # illegal from start


if __name__ == "__main__":
    unittest.main()
