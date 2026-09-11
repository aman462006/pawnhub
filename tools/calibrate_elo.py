"""Estimate our engine's real Elo by playing it against Stockfish capped at known Elo levels.

Stockfish's UCI_LimitStrength + UCI_Elo make it play at a target rating. We play a match against
a few anchor ratings; from the score against each, the standard Elo formula gives an implied
rating. Averaging the anchors where the match wasn't lopsided gives a robust estimate.

Usage:
  python tools/calibrate_elo.py --movetime 0.4 --games 12 --anchors 1800,2000,2200 [--nnue]
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import chess.engine

import common


def implied_elo(anchor: int, score: float) -> float:
    score = min(max(score, 0.01), 0.99)
    return anchor - 400.0 * math.log10(1.0 / score - 1.0)


def play(our, sf, our_white: bool, movetime: float) -> str:
    board = chess.Board()
    while not board.is_game_over(claim_draw=True):
        our_to_move = (board.turn == chess.WHITE) == our_white
        eng = our if our_to_move else sf
        board.push(eng.play(board, chess.engine.Limit(time=movetime)).move)
    return board.result(claim_draw=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--movetime", type=float, default=0.4)
    ap.add_argument("--games", type=int, default=12)
    ap.add_argument("--anchors", default="1800,2000,2200")
    ap.add_argument("--nnue", action="store_true", help="use the NNUE eval (default: PeSTO)")
    args = ap.parse_args()
    anchors = [int(x) for x in args.anchors.split(",")]

    our = chess.engine.SimpleEngine.popen_uci(common.ENGINE_PATH)
    our.configure({"UseNNUE": args.nnue})
    sf = chess.engine.SimpleEngine.popen_uci(common.STOCKFISH_PATH)
    sf.configure({"UCI_LimitStrength": True})

    eval_name = "NNUE" if args.nnue else "PeSTO"
    print(f"Calibrating {eval_name} engine @ {args.movetime}s/move, {args.games} games per anchor\n")

    estimates = []
    try:
        for A in anchors:
            sf.configure({"UCI_Elo": A})
            w = l = d = 0
            for g in range(args.games):
                our_white = (g % 2 == 0)
                res = play(our, sf, our_white, args.movetime)
                if res == "1/2-1/2":
                    d += 1
                else:
                    our_won = (res == "1-0") == our_white
                    if our_won: w += 1
                    else: l += 1
            score = (w + 0.5 * d) / args.games
            est = implied_elo(A, score)
            estimates.append((A, score, est))
            print(f"  vs SF@{A}: +{w} ={d} -{l}  score {score:.2f}  -> implied Elo {est:.0f}", flush=True)
    finally:
        our.quit(); sf.quit()

    reliable = [e for (A, s, e) in estimates if 0.15 <= s <= 0.85] or [e for (_, _, e) in estimates]
    print(f"\nEstimated Elo of {eval_name} engine (~{args.movetime}s/move): {sum(reliable)/len(reliable):.0f}")


if __name__ == "__main__":
    main()
