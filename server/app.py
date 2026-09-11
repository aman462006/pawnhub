"""FastAPI backend: a thin wrapper around the existing UCI chess engine.

The engine itself is NOT modified. Each of the four bots is just the same engine.exe played at a
different strength, controlled by search depth / time (and a little randomness for the weaker ones
so they feel like genuine beginners rather than a perfect engine playing fast).

Endpoints:
  GET  /api/bots       -> list of bots (id, name, tier)
  POST /api/bot-move   -> { fen, bot } -> { move (uci), san }
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
ENGINE_PATH = os.path.join(ROOT, "engine", "build", "engine.exe")
WEB_DIST = os.path.join(ROOT, "web", "dist")

# The four bots: same engine, increasing strength. "random" = chance of playing a random legal
# move instead of the engine's, which makes the weaker bots blunder like a human beginner.
BOTS = {
    "veer":       {"name": "Veer",       "tier": "Beginner",     "depth": 4,          "random": 0.30},
    "paramveer":  {"name": "Paramveer",  "tier": "Intermediate", "depth": 7,          "random": 0.10},
    "dharamveer": {"name": "Dharamveer", "tier": "Advanced",     "depth": 10,         "random": 0.0},
    "aman":       {"name": "Aman",       "tier": "Strongest",    "movetime": 1.0,     "random": 0.0},
}

_engine: chess.engine.SimpleEngine | None = None
_lock = threading.Lock()


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
            limit = (chess.engine.Limit(time=cfg["movetime"]) if "movetime" in cfg
                     else chess.engine.Limit(depth=cfg["depth"]))
            move = _engine.play(board, limit).move
        san = board.san(move)

    return {"move": move.uci(), "san": san}


# In production, serve the built frontend (web/dist) from the same server. In development the
# Vite dev server handles the UI and proxies /api here, so this mount is simply absent.
if os.path.isdir(WEB_DIST):
    app.mount("/", StaticFiles(directory=WEB_DIST, html=True), name="web")
