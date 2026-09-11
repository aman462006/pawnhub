import { useState, type CSSProperties } from 'react'
import { Chessboard } from 'react-chessboard'
import type { Square } from 'chess.js'
import type { Side } from '../hooks/useChessGame'

interface BoardProps {
  fen: string
  orientation: Side
  lastMove: [Square, Square] | null
  checkSquare: Square | null
  interactive: boolean
  legalTargets: (from: Square) => Square[]
  onMove: (from: Square, to: Square) => boolean
}

// Which squares currently hold a piece (used to draw capture rings vs. move dots).
function occupied(fen: string): Set<string> {
  const set = new Set<string>()
  const rows = fen.split(' ')[0].split('/')
  for (let r = 0; r < 8; r++) {
    let file = 0
    for (const ch of rows[r]) {
      if (ch >= '1' && ch <= '9') file += Number(ch)
      else {
        set.add('abcdefgh'[file] + (8 - r))
        file++
      }
    }
  }
  return set
}

const GOLD = 'rgba(201, 168, 74, 0.45)'
const GREEN = 'rgba(47, 111, 79, 0.40)'
const RED = 'rgba(176, 70, 59, 0.50)'

export function Board({
  fen,
  orientation,
  lastMove,
  checkSquare,
  interactive,
  legalTargets,
  onMove,
}: BoardProps) {
  const [selected, setSelected] = useState<Square | null>(null)
  const [targets, setTargets] = useState<Square[]>([])

  const clear = () => {
    setSelected(null)
    setTargets([])
  }

  const select = (sq: Square) => {
    const t = legalTargets(sq)
    if (t.length) {
      setSelected(sq)
      setTargets(t)
    } else {
      clear()
    }
  }

  const handleSquareClick = ({ square }: { square: string }) => {
    if (!interactive) return
    const sq = square as Square
    if (selected && targets.includes(sq)) {
      onMove(selected, sq)
      clear()
      return
    }
    if (sq === selected) {
      clear()
      return
    }
    select(sq)
  }

  const handleDrop = ({
    sourceSquare,
    targetSquare,
  }: {
    sourceSquare: string
    targetSquare: string | null
  }) => {
    if (!interactive || !targetSquare) return false
    const ok = onMove(sourceSquare as Square, targetSquare as Square)
    clear()
    return ok
  }

  // Build layered square styles (tints via inset shadow, dots/rings via background image).
  const styles: Record<string, CSSProperties> = {}
  const add = (sq: string, s: CSSProperties) => {
    styles[sq] = { ...(styles[sq] || {}), ...s }
  }

  if (lastMove) {
    add(lastMove[0], { boxShadow: `inset 0 0 0 100px ${GOLD}` })
    add(lastMove[1], { boxShadow: `inset 0 0 0 100px ${GOLD}` })
  }
  if (selected) add(selected, { boxShadow: `inset 0 0 0 100px ${GREEN}` })
  if (checkSquare)
    add(checkSquare, {
      backgroundImage: `radial-gradient(circle, ${RED} 0%, rgba(176,70,59,0.25) 42%, transparent 72%)`,
    })

  const occ = occupied(fen)
  for (const t of targets) {
    if (occ.has(t)) {
      // capture ring
      add(t, {
        backgroundImage: `radial-gradient(circle, transparent 0 58%, ${GREEN} 60% 80%, transparent 82%)`,
      })
    } else {
      // move dot
      add(t, {
        backgroundImage: `radial-gradient(circle, ${GREEN} 0 17%, transparent 19%)`,
      })
    }
  }

  return (
    <div className="board-square">
      <Chessboard
        options={{
          id: 'gambit-board',
          position: fen,
          boardOrientation: orientation,
          onSquareClick: handleSquareClick,
          onPieceDrop: handleDrop,
          squareStyles: styles,
          lightSquareStyle: { backgroundColor: 'var(--sq-light)' },
          darkSquareStyle: { backgroundColor: 'var(--sq-dark)' },
          boardStyle: { borderRadius: '5px', overflow: 'hidden' },
          darkSquareNotationStyle: { color: 'rgba(255,255,255,0.72)', fontWeight: 600 },
          lightSquareNotationStyle: { color: 'rgba(60,60,52,0.55)', fontWeight: 600 },
          animationDurationInMs: 170,
          allowDragging: interactive,
          showNotation: true,
        }}
      />
    </div>
  )
}
