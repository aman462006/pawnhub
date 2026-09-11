"""Shared paths/helpers for the Phase 3 (NNUE) tooling."""
import glob
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE_PATH = os.path.join(ROOT, "engine", "build", "engine.exe")
DATA_DIR = os.path.join(ROOT, "data")
NET_DIR = os.path.join(ROOT, "nets")


def find_stockfish() -> str:
    """Locate the Stockfish executable (PATH first, then the winget install location)."""
    p = shutil.which("stockfish")
    if p:
        return p
    local = os.environ.get("LOCALAPPDATA", "")
    pattern = os.path.join(local, "Microsoft", "WinGet", "Packages",
                           "Stockfish.Stockfish*", "**", "stockfish*.exe")
    hits = glob.glob(pattern, recursive=True)
    if hits:
        return hits[0]
    raise FileNotFoundError("Stockfish not found. Install with: winget install Stockfish.Stockfish")


STOCKFISH_PATH = find_stockfish()
