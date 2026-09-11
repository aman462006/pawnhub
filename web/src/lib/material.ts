import type { Chess } from 'chess.js'

const VALUE: Record<string, number> = { p: 1, n: 3, b: 3, r: 5, q: 9 }
const START: Record<string, number> = { p: 8, n: 2, b: 2, r: 2, q: 1 }
const ORDER = ['q', 'r', 'b', 'n', 'p'] as const

// Solid glyphs for black pieces, outline glyphs for white — so captured pieces read clearly.
const GLYPH: Record<'w' | 'b', Record<string, string>> = {
  w: { p: '♙', n: '♘', b: '♗', r: '♖', q: '♕' },
  b: { p: '♟', n: '♞', b: '♝', r: '♜', q: '♛' },
}

export interface Captures {
  whiteCaptured: string[] // black pieces White has taken
  blackCaptured: string[] // white pieces Black has taken
  whiteAdvantage: number  // material points White is up (negative = Black is up)
}

export function analyzeCaptures(chess: Chess): Captures {
  const on = {
    w: { p: 0, n: 0, b: 0, r: 0, q: 0 } as Record<string, number>,
    b: { p: 0, n: 0, b: 0, r: 0, q: 0 } as Record<string, number>,
  }
  for (const row of chess.board())
    for (const sq of row)
      if (sq && sq.type !== 'k') on[sq.color][sq.type as string]++

  const whiteCaptured: string[] = []
  const blackCaptured: string[] = []
  let whiteVal = 0
  let blackVal = 0
  for (const t of ORDER) {
    for (let i = 0; i < Math.max(0, START[t] - on.b[t]); i++) whiteCaptured.push(GLYPH.b[t])
    for (let i = 0; i < Math.max(0, START[t] - on.w[t]); i++) blackCaptured.push(GLYPH.w[t])
    whiteVal += on.w[t] * VALUE[t]
    blackVal += on.b[t] * VALUE[t]
  }
  return { whiteCaptured, blackCaptured, whiteAdvantage: whiteVal - blackVal }
}
