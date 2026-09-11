// All chess game state and logic in one place. Components stay presentational and just read
// from this hook. The engine is reached only through lib/api.ts (getBotMove).

import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { Chess } from 'chess.js'
import type { Square } from 'chess.js'
import { getBotMove } from '../lib/api'
import type { BotId } from '../lib/bots'

export type Side = 'white' | 'black'

export interface GameStatus {
  over: boolean
  result: 'win' | 'loss' | 'draw' | null // from the human's point of view
  reason: string
  turn: Side
  inCheck: boolean
  checkSquare: Square | null
}

const sideToMove = (c: Chess): Side => (c.turn() === 'w' ? 'white' : 'black')

function kingSquare(c: Chess, color: 'w' | 'b'): Square | null {
  for (const row of c.board())
    for (const sq of row)
      if (sq && sq.type === 'k' && sq.color === color) return sq.square as Square
  return null
}

export function useChessGame(botId: BotId, humanSide: Side) {
  const [chess] = useState(() => new Chess())
  const [fen, setFen] = useState(chess.fen())
  const [lastMove, setLastMove] = useState<[Square, Square] | null>(null)
  const [thinking, setThinking] = useState(false)
  const [resigned, setResigned] = useState(false)

  const gen = useRef(0) // bumped on new-game / undo to invalidate an in-flight bot move
  const inFlight = useRef(false)
  const sync = useCallback(() => setFen(chess.fen()), [chess])

  const status = useMemo<GameStatus>(() => {
    const turn = sideToMove(chess)
    const inCheck = chess.inCheck()
    const checkSquare = inCheck ? kingSquare(chess, chess.turn()) : null
    if (resigned)
      return { over: true, result: 'loss', reason: 'You resigned', turn, inCheck, checkSquare }
    if (chess.isCheckmate())
      return {
        over: true,
        result: turn === humanSide ? 'loss' : 'win',
        reason: 'Checkmate',
        turn, inCheck, checkSquare,
      }
    if (chess.isStalemate())
      return { over: true, result: 'draw', reason: 'Stalemate', turn, inCheck, checkSquare }
    if (chess.isThreefoldRepetition())
      return { over: true, result: 'draw', reason: 'Threefold repetition', turn, inCheck, checkSquare }
    if (chess.isInsufficientMaterial())
      return { over: true, result: 'draw', reason: 'Insufficient material', turn, inCheck, checkSquare }
    if (chess.isDraw())
      return { over: true, result: 'draw', reason: 'Draw', turn, inCheck, checkSquare }
    return { over: false, result: null, reason: '', turn, inCheck, checkSquare }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fen, resigned, humanSide])

  // Let the bot move whenever it is its turn.
  useEffect(() => {
    if (resigned || chess.isGameOver()) return
    if (sideToMove(chess) === humanSide) return
    if (inFlight.current) return
    inFlight.current = true
    const myGen = gen.current
    setThinking(true)
    getBotMove(chess.fen(), botId)
      .then(({ move }) => {
        if (myGen !== gen.current) return // a new game/undo happened while thinking — discard
        const from = move.slice(0, 2) as Square
        const to = move.slice(2, 4) as Square
        const promotion = move.length > 4 ? move[4] : undefined
        chess.move({ from, to, promotion })
        setLastMove([from, to])
        sync()
      })
      .catch((e) => console.error(e))
      .finally(() => {
        inFlight.current = false
        setThinking(false)
      })
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fen, resigned])

  const legalTargets = useCallback(
    (from: Square): Square[] =>
      chess.moves({ square: from, verbose: true }).map((m) => m.to as Square),
    [chess],
  )

  const userMove = useCallback(
    (from: Square, to: Square): boolean => {
      if (status.over || thinking) return false
      if (sideToMove(chess) !== humanSide) return false
      const piece = chess.get(from)
      const isPromo = piece?.type === 'p' && (to[1] === '8' || to[1] === '1')
      try {
        chess.move({ from, to, promotion: isPromo ? 'q' : undefined })
      } catch {
        return false
      }
      setLastMove([from, to])
      sync()
      return true
    },
    [chess, humanSide, status.over, thinking, sync],
  )

  const newGame = useCallback(() => {
    gen.current++
    inFlight.current = false
    chess.reset()
    setResigned(false)
    setLastMove(null)
    setThinking(false)
    sync()
  }, [chess, sync])

  const resign = useCallback(() => {
    if (!status.over) setResigned(true)
  }, [status.over])

  const undo = useCallback(() => {
    if (thinking) return
    gen.current++
    setResigned(false)
    chess.undo() // undo the bot's reply
    if (sideToMove(chess) !== humanSide && chess.history().length > 0) chess.undo() // and our move
    const h = chess.history({ verbose: true })
    const last = h[h.length - 1]
    setLastMove(last ? [last.from as Square, last.to as Square] : null)
    sync()
  }, [chess, humanSide, thinking, sync])

  const historySan = useMemo(() => chess.history(), [fen]) // eslint-disable-line react-hooks/exhaustive-deps

  return {
    chess, fen, lastMove, thinking, status, historySan,
    legalTargets, userMove, newGame, resign, undo,
  }
}
