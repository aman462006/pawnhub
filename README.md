# PawnHub — a chess engine built from scratch

**▶ Play it live: [pawnhub.onrender.com](https://pawnhub.onrender.com)**
_(hosted on Render's free tier — it sleeps when idle, so the first load after a while can take a few
seconds to wake up)_

A complete chess engine written from the ground up in **C++**, with a **neural-network evaluation
(NNUE)** trained in **Python**, and a polished **React web app** to play against it. It speaks the
**UCI** protocol, so it runs in any chess GUI or bot host, and it plays at roughly **~2300 Lichess
blitz**.

Nothing about the engine is borrowed: the board representation, move generator, search, and both
evaluations are all hand-written. `python-chess`, `PyTorch`, and Stockfish are used **only** in the
offline training pipeline and the test harness — never inside the engine itself.

```
                 ┌──────────────────────────── PawnHub ────────────────────────────┐
   self-play  →  │  C++ engine (UCI)  ──────────────►  FastAPI backend  ──►  React  │
   + Stockfish   │  bitboards · search · NNUE          (server/app.py)      web app │
   labels        └──────────────────────────────────────────────────────────────────┘
        │                              ▲
        └──────  PyTorch training  ────┘   (trains nets/nnue.bin, loaded by the engine)
```

---

## 1. The C++ engine

Located in `engine/src/`. Built with MinGW `g++` (C++20, `-O2 -march=native -static`) via
`engine/build.ps1` → `engine/build/engine.exe`.

### Board representation

- **Bitboards**: the position is a set of 64-bit integers (one per piece type per colour), so most
  board queries are single machine instructions.
- **Magic bitboards** for sliding pieces (bishop/rook/queen): a perfect-hash lookup turns "what can
  this slider attack given the blockers?" into one multiply-shift-index, precomputed at startup
  (`magic.cpp`).
- **Zobrist hashing** (`zobrist.cpp`) gives each position a 64-bit key for the transposition table
  and repetition detection.
- **make/unmake** with an `Undo` record — the search descends and restores in place rather than
  copying the whole position.

### Move generation

- Full legal move generation (`movegen.cpp`): pseudo-legal generation filtered for pins, checks,
  en-passant edge cases, and castling legality.
- **Perft-verified**: node counts from every standard test position match the published reference
  numbers exactly, which is the standard proof that move generation is bug-free.

### Search (`search.cpp`)

A modern alpha-beta searcher in the negamax formulation:

- **Iterative deepening** with a per-move time/node budget.
- **Transposition table** — 2²² entries (~64 MB), power-of-two sized for a one-instruction index
  mask; stores best move, score, depth, and a bound flag (exact/lower/upper).
- **Principal Variation Search (PVS)** — full window on the first move, null-window scout on the
  rest, re-searching only when a scout beats alpha.
- **Aspiration windows** — from depth 4 on, search a narrow window around the previous score and
  widen only on fail-high/low.
- **Quiescence search** at the leaves — keeps searching captures (and checks) until the position is
  quiet, so the evaluation is never taken mid-exchange. Bad captures are skipped via **SEE** (static
  exchange evaluation).
- **Null-move pruning** — give the opponent a free move; if the position is still ≥ beta, prune
  (disabled when the side has only pawns, to avoid zugzwang blunders).
- **Late Move Reductions (LMR)** — quiet moves tried late are searched shallower, then re-searched
  at full depth only if they surprise us.
- **Reverse futility / futility / late-move pruning** — at shallow depth, skip or prune quiet moves
  that can't realistically affect alpha/beta.
- **Check extensions** — search one ply deeper when in check so forcing lines aren't cut short.
- **Move ordering**: TT move → good captures (SEE + MVV/LVA) → **killer moves** → **history
  heuristic** (`depth²` bonuses on quiet moves that cause cutoffs).

### Evaluation

The engine ships with **two** evaluations behind a single dispatch point, so they can be A/B tested
in the same binary via the `UseNNUE` UCI option:

1. **PeSTO** — a hand-crafted, tapered piece-square-table evaluation (`eval.cpp`) that interpolates
   between middlegame and endgame tables by material. Strong, classical, and used as the training
   bootstrap and a fallback. Roughly **~2000** strength on its own.
2. **NNUE** — the neural network described next, and the default.

### NNUE (the neural evaluation)

A **two-perspective NNUE**, `768 → 512×2 → 1`:

- **Feature encoding** (shared with training in `tools/features.py`): the board is encoded twice —
  once from White's point of view and once from Black's. Each perspective has `768 = 2 (own/opp) ×
  6 piece types × 64 squares` features. At evaluation time the two accumulators are read in
  `(side-to-move, opponent)` order, so the network always "faces" the position from the mover's
  side. This gives it awareness of the tempo/initiative that a single absolute-perspective net is
  blind to.
- **Efficiently-updatable accumulator** (`nnue.cpp`): the first layer's output is kept incrementally.
  When a move changes only a few features, the search updates the accumulator (`nnue_make` /
  `nnue_unmake`) instead of recomputing it — the "U" in NNUE.
- **Integer quantization**: weights are quantized (`QA=256`, `QW=256`) and inference is pure integer
  math (int16 weights, int32/int64 accumulation). The C++ integer path reproduces the PyTorch float
  output to within a few centipawns, checked at export time against `nets/verify.csv`.

### UCI (`uci.cpp`)

Standard UCI so the engine drops into any GUI or the Lichess-bot bridge. Includes a built-in
`datagen` self-play mode and a `--selftest` that runs perft, tactics, SEE, and NNUE
(C++ == Python export) consistency checks.

---

## 2. The training pipeline (Python, `tools/`)

The net is trained offline in four stages. The guiding idea is **knowledge distillation with a
win/draw/loss (WDL) blend**: a strong teacher supplies fine positional judgement (the score), and
the actual game outcome anchors it to "what really wins."

### Stage 1 — self-play data generation

The engine plays games against itself at a fixed node budget from randomised openings
(`engine`'s built-in `datagen`, driven by `tools/gen_selfplay.ps1`). Every quiet position is written
as `fen, score_stm, result_white`:

- `score_stm` — the engine's own search score (centipawns, side-to-move view),
- `result_white` — the eventual game result for White (`1.0 / 0.5 / 0.0`).

### Stage 2 — Stockfish distillation labelling

The first-generation net is limited by its teacher (shallow PeSTO, ~2000). To break that ceiling,
`tools/relabel_stockfish.py` re-labels every position's **score** with **Stockfish at a fixed
depth** (scores clamped to ±2000 cp), while **keeping the original game result**. The retrained net
now imitates a much stronger (~2600) evaluator instead of a ~2000 one. Labelling shards across
processes (`--shard i --nshards n`, driven by `relabel_parallel.ps1`) so multiple Stockfish
instances run in parallel.

### Stage 3 — training (`tools/train_nnue.py`, PyTorch)

- Model mirrors the C++ inference exactly: an embedding table as the feature transformer, summed
  active features + bias per perspective, `ReLU`, concatenated `(own, opp)`, single linear output.
- **Target** per position blends score and result:
  `target = λ·σ(score/400) + (1−λ)·result`, with `λ = 0.6`.
- **Loss**: MSE in win-probability space.
- **Scale**: trains on ~14.9M positions. Features are stored compactly on the CPU as `int16` and
  streamed to the GPU per batch (loading everything onto a 6 GB GPU at once would overflow). Adam +
  cosine-annealed LR, ~70 epochs.

### Stage 4 — quantized export (`tools/export_net.py`)

Converts the float checkpoint into `nets/nnue.bin` (magic `NNU2`): quantizes `W1/b1/W2/b2` to
integers and writes them in the exact byte layout the engine reads. It also emits `nets/verify.csv`
with reference side-to-move evals and reports the mean/max quantization error so any drift between
the PyTorch and C++ evaluations is caught immediately.

**Result**: the gen-2 (Stockfish-distilled) NNUE beats the PeSTO baseline by roughly **+~180 Elo**
in self-play, and lifts the engine to ~2300 on Lichess.

---

## 3. Testing & calibration (`tools/`)

- **Correctness** — `engine.exe --selftest`: perft counts, a tactics suite, SEE, and NNUE
  (C++ vs Python export) agreement.
- **Relative strength** — `tools/match_ab.py`: A/B self-play between two builds/configs (e.g. NNUE
  vs PeSTO, or a candidate net vs the current one) against a fixed baseline binary, reporting the
  score and an Elo delta.
- **Absolute strength** — `tools/calibrate_elo.py` estimates Elo against Stockfish capped at various
  skill levels (noisy), while the **Lichess bot** provides the real, external rating (~2300 blitz).

---

## 4. The web app — PawnHub

A production-quality way to play the engine in the browser. One server hosts both the API and the
built site.

### Frontend (`web/`, React + TypeScript + Vite)

- Interactive board (`react-chessboard` + `chess.js`): click or drag, legal-move dots, last-move and
  check highlights, coordinates, and move animation.
- Calm, white, editorial design — one sage-green accent, no gradients/glass/robot art.
- Choose your **side** (White / Black / Random — if you take Black, the engine moves first), a
  pictorial **build roadmap**, and an interactive **"meet the opponents"** deep-dive.
- The engine is reached **only** through a single isolated interface (`web/src/lib/api.ts`), so the
  UI never touches engine internals.

### Backend (`server/app.py`, FastAPI)

A thin wrapper around `engine.exe` via `python-chess`. The engine is **not modified**. Endpoints:

- `GET /api/bots` — list the bots,
- `POST /api/bot-move` — `{ fen, bot }` → `{ move, san }`.

In production it also serves the built frontend from `web/dist` at the same origin.

### The four opponents

All four are the **same** neural engine; only the search budget (and a little randomness for the
weaker two, so they blunder like humans rather than play perfectly-fast) changes:

| Bot         | Tier         | Piece  | Engine setting        | Approx. strength |
|-------------|--------------|--------|-----------------------|------------------|
| Veer        | Beginner     | Rook   | depth 4, 30% random   | ~800             |
| Paramveer   | Intermediate | Knight | depth 7, 10% random   | ~1400            |
| Dharamveer  | Advanced     | Queen  | depth 10              | ~1900            |
| Aman        | Strongest    | King   | 1 s / move, full      | ~2300            |

---

## Repository layout

```
engine/                 C++ engine
  src/                    bitboard, magic, movegen, position, search, eval, nnue,
                          zobrist, uci, datagen, types
  build.ps1               build script (MinGW g++, C++20, -O2 -march=native, -static)
tools/                  Python training + testing pipeline
  features.py             the 768-feature encoding (shared Python/C++ definition)
  gen_data.py / gen_selfplay.ps1        self-play data generation
  relabel_stockfish.py / relabel_parallel.ps1   Stockfish distillation labelling
  train_nnue.py           train the net (PyTorch, WDL-blended targets)
  export_net.py           quantize + export nets/nnue.bin (+ verify.csv)
  match_ab.py             A/B self-play test harness
  calibrate_elo.py        estimate Elo vs Stockfish skill levels
nets/                   trained network (nnue.bin) + torch checkpoint + reference evals
server/app.py           FastAPI backend wrapping the UCI engine
web/                    React + TypeScript frontend (Vite)
lichess-bot/            lichess-bot bridge + config to run the engine on Lichess
src/, tests/            small Python demo engines + tests
```

---

## Build & run

### Build the engine (Windows, MinGW-w64 `g++`, C++20)

```powershell
.\engine\build.ps1                       # -> engine\build\engine.exe
.\engine\build\engine.exe --selftest     # perft + tactics + SEE + NNUE checks
```

`engine.exe` is a standalone UCI engine. `UseNNUE` (default `true`) switches between the NNUE and
the PeSTO evaluation.

### Run the web app

```powershell
# 1) build the frontend
cd web; npm install; npm run build; cd ..
# 2) serve API + UI together at http://localhost:8000
.\.venv\Scripts\python.exe -m uvicorn server.app:app --port 8000
```

For frontend development: `cd web; npm run dev` (Vite on :5173, proxies `/api` to the backend).

### Train the network

```powershell
py -m pip install -r requirements.txt
# 1) self-play positions            2) re-label scores with Stockfish (WDL kept)
powershell tools\gen_selfplay.ps1
powershell tools\relabel_parallel.ps1
# 3) train, then export the quantized net the engine loads
py tools\train_nnue.py --data data\<dataset>.csv
py tools\export_net.py
```

### Run on Lichess

```powershell
cd lichess-bot
..\.venv\Scripts\python.exe lichess-bot.py     # uses engine.exe (UseNNUE true)
```

---

## Requirements

- **Engine**: MinGW-w64 `g++` with C++20.
- **Pipeline**: Python 3.12, `python-chess`, `numpy`, `PyTorch` (CUDA optional), and Stockfish
  (for distillation labelling and calibration).
- **Web**: Node.js + npm.
