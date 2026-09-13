"""FastAPI backend: a thin wrapper around the existing UCI chess engine.

The engine itself is NOT modified. Each of the four bots is just the same engine.exe played at a
different strength, controlled by search depth / time (and a little randomness for the weaker ones
so they feel like genuine beginners rather than a perfect engine playing fast).

Endpoints:
  GET  /api/bots       -> list of bots (id, name, tier)
  POST /api/bot-move   -> { fen, bot } -> { move (uci), san }
  POST /api/analyze    -> { fen, bot } -> { fen, turn, best, line } (best move + continuation)
"""
import os
import random
import threading
from contextlib import asynccontextmanager

import chess
import chess.engine
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The engine binary is "engine.exe" on Windows and "engine" on Linux (e.g. in the Docker
# deploy image). Allow an explicit override via the ENGINE_PATH environment variable.
ENGINE_NAME = "engine.exe" if os.name == "nt" else "engine"
ENGINE_PATH = os.environ.get("ENGINE_PATH") or os.path.join(ROOT, "engine", "build", ENGINE_NAME)
WEB_DIST = os.path.join(ROOT, "web", "dist")

# The four bots: same engine, increasing strength. "random" = chance of playing a random legal
# move instead of the engine's, which makes the weaker bots blunder like a human beginner.
BOTS = {
    "veer":       {"name": "Veer",       "tier": "Beginner",     "depth": 4,          "random": 0.30},
    "paramveer":  {"name": "Paramveer",  "tier": "Intermediate", "depth": 7,          "random": 0.10},
    "dharamveer": {"name": "Dharamveer", "tier": "Advanced",     "depth": 10,         "random": 0.0},
    "aman":       {"name": "Aman",       "tier": "Strongest",    "movetime": 1.0,     "random": 0.0},
}

# Our engine reports mate as a huge score (MATE = 1_000_000 in search.cpp) rather than a UCI
# "score mate" token, so we translate large centipawn values back into a mate distance here.
MATE = 1_000_000
MATE_THRESHOLD = MATE - 1_000

_engine: chess.engine.SimpleEngine | None = None
_lock = threading.Lock()


def _limit_for(cfg: dict) -> "chess.engine.Limit":
    """The search budget that defines a bot's strength (fixed time for Aman, else fixed depth)."""
    return (chess.engine.Limit(time=cfg["movetime"]) if "movetime" in cfg
            else chess.engine.Limit(depth=cfg["depth"]))


@asynccontextmanager
async def lifespan(_app: FastAPI):
    global _engine
    _engine = chess.engine.SimpleEngine.popen_uci(ENGINE_PATH)
    _engine.configure({"UseNNUE": True})   # all bots use the neural net; strength varies by depth
    try:
        yield
    finally:
        if _engine:
            _engine.quit()


app = FastAPI(title="Chess Bots", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"],
)


class MoveRequest(BaseModel):
    fen: str
    bot: str


@app.get("/api/bots")
def list_bots():
    return [{"id": bot_id, "name": b["name"], "tier": b["tier"]} for bot_id, b in BOTS.items()]


@app.post("/api/bot-move")
def bot_move(req: MoveRequest):
    cfg = BOTS.get(req.bot)
    if cfg is None:
        raise HTTPException(status_code=400, detail=f"unknown bot '{req.bot}'")
    try:
        board = chess.Board(req.fen)
    except ValueError:
        raise HTTPException(status_code=400, detail="invalid FEN")
    if board.is_game_over():
        raise HTTPException(status_code=400, detail="game is already over")

    with _lock:
        if cfg["random"] and random.random() < cfg["random"]:
            move = random.choice(list(board.legal_moves))
        else:
            move = _engine.play(board, _limit_for(cfg)).move
        san = board.san(move)

    return {"move": move.uci(), "san": san}


class AnalyzeRequest(BaseModel):
    fen: str
    bot: str


def _score_from_white(pov_score: "chess.engine.PovScore") -> dict:
    """Turn the engine's raw score into { cp, mate } from White's point of view.

    The engine never emits a UCI "score mate", so a forced mate arrives as a centipawn value
    close to MATE. We detect that and convert it back into a signed number of moves.
    """
    cp = pov_score.white().score()  # int centipawns, White's perspective (never None for us)
    if cp is None:
        return {"cp": None, "mate": None}
    if cp >= MATE_THRESHOLD:
        return {"cp": None, "mate": (MATE - cp + 1) // 2}       # White mates in N
    if cp <= -MATE_THRESHOLD:
        return {"cp": None, "mate": -((MATE + cp + 1) // 2)}    # White gets mated in N
    return {"cp": cp, "mate": None}


@app.post("/api/analyze")
def analyze(req: AnalyzeRequest):
    """Ask the chosen bot for the best move in a position, plus a short principal variation.

    Because the engine exposes no MultiPV / PV output, we get the best move + score in one search
    and then build the continuation by playing the engine against itself for a few more plies at a
    light fixed depth (illustrative only — the headline move/eval uses the bot's real strength).
    """
    cfg = BOTS.get(req.bot)
    if cfg is None:
        raise HTTPException(status_code=400, detail=f"unknown bot '{req.bot}'")
    try:
        board = chess.Board(req.fen)
    except ValueError:
        raise HTTPException(status_code=400, detail="invalid FEN")
    if not board.is_valid():
        raise HTTPException(status_code=400, detail="illegal position")
    if board.is_game_over():
        raise HTTPException(status_code=400, detail="position is already game over")

    pv_limit = chess.engine.Limit(depth=min(cfg.get("depth", 8), 8))
    with _lock:
        result = _engine.play(board, _limit_for(cfg), info=chess.engine.INFO_SCORE)
        best = result.move
        score = _score_from_white(result.info["score"]) if result.info.get("score") else {"cp": None, "mate": None}

        # Build the continuation line (SAN), best move first, then a few engine replies.
        line = [board.san(best)]
        walk = board.copy()
        walk.push(best)
        for _ in range(5):
            if walk.is_game_over():
                break
            nxt = _engine.play(walk, pv_limit).move
            if nxt is None:
                break
            line.append(walk.san(nxt))
            walk.push(nxt)

    return {
        "fen": board.fen(),
        "turn": "white" if board.turn == chess.WHITE else "black",
        "best": {"move": best.uci(), "san": board.san(best), **score},
        "line": line,
    }


# In production, serve the built frontend (web/dist) from the same server. In development the
# Vite dev server handles the UI and proxies /api here, so this mount is simply absent.
if os.path.isdir(WEB_DIST):
    app.mount("/", StaticFiles(directory=WEB_DIST, html=True), name="web")
