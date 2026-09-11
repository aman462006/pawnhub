"""Bot-vs-bot auto-play. Our measuring stick for the whole project.

Every future "did the engine actually get stronger?" question gets answered here:
pit the new engine against an old one (or Stockfish later) over many games and look
at the win/draw/loss score. This is how we turn "feels better" into a real number.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import chess

from .engines.base import Engine


@dataclass
class MatchResult:
    white: str
    black: str
    wins_white: int = 0
    wins_black: int = 0
    draws: int = 0
    games: list[str] = field(default_factory=list)  # result strings like "1-0"

    def summary(self) -> str:
        total = self.wins_white + self.wins_black + self.draws
        return (
            f"{self.white} (White) vs {self.black} (Black) over {total} games:\n"
            f"  {self.white} wins: {self.wins_white}\n"
            f"  {self.black} wins: {self.wins_black}\n"
            f"  draws:            {self.draws}"
        )


def play_game(white: Engine, black: Engine, max_plies: int = 400) -> chess.Board:
    """Play one full game and return the finished board.

    `max_plies` is a safety valve so a game between two weak/random engines that
    shuffles forever eventually stops and is scored as a draw.
    """
    board = chess.Board()
    while not board.is_game_over() and board.ply() < max_plies:
        engine = white if board.turn == chess.WHITE else black
        board.push(engine.choose_move(board))
    return board


def play_match(white: Engine, black: Engine, games: int = 10) -> MatchResult:
    """Play `games` games with fixed colors and tally the score."""
    result = MatchResult(white=white.name, black=black.name)
    for _ in range(games):
        board = play_game(white, black)
        outcome = board.result()  # "1-0", "0-1", or "1/2-1/2"
        result.games.append(outcome)
        if outcome == "1-0":
            result.wins_white += 1
        elif outcome == "0-1":
            result.wins_black += 1
        else:
            result.draws += 1
    return result


if __name__ == "__main__":
    # Quick self-test: Random vs Random. Expect a spread of results and no crashes.
    from .engines.random_engine import RandomEngine

    res = play_match(RandomEngine(seed=1), RandomEngine(seed=2), games=20)
    print(res.summary())
