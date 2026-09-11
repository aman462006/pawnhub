"""A/B self-play test: play two engine builds/configs against each other and report the score
and estimated Elo difference. This is our measurement tool — run it after any change to see
whether it actually helped (self-play A/B is far more sensitive than calibration vs Stockfish).

Each game starts from a shared randomised opening (so both sides get the same start), and colors
alternate for fairness.

Usage:
  python tools/match_ab.py --a engine\\build\\engine.exe --b engine\\build\\engine_base.exe --games 60 --movetime 0.2
  (flags --a-nnue / --b-nnue turn on the NNUE eval for that side)
"""
import argparse
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import chess.engine

import common


def random_opening(rng, plies):
    board = chess.Board()
    for _ in range(plies):
        moves = list(board.legal_moves)
        if not moves:
            return chess.Board()
        board.push(rng.choice(moves))
    return board if not board.is_game_over() else chess.Board()


def play(a, b, start_fen, a_white, limit):
    board = chess.Board(start_fen)
    while not board.is_game_over(claim_draw=True):
        eng = a if ((board.turn == chess.WHITE) == a_white) else b
        board.push(eng.play(board, limit).move)
    return board.result(claim_draw=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default=os.path.join(common.ROOT, "engine", "build", "engine.exe"))
    ap.add_argument("--b", default=os.path.join(common.ROOT, "engine", "build", "engine_base.exe"))
    ap.add_argument("--a-nnue", action="store_true")
    ap.add_argument("--b-nnue", action="store_true")
    ap.add_argument("--movetime", type=float, default=0.2)
    ap.add_argument("--games", type=int, default=60)
    ap.add_argument("--openrandom", type=int, default=4)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    a = chess.engine.SimpleEngine.popen_uci(args.a); a.configure({"UseNNUE": args.a_nnue})
    b = chess.engine.SimpleEngine.popen_uci(args.b); b.configure({"UseNNUE": args.b_nnue})
    limit = chess.engine.Limit(time=args.movetime)
    rng = random.Random(args.seed)

    w = d = l = 0
    try:
        for g in range(args.games):
            start = random_opening(rng, args.openrandom).fen()
            a_white = (g % 2 == 0)
            res = play(a, b, start, a_white, limit)
            if res == "1/2-1/2":
                d += 1
            else:
                a_won = (res == "1-0") == a_white
                if a_won: w += 1
                else: l += 1
            if (g + 1) % 10 == 0:
                print(f"  after {g+1}: A +{w} ={d} -{l}", flush=True)
    finally:
        a.quit(); b.quit()

    n = w + d + l
    score = (w + 0.5 * d) / n
    if 0 < score < 1:
        elo = -400.0 * math.log10(1.0 / score - 1.0)
        # rough 95% error bar
        se = math.sqrt(score * (1 - score) / n)
        elo_hi = -400.0 * math.log10(1.0 / min(0.999, score + 1.96 * se) - 1.0)
        err = elo_hi - elo
    else:
        elo, err = float("inf"), 0.0
    print(f"\nA vs B: +{w} ={d} -{l}  (score {score:.3f})")
    print(f"Estimated Elo(A) - Elo(B) = {elo:+.0f} +/- {err:.0f}")


if __name__ == "__main__":
    main()
