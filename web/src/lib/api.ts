// The single point of contact with the chess engine backend.
//
// Everything the UI knows about "how the bot thinks" lives here. To swap in a different engine
// (or to mock it), you only need to change this one function — the components never call the
// network directly.

import type { BotId } from './bots'

export interface BotMove {
  move: string // UCI, e.g. "e2e4" or "e7e8q"
  san: string  // e.g. "e4", "Qxe8+"
}

export async function getBotMove(fen: string, bot: BotId): Promise<BotMove> {
  const res = await fetch('/api/bot-move', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ fen, bot }),
  })
  if (!res.ok) {
    const detail = await res.text().catch(() => '')
    throw new Error(`Engine request failed (${res.status}) ${detail}`)
  }
  return res.json()
}

export interface BestMove {
  move: string // UCI
  san: string
  cp: number | null // centipawns from White's point of view (null when it's a forced mate)
  mate: number | null // signed mate-in-N from White's point of view (null when it's a cp eval)
}

export interface Analysis {
  fen: string
  turn: 'white' | 'black'
  best: BestMove
  line: string[] // principal variation in SAN, best move first
}

export async function analyzePosition(fen: string, bot: BotId): Promise<Analysis> {
  const res = await fetch('/api/analyze', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ fen, bot }),
  })
  if (!res.ok) {
    // The backend sends a JSON { detail } on 400; fall back to plain text otherwise.
    let detail = ''
    try {
      detail = (await res.json()).detail
    } catch {
      detail = await res.text().catch(() => '')
    }
    throw new Error(detail || `Analysis failed (${res.status})`)
  }
  return res.json()
}
