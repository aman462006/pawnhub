"""Re-label self-play positions with Stockfish evaluations (knowledge distillation).

Our gen-1 net was capped by its teacher (shallow PeSTO). Here we swap in a far stronger
teacher: Stockfish at a fixed depth. We keep each position's game result (WDL) and only
replace the score. The retrained net imitates a ~2600 evaluator instead of a ~2000 one.

Sharding: pass --shard i --nshards n to process only every n-th line, so several copies can
run in parallel (each with its own Stockfish). Reads 'fen,score,result', writes 'fen,sf_score,result'.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import chess.engine

import common

CLAMP = 2000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--infile", required=True)
    ap.add_argument("--outfile", required=True)
    ap.add_argument("--depth", type=int, default=10)
    ap.add_argument("--shard", type=int, default=0)
    ap.add_argument("--nshards", type=int, default=1)
    args = ap.parse_args()

    sf = chess.engine.SimpleEngine.popen_uci(common.STOCKFISH_PATH)
    sf.configure({"Threads": 1, "Hash": 64})

    done = 0
    with open(args.infile, "r", encoding="ascii") as fin, \
         open(args.outfile, "w", encoding="ascii") as fout:
        for i, line in enumerate(fin):
            if i % args.nshards != args.shard:
                continue
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split(",")
            if len(parts) < 3:
                continue
            fen = ",".join(parts[:-2])
            result = parts[-1]
            board = chess.Board(fen)
            info = sf.analyse(board, chess.engine.Limit(depth=args.depth))
            cp = info["score"].pov(board.turn).score(mate_score=100000)
            cp = max(-CLAMP, min(CLAMP, cp))
            fout.write(f"{fen},{cp},{result}\n")
            done += 1
            if done % 20000 == 0:
                print(f"  shard {args.shard}: {done} labeled", flush=True)

    sf.quit()
    print(f"shard {args.shard} done: {done} positions -> {args.outfile}", flush=True)


if __name__ == "__main__":
    main()
