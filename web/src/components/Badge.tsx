// Avatars built from the chess pieces themselves — the board's own artwork, reused so the
// bots feel part of the game. No robot art, no emoji, no letter monograms.

import { User } from 'lucide-react'
import { defaultPieces } from 'react-chessboard'
import type { Piece } from '../lib/bots'

// react-chessboard exposes each piece as a render function keyed by colour+type (e.g. 'bK').
// We use the dark set so the pieces read as crisp silhouettes on our light surfaces.
const renderPiece = (piece: Piece) =>
  defaultPieces['b' + piece]?.({ svgStyle: { width: '100%', height: '100%' } }) ?? null

export function PieceBadge({
  piece,
  size = 42,
  accent = false,
}: {
  piece: Piece
  size?: number
  accent?: boolean
}) {
  return (
    <span
      className={'piece-badge' + (accent ? ' accent' : '')}
      style={{ width: size, height: size, borderRadius: Math.round(size * 0.22) }}
    >
      <span className="piece-badge-art" style={{ width: size * 0.72, height: size * 0.72 }}>
        {renderPiece(piece)}
      </span>
    </span>
  )
}

// The human player's avatar — a simple, neutral figure.
export function UserBadge({ size = 34 }: { size?: number }) {
  return (
    <span
      className="piece-badge user"
      style={{ width: size, height: size, borderRadius: Math.round(size * 0.22) }}
    >
      <User strokeWidth={1.7} style={{ width: size * 0.5, height: size * 0.5 }} />
    </span>
  )
}
