"""Train the two-perspective NNUE (768 -> 512x2 -> 1) with WDL-blended targets.

Scales to ~14M positions: features are stored compactly on the CPU as int16 and streamed to
the GPU per batch (loading everything onto a 6 GB GPU at once would overflow). Target per
position blends the (Stockfish) search score with the game result:

    target_winprob = LAMBDA * sigmoid(score/400) + (1 - LAMBDA) * result
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chess
import numpy as np
import torch
import torch.nn as nn

import common
from features import NUM_FEATURES, MAX_ACTIVE, active_features, WHITE, BLACK

PAD = NUM_FEATURES
CP_SCALE = 400.0
HIDDEN = 512
LAMBDA = 0.6


class NNUE(nn.Module):
    def __init__(self, hidden: int = HIDDEN):
        super().__init__()
        self.ft = nn.Embedding(NUM_FEATURES + 1, hidden, padding_idx=PAD)
        self.ft_bias = nn.Parameter(torch.zeros(hidden))
        self.out = nn.Linear(2 * hidden, 1)

    def forward(self, fw, fb, stm):
        acc_w = self.ft(fw).sum(1) + self.ft_bias
        acc_b = self.ft(fb).sum(1) + self.ft_bias
        own = torch.where(stm, acc_w, acc_b)
        opp = torch.where(stm, acc_b, acc_w)
        x = torch.relu(torch.cat([own, opp], dim=1))
        return self.out(x).squeeze(1)


def load_dataset(path: str):
    """Parse 'fen,score,result_white' into compact CPU tensors (features as int16)."""
    t0 = time.time()
    with open(path, "r", encoding="ascii") as f:
        lines = f.readlines()
    n = len(lines)
    FW = np.full((n, MAX_ACTIVE), PAD, dtype=np.int16)
    FB = np.full((n, MAX_ACTIVE), PAD, dtype=np.int16)
    STM = np.zeros(n, dtype=bool)
    SC = np.zeros(n, dtype=np.float32)
    RES = np.zeros(n, dtype=np.float32)

    k = 0
    for line in lines:
        line = line.rstrip("\n")
        if not line:
            continue
        parts = line.split(",")
        if len(parts) < 3:
            continue
        fen = ",".join(parts[:-2])
        board = chess.Board(fen)
        wtm = board.turn == chess.WHITE
        fw = active_features(board, WHITE)
        fb = active_features(board, BLACK)
        FW[k, :len(fw)] = fw
        FB[k, :len(fb)] = fb
        STM[k] = wtm
        SC[k] = int(parts[-2])
        rw = float(parts[-1])
        RES[k] = rw if wtm else (1.0 - rw)
        k += 1
    print(f"  encoded {k} positions in {time.time()-t0:.0f}s")
    return (torch.from_numpy(FW[:k]), torch.from_numpy(FB[:k]),
            torch.from_numpy(STM[:k]).unsqueeze(1),
            torch.from_numpy(SC[:k]), torch.from_numpy(RES[:k]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.path.join(common.DATA_DIR, "distill_all.csv"))
    ap.add_argument("--out", default=os.path.join(common.NET_DIR, "nnue.pt"))
    ap.add_argument("--epochs", type=int, default=70)
    ap.add_argument("--batch", type=int, default=32768)
    ap.add_argument("--lr", type=float, default=1.5e-3)
    args = ap.parse_args()
    os.makedirs(common.NET_DIR, exist_ok=True)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"device: {device}" + (f" ({torch.cuda.get_device_name(0)})" if device == "cuda" else ""))

    # Data stays on the CPU; batches are moved to the GPU on demand.
    FW, FB, STM, SC, RES = load_dataset(args.data)
    n = SC.shape[0]
    perm = torch.randperm(n)
    n_val = min(30000, n // 20)
    vi, ti = perm[:n_val], perm[n_val:]

    model = NNUE().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=args.epochs)

    def batch_to_gpu(idx):
        return (FW[idx].to(device).long(), FB[idx].to(device).long(), STM[idx].to(device),
                SC[idx].to(device), RES[idx].to(device))

    def wp(cp):
        return torch.sigmoid(cp / CP_SCALE)

    def target_wp(sc, res):
        return LAMBDA * wp(sc) + (1 - LAMBDA) * res

    ntr = ti.shape[0]
    for epoch in range(1, args.epochs + 1):
        model.train()
        order = ti[torch.randperm(ntr)]
        total = 0.0
        for i in range(0, ntr, args.batch):
            b = order[i:i + args.batch]
            fw, fb, stm, sc, res = batch_to_gpu(b)
            pred = model(fw, fb, stm)
            loss = torch.mean((wp(pred) - target_wp(sc, res)) ** 2)
            opt.zero_grad(); loss.backward(); opt.step()
            total += loss.item() * b.numel()
        sched.step()
        if epoch % 5 == 0 or epoch == 1:
            model.eval()
            with torch.no_grad():
                fw, fb, stm, sc, res = batch_to_gpu(vi)
                vp = model(fw, fb, stm)
                vloss = torch.mean((wp(vp) - target_wp(sc, res)) ** 2).item()
                mae = torch.mean(torch.abs(vp - sc)).item()
            print(f"  epoch {epoch:3d}  train_mse {total/ntr:.5f}  val_mse {vloss:.5f}  val_mae_vs_score {mae:.0f}cp", flush=True)

    torch.save({"state": model.state_dict(), "hidden": HIDDEN}, args.out)
    print(f"saved model -> {args.out}")


if __name__ == "__main__":
    main()
