"""The feature encoding shared by training (Python) and inference (C++).

Two-perspective NNUE: we encode the board TWICE — once from White's point of view and once
from Black's. In each perspective, features are indexed by (is-this-my-piece, piece-type,
square-from-my-side). At evaluation time the two accumulators are read in the order
(side-to-move, opponent), so the network always "faces" the position from the mover's side.
This is what gives it awareness of whose turn it is (tempo/initiative) — the thing the first,
absolute-only net was blind to.

Per perspective there are 768 = 2 (own/opp) x 6 piece types x 64 squares features. The two
accumulators are BOTH tied to absolute piece positions, so each still updates incrementally
when a piece moves.
"""
import chess

NUM_FEATURES = 768
MAX_ACTIVE = 32   # at most 32 pieces on the board

WHITE, BLACK = 0, 1


def feature_index(perspective: int, color: int, piece_type_0based: int, square: int) -> int:
    """Feature index for a piece, as seen from `perspective` (0=White view, 1=Black view)."""
    rel_color = 0 if color == perspective else 1              # 0 = my piece, 1 = opponent's
    rel_sq = square if perspective == WHITE else (square ^ 56)  # flip board for Black's view
    return (rel_color * 6 + piece_type_0based) * 64 + rel_sq


def active_features(board: chess.Board, perspective: int) -> list[int]:
    """Active feature indices for a position, from the given perspective."""
    feats = []
    for square, piece in board.piece_map().items():
        color = WHITE if piece.color == chess.WHITE else BLACK
        pt = piece.piece_type - 1                              # python-chess PAWN=1 -> 0
        feats.append(feature_index(perspective, color, pt, square))
    return feats
