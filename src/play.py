"""Play a game against an engine in the terminal.

Usage (from the project root, with the venv active):
    py -m src.play                 # you are White vs the Random engine
    py -m src.play --color black   # you are Black
    py -m src.play --engine random # choose the opponent (only 'random' for now)

Type moves as SAN (e4, Nf3, O-O) or UCI (e2e4). Commands: 'quit', 'resign', 'board'.
"""
from __future__ import annotations

import argparse

import chess

from .engines.advanced_engine import AdvancedEngine
from .engines.random_engine import RandomEngine
from .engines.search_engine import AlphaBetaEngine
from .gameio import describe_result, parse_human_move, render

ENGINES = {
    "random": RandomEngine,       # "Pawn"
    "alphabeta": AlphaBetaEngine,  # "Knight"
    "advanced": AdvancedEngine,    # "Bishop"
}


def build_engine(name: str) -> object:
    return ENGINES[name]()


def human_turn(board: chess.Board) -> bool:
    """Prompt the human until they enter a legal move or a command.

    Returns True to keep playing, False if the human quit/resigned.
    """
    while True:
        raw = input("Your move: ").strip().lower()
        if raw in {"quit", "exit"}:
            print("Bye.")
            return False
        if raw == "resign":
            print("You resigned.")
            return False
        if raw == "board":
            print(render(board))
            continue

        move = parse_human_move(board, raw)
        if move is None:
            print("  Illegal or unrecognized move. Try SAN (Nf3) or UCI (g1f3).")
            continue
        board.push(move)
        return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Play chess against an engine.")
    parser.add_argument("--color", choices=["white", "black"], default="white",
                        help="the color YOU play (default: white)")
    parser.add_argument("--engine", choices=list(ENGINES), default="advanced",
                        help="which engine to play against (default: advanced)")
    args = parser.parse_args()

    human_is_white = args.color == "white"
    engine = build_engine(args.engine)
    board = chess.Board()

    print(f"You are {args.color.upper()} vs {engine.name}. Type 'quit' to stop.\n")
    print(render(board))

    while not board.is_game_over():
        human_to_move = board.turn == chess.WHITE and human_is_white \
            or board.turn == chess.BLACK and not human_is_white

        if human_to_move:
            if not human_turn(board):
                return
        else:
            move = engine.choose_move(board)
            print(f"{engine.name} plays: {board.san(move)}")
            board.push(move)

        print("\n" + render(board) + "\n")

    print(describe_result(board))


if __name__ == "__main__":
    main()
