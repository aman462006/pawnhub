"""Head-to-head: the new C++ engine (driven over UCI) vs the Python 'Bishop' oracle.

This is the proof that closes Phase 2.5. Both get the same time per move; if the C++ engine's
depth advantage is real, it should dominate. python-chess launches the C++ exe as a UCI
subprocess and relays moves — exactly the hybrid architecture we designed.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import chess
import chess.engine

from src.engines.advanced_engine import AdvancedEngine

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE_PATH = os.path.join(ROOT, "engine", "build", "engine.exe")


def play_game(cpp: chess.engine.SimpleEngine, cpp_is_white: bool, movetime: float) -> str:
    board = chess.Board()
    bishop = AdvancedEngine(time_limit=movetime)
    while not board.is_game_over(claim_draw=True):
        cpp_to_move = (board.turn == chess.WHITE) == cpp_is_white
        if cpp_to_move:
            result = cpp.play(board, chess.engine.Limit(time=movetime))
            board.push(result.move)
        else:
            board.push(bishop.choose_move(board))
    return board.result(claim_draw=True)


def main() -> None:
    movetime = 0.1
    games = 6
    cpp = chess.engine.SimpleEngine.popen_uci(ENGINE_PATH)
    wins = losses = draws = 0
    try:
        for g in range(games):
            cpp_is_white = (g % 2 == 0)
            result = play_game(cpp, cpp_is_white, movetime)
            if result == "1/2-1/2":
                draws += 1
                outcome = "draw"
            else:
                cpp_won = (result == "1-0") == cpp_is_white
                if cpp_won:
                    wins += 1; outcome = "C++ win"
                else:
                    losses += 1; outcome = "Bishop win"
            print(f"  game {g+1}: C++ plays {'White' if cpp_is_white else 'Black'} -> {result} ({outcome})")
    finally:
        cpp.quit()

    print(f"\nC++ engine vs Bishop over {games} games @ {movetime}s/move:")
    print(f"  C++ wins:    {wins}")
    print(f"  Bishop wins: {losses}")
    print(f"  draws:       {draws}")


if __name__ == "__main__":
    main()
