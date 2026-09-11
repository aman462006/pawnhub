"""Quantize the two-perspective NNUE to integers and write the file the C++ engine loads.

Integer evaluation (must match nnue.cpp exactly), from the side-to-move's view:
    acc_side[h] = b1_q[h] + sum over that side's active features f of W1_q[f][h]
    x = [ relu(acc_stm) , relu(acc_opp) ]                      (length 2H, int32)
    out = sum_i W2_q[i] * x[i]                                 (int64)
    eval_cp = trunc(out / (QA*QW)) + b2_cp

Output: nets/nnue.bin (magic "NNU2") + nets/verify.csv (reference side-to-move evals).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import numpy as np
import torch

import common
from features import NUM_FEATURES, active_features, WHITE, BLACK
from train_nnue import NNUE

QA = 256
QW = 256
MAGIC = b"NNU2"

VERIFY_FENS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
    "8/8/8/4k3/8/4K3/4P3/8 w - - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "6k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1",
    "rnbq1rk1/pp2bppp/2p2n2/3p4/2PP4/2N1PN2/PP3PPP/R1BQKB1R w KQ - 0 7",
]


def quantized_eval(W1q, b1q, W2q, b2_cp, board):
    def acc(persp):
        a = b1q.astype(np.int64).copy()
        for f in active_features(board, persp):
            a += W1q[f].astype(np.int64)
        return a
    aw, ab = acc(WHITE), acc(BLACK)
    stm_white = board.turn == chess.WHITE
    own, opp = (aw, ab) if stm_white else (ab, aw)
    x = np.concatenate([np.maximum(own, 0), np.maximum(opp, 0)])
    out = int(np.sum(W2q.astype(np.int64) * x))
    d = QA * QW
    q = out // d if out >= 0 else -((-out) // d)
    return q + int(b2_cp)


def main():
    ckpt = torch.load(os.path.join(common.NET_DIR, "nnue.pt"), map_location="cpu")
    H = ckpt["hidden"]
    model = NNUE(H); model.load_state_dict(ckpt["state"]); model.eval()

    W1 = model.ft.weight.detach().numpy()[:NUM_FEATURES]     # (768, H)
    b1 = model.ft_bias.detach().numpy()                      # (H,)
    W2 = model.out.weight.detach().numpy()[0]                # (2H,)
    b2 = float(model.out.bias.detach().numpy()[0])

    W1q = np.round(W1 * QA).astype(np.int16)
    b1q = np.round(b1 * QA).astype(np.int32)
    W2q = np.round(W2 * QW).astype(np.int16)
    b2_cp = int(round(b2))

    out_path = os.path.join(common.NET_DIR, "nnue.bin")
    with open(out_path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<iii", H, QA, QW))
        f.write(W1q.tobytes())      # 768*H int16
        f.write(b1q.tobytes())      # H int32
        f.write(W2q.tobytes())      # 2H int16
        f.write(struct.pack("<i", b2_cp))
    print(f"wrote {out_path} ({os.path.getsize(out_path)} bytes, H={H})")

    verify_path = os.path.join(common.NET_DIR, "verify.csv")
    errs = []
    with open(verify_path, "w", encoding="ascii") as vf:
        for fen in VERIFY_FENS:
            board = chess.Board(fen)
            row = active_features(board, WHITE)
            rowb = active_features(board, BLACK)
            with torch.no_grad():
                fw = torch.tensor([row + [NUM_FEATURES] * (32 - len(row))], dtype=torch.long)
                fb = torch.tensor([rowb + [NUM_FEATURES] * (32 - len(rowb))], dtype=torch.long)
                stm = torch.tensor([[board.turn == chess.WHITE]], dtype=torch.bool)
                fpred = float(model(fw, fb, stm))
            qpred = quantized_eval(W1q, b1q, W2q, b2_cp, board)
            errs.append(abs(fpred - qpred))
            vf.write(f"{fen},{qpred}\n")
            print(f"  {fen[:30]:30s} float {fpred:+8.1f}  quant {qpred:+5d}  (stm POV)")
    print(f"quantization error: mean {np.mean(errs):.1f}cp, max {np.max(errs):.1f}cp")
    print(f"wrote reference evals -> {verify_path}")


if __name__ == "__main__":
    main()
