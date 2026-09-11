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
