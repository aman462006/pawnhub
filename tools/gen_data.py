"""Generate NNUE training data: (position, Stockfish-evaluation) pairs.

Strategy for GOOD data:
  * Diversity — start each game with a few random moves so we don't just see main-line
    openings, and inject occasional random moves so positions span the whole game.
  * Realism — otherwise let Stockfish pick moves, so positions look like real chess.
  * Quiet positions only — skip positions where the side to move is in check; a static
    evaluation is meaningless mid-tactic (that's what search is for).
  * Labels — each recorded position gets Stockfish's evaluation, in centipawns from the
    side-to-move's perspective, clamped so rare mate scores don't dominate training.

Output: a CSV "fen,cp" (one position per line) in data/.

Usage:
  python tools/gen_data.py --positions 200000 --depth 8 --out data/positions.csv
"""
import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import chess.engine

import common

CLAMP = 2000   # centipawns; keep labels in a sane range (mate -> +/-CLAMP)


def gen(positions: int, depth: int, out_path: str, random_plies: int,
        random_prob: float, max_plies: int, seed: int) -> None:
    rng = random.Random(seed)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    sf = chess.engine.SimpleEngine.popen_uci(common.STOCKFISH_PATH)
    sf.configure({"Threads": 2, "Hash": 256})

    written = 0
    t0 = time.time()
    with open(out_path, "w", encoding="ascii") as f:
        while written < positions:
            board = chess.Board()
            ply = 0
            while not board.is_game_over() and ply < max_plies and written < positions:
                # First few plies: purely random, for opening variety.
                if ply < random_plies:
                    board.push(rng.choice(list(board.legal_moves)))
                    ply += 1
                    continue

                info = sf.analyse(board, chess.engine.Limit(depth=depth))
                score = info["score"].pov(board.turn)
                cp = score.score(mate_score=100000)
                cp = max(-CLAMP, min(CLAMP, cp))   # clamp (also squashes mate scores)

                # Record only quiet positions (side to move not in check).
                if not board.is_check():
                    f.write(f"{board.fen()},{cp}\n")
                    written += 1
                    if written % 2000 == 0:
                        rate = written / (time.time() - t0)
                        print(f"  {written}/{positions} positions ({rate:.0f}/s)", flush=True)

                # Choose the next move: usually Stockfish's, sometimes random (variety).
                pv = info.get("pv")
                if rng.random() < random_prob or not pv:
                    move = rng.choice(list(board.legal_moves))
                else:
                    move = pv[0]
                board.push(move)
                ply += 1

    sf.quit()
    dt = time.time() - t0
    print(f"Done: {written} positions in {dt:.0f}s ({written/dt:.0f}/s) -> {out_path}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--positions", type=int, default=200000)
    ap.add_argument("--depth", type=int, default=8)
    ap.add_argument("--out", default=os.path.join(common.DATA_DIR, "positions.csv"))
    ap.add_argument("--random-plies", type=int, default=6)
    ap.add_argument("--random-prob", type=float, default=0.15)
    ap.add_argument("--max-plies", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    gen(args.positions, args.depth, args.out, args.random_plies,
        args.random_prob, args.max_plies, args.seed)
