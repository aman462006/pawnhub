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

  // Review navigation: which ply's position is shown on the board. `null` means "live" — follow
  // the real game. Any number 0..N is a *read-only* snapshot (N = number of plies played); it never
  // mutates the actual game, so this is purely a viewer and is completely separate from undo.
  const [viewPos, setViewPos] = useState<number | null>(null)

  const gen = useRef(0) // bumped on new-game / undo to invalidate an in-flight bot move
  const inFlight = useRef(false)
  // Any real change to the game (a move, undo, new game) snaps the board back to the live position.
  const sync = useCallback(() => {
    setViewPos(null)
    setFen(chess.fen())
  }, [chess])

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

  // ---- Review navigation ---------------------------------------------------------------------
  // The verbose move list and a snapshot FEN after every ply, both recomputed only when a real
  // move lands (fen changes). Index 0 is the starting position; index k is after k plies.
  const verbose = useMemo(() => chess.history({ verbose: true }), [fen]) // eslint-disable-line react-hooks/exhaustive-deps
  const fenHistory = useMemo(() => {
    const replay = new Chess()
    const fens = [replay.fen()]
    for (const san of historySan) {
      replay.move(san)
      fens.push(replay.fen())
    }
    return fens
  }, [historySan])

  const plies = verbose.length
  const reviewing = viewPos !== null
  const atStart = (viewPos ?? plies) === 0
  const atLive = viewPos === null

  // What the board should actually render — live position, or the reviewed snapshot.
  const boardFen = viewPos === null ? fen : fenHistory[viewPos]
  const boardLastMove: [Square, Square] | null =
    viewPos === null
      ? lastMove
      : viewPos > 0
        ? [verbose[viewPos - 1].from as Square, verbose[viewPos - 1].to as Square]
        : null
  const boardCheckSquare = useMemo<Square | null>(() => {
    if (viewPos === null) return status.checkSquare
    const c = new Chess(fenHistory[viewPos])
    return c.inCheck() ? kingSquare(c, c.turn()) : null
  }, [viewPos, fenHistory, status.checkSquare])

  // Which ply (0-based index into historySan) is highlighted in the move list.
  const activePly = viewPos === null ? plies - 1 : viewPos - 1

  const goStart = useCallback(() => setViewPos(plies === 0 ? null : 0), [plies])
  const goPrev = useCallback(
    () => setViewPos((p) => Math.max(0, (p ?? plies) - 1)),
    [plies],
  )
  const goNext = useCallback(
    () => setViewPos((p) => {
      const next = (p ?? plies) + 1
      return next >= plies ? null : next
    }),
    [plies],
  )
  const goLive = useCallback(() => setViewPos(null), [])
  // Clicking a move in the list jumps to the position *after* that move.
  const gotoPly = useCallback(
    (ply: number) => setViewPos(ply + 1 >= plies ? null : ply + 1),
    [plies],
  )

  return {
    chess, fen, lastMove, thinking, status, historySan,
    legalTargets, userMove, newGame, resign, undo,
    // review
    reviewing, atStart, atLive, boardFen, boardLastMove, boardCheckSquare, activePly,
    goStart, goPrev, goNext, goLive, gotoPly,
  }
}
