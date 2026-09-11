"""The Phase 3 verdict: NNUE evaluation vs handcrafted evaluation.

Both players are the SAME C++ engine at the SAME time/move — the only difference is the
UseNNUE option. So any score difference is purely the neural net's judgment vs the hand-written
material+PST tables. If NNUE is better (as it should be, typically a few hundred Elo),
it should win the match.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import chess.engine

import common

# Set by main(): the per-move limit both engines use (fair comparison).
LIMIT = chess.engine.Limit(time=0.1)


def new_engine(use_nnue: bool) -> chess.engine.SimpleEngine:
    eng = chess.engine.SimpleEngine.popen_uci(common.ENGINE_PATH)
    eng.configure({"UseNNUE": use_nnue})
    return eng


def play_game(nnue_eng, hand_eng, nnue_white: bool) -> str:
    board = chess.Board()
    while not board.is_game_over(claim_draw=True):
        nnue_to_move = (board.turn == chess.WHITE) == nnue_white
        eng = nnue_eng if nnue_to_move else hand_eng
        board.push(eng.play(board, LIMIT).move)
    return board.result(claim_draw=True)


def main() -> None:
    global LIMIT
    ap = argparse.ArgumentParser()
    ap.add_argument("--movetime", type=float, default=0.1)
    ap.add_argument("--depth", type=int, default=0)   # if >0, compare at fixed depth (removes speed factor)
    ap.add_argument("--games", type=int, default=20)
    args = ap.parse_args()
    LIMIT = chess.engine.Limit(depth=args.depth) if args.depth > 0 else chess.engine.Limit(time=args.movetime)
    games = args.games
    nnue_eng = new_engine(True)
    hand_eng = new_engine(False)
    wins = losses = draws = 0
    try:
        for g in range(games):
            nnue_white = (g % 2 == 0)
            result = play_game(nnue_eng, hand_eng, nnue_white)
            if result == "1/2-1/2":
                draws += 1; tag = "draw"
            else:
                nnue_won = (result == "1-0") == nnue_white
                if nnue_won: wins += 1; tag = "NNUE win"
                else: losses += 1; tag = "handcrafted win"
            print(f"  game {g+1:2d}: NNUE plays {'White' if nnue_white else 'Black'} -> {result} ({tag})", flush=True)
    finally:
        nnue_eng.quit(); hand_eng.quit()

    limit_desc = f"depth {args.depth}" if args.depth > 0 else f"{args.movetime}s/move"
    print(f"\nNNUE vs handcrafted, {games} games @ {limit_desc} (same engine, same search):")
    print(f"  NNUE wins:        {wins}")
    print(f"  handcrafted wins: {losses}")
    print(f"  draws:            {draws}")


if __name__ == "__main__":
    main()
