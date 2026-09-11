"""Small helpers for showing the board and reading a human's move in the terminal.

Kept separate from game logic so the "how it looks / how we talk to the user" code
doesn't tangle with the "rules of chess" code.
"""
from __future__ import annotations

import chess


def render(board: chess.Board) -> str:
    """Return a printable board from White's perspective, with rank/file labels."""
    # board.unicode() draws actual piece glyphs; invert_color keeps White at the
    # bottom regardless of whose turn it is.
    return board.unicode(borders=True, invert_color=True)


def parse_human_move(board: chess.Board, text: str) -> chess.Move | None:
    """Turn user text into a legal move, or return None if it isn't one.

    Accepts either SAN ("Nf3", "e4", "O-O") or raw UCI ("g1f3"), because beginners
    and engines speak different dialects and we want to accept both.
    """
    text = text.strip()
    if not text:
        return None

    # Try SAN first (how humans write moves).
    try:
        return board.parse_san(text)
    except ValueError:
        pass

    # Fall back to UCI (how engines write moves), and confirm it's actually legal.
    try:
        move = chess.Move.from_uci(text)
    except ValueError:
        return None
    return move if move in board.legal_moves else None


def describe_result(board: chess.Board) -> str:
    """Human-friendly one-liner explaining how the finished game ended."""
    if board.is_checkmate():
        winner = "Black" if board.turn == chess.WHITE else "White"
        return f"Checkmate — {winner} wins."
    if board.is_stalemate():
        return "Draw — stalemate."
    if board.is_insufficient_material():
        return "Draw — insufficient material."
    if board.is_seventyfive_moves():
        return "Draw — 75-move rule."
    if board.is_fivefold_repetition():
        return "Draw — fivefold repetition."
    if board.can_claim_draw():
        return "Draw — claimable (50-move / threefold repetition)."
    return f"Game over — result {board.result()}."
