import { useMemo, useState } from 'react'
import { Chess } from 'chess.js'
import { Chessboard } from 'react-chessboard'
import { Check, ClipboardPaste, Copy, Search } from 'lucide-react'
import { analyzePosition, type Analysis } from '../lib/api'
import { BOTS, type BotId } from '../lib/bots'
import { PieceBadge } from './Badge'
import type { Side } from '../hooks/useChessGame'

type InputMode = 'fen' | 'pgn'

// Turn whatever the user pasted into a FEN. We accept a raw FEN, or a PGN / bare move list (in
// which case we analyse the final position). Returns an error string instead of throwing so the
// UI can show it inline.
function toFen(text: string, mode: InputMode): { fen: string | null; error: string } {
  const raw = text.trim()
  if (!raw) return { fen: null, error: '' }
  if (mode === 'pgn') {
    try {
      const c = new Chess()
      c.loadPgn(raw)
      if (c.history().length === 0) return { fen: null, error: 'No moves found in that PGN.' }
      return { fen: c.fen(), error: '' }
    } catch {
      return { fen: null, error: "Couldn't read that as PGN." }
    }
  }
  try {
    return { fen: new Chess(raw).fen(), error: '' }
  } catch {
    return { fen: null, error: 'That is not a valid FEN.' }
  }
}

function EvalPill({ analysis }: { analysis: Analysis }) {
  const { cp, mate } = analysis.best
  let text: string
  let good: boolean
  if (mate !== null) {
    text = `Mate in ${Math.abs(mate)}`
    good = mate > 0
  } else if (cp !== null) {
    const pawns = cp / 100
    text = (pawns > 0 ? '+' : '') + pawns.toFixed(2)
    good = cp >= 0
  } else {
    text = '—'
    good = true
  }
  // Eval is from White's point of view; note who it favours.
  const favours = mate !== null ? (mate > 0 ? 'White' : 'Black') : cp !== null && cp !== 0 ? (cp > 0 ? 'White' : 'Black') : 'even'
  return (
    <div className={'eval-pill' + (good ? '' : ' neg')}>
      <span className="mono val">{text}</span>
      <span className="sub">{favours === 'even' ? 'level' : `${favours} is better`}</span>
    </div>
  )
}

export function Analyze({ initialFen }: { initialFen?: string }) {
  const [mode, setMode] = useState<InputMode>('fen')
  const [fenText, setFenText] = useState(initialFen ?? '')
  const [pgnText, setPgnText] = useState('')
  const [botId, setBotId] = useState<BotId>('aman')
  const [analysis, setAnalysis] = useState<Analysis | null>(null)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [copied, setCopied] = useState(false)

  const text = mode === 'fen' ? fenText : pgnText
  const setText = mode === 'fen' ? setFenText : setPgnText
  const parsed = useMemo(() => toFen(text, mode), [text, mode])

  // Orient the preview board to the side that is to move in the parsed position.
  const orientation: Side = useMemo(() => {
    if (!parsed.fen) return 'white'
    return parsed.fen.split(' ')[1] === 'b' ? 'black' : 'white'
  }, [parsed.fen])

  const pasteFromClipboard = async () => {
    try {
      const clip = (await navigator.clipboard.readText()).trim()
      if (!clip) return
      // Auto-detect: a PGN either has header tags or move numbers like "1.".
      const looksPgn = /\[|\d+\s*\./.test(clip) && !/^[rnbqkpRNBQKP1-8/]+\s+[wb]\s/.test(clip)
      setMode(looksPgn ? 'pgn' : 'fen')
      if (looksPgn) setPgnText(clip)
      else setFenText(clip)
      setAnalysis(null)
      setError('')
    } catch {
      setError('Clipboard access was blocked — paste into the box instead.')
    }
  }

  const run = async () => {
    if (!parsed.fen) {
      setError(parsed.error || 'Enter a position first.')
      return
    }
    setLoading(true)
    setError('')
    setAnalysis(null)
    try {
      setAnalysis(await analyzePosition(parsed.fen, botId))
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Analysis failed.')
    } finally {
      setLoading(false)
    }
  }

  const copyFen = async () => {
    if (!parsed.fen) return
    try {
      await navigator.clipboard.writeText(parsed.fen)
      setCopied(true)
      setTimeout(() => setCopied(false), 1400)
    } catch {
      /* ignore — clipboard may be unavailable */
    }
  }

  return (
    <div className="grow">
      <section className="analyze">
        <div className="container">
          <div className="section-head">
            <h2>Analyse a position</h2>
            <span className="hint">Paste a FEN or PGN, pick an engine, and see the best move.</span>
          </div>

          <div className="analyze-grid">
            {/* Left: the position preview */}
            <div className="board-col">
              <div className="board-wrap">
                <div className="board-square">
                  {parsed.fen ? (
                    <Chessboard
                      options={{
                        id: 'analyze-board',
                        position: parsed.fen,
                        boardOrientation: orientation,
                        allowDragging: false,
                        lightSquareStyle: { backgroundColor: 'var(--sq-light)' },
                        darkSquareStyle: { backgroundColor: 'var(--sq-dark)' },
                        boardStyle: { borderRadius: '5px', overflow: 'hidden' },
                        showNotation: true,
                      }}
                    />
                  ) : (
                    <div className="board-placeholder">
                      <span>Paste a position to preview it here.</span>
                    </div>
                  )}
                </div>
              </div>
              {parsed.fen && (
                <button className="btn btn-ghost btn-block" onClick={copyFen} style={{ marginTop: 12 }}>
                  {copied ? <Check strokeWidth={1.9} /> : <Copy strokeWidth={1.9} />}
                  {copied ? 'Copied FEN' : 'Copy FEN'}
                </button>
              )}
            </div>

            {/* Right: input + engine + result */}
            <aside className="panel">
              <div className="panel-sec">
                <div className="seg">
                  <button className={'seg-btn' + (mode === 'fen' ? ' on' : '')} onClick={() => setMode('fen')}>
                    FEN
                  </button>
                  <button className={'seg-btn' + (mode === 'pgn' ? ' on' : '')} onClick={() => setMode('pgn')}>
                    PGN / moves
                  </button>
                </div>

                <textarea
                  className="paste-box"
                  rows={mode === 'pgn' ? 5 : 3}
                  spellCheck={false}
                  placeholder={
                    mode === 'fen'
                      ? 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
                      : '1. e4 e5 2. Nf3 Nc6 3. Bb5 …'
                  }
                  value={text}
                  onChange={(e) => {
                    setText(e.target.value)
                    setAnalysis(null)
                    setError('')
                  }}
                />

                <button className="btn btn-block" onClick={pasteFromClipboard} style={{ marginTop: 8 }}>
                  <ClipboardPaste strokeWidth={1.9} /> Paste from clipboard
                </button>

                {parsed.error && <div className="paste-err">{parsed.error}</div>}
              </div>

              <div className="panel-sec">
                <span className="eyebrow">Engine</span>
                <div className="bot-pick">
                  {BOTS.map((b) => (
                    <button
                      key={b.id}
                      className={'bot-pick-item' + (botId === b.id ? ' on' : '')}
                      onClick={() => setBotId(b.id)}
                    >
                      <PieceBadge piece={b.piece} size={26} accent={botId === b.id} />
                      <span className="bp-name">{b.name}</span>
                      <span className="bp-tier">{b.tier}</span>
                    </button>
                  ))}
                </div>
              </div>

              <div className="panel-sec">
                <button className="btn btn-primary btn-block btn-lg" onClick={run} disabled={loading || !parsed.fen}>
                  <Search strokeWidth={1.9} /> {loading ? 'Analysing…' : 'Analyse'}
                </button>

                {error && <div className="paste-err" style={{ marginTop: 12 }}>{error}</div>}

                {analysis && (
                  <div className="result">
                    <div className="result-top">
                      <div>
                        <span className="eyebrow">Best move</span>
                        <div className="best-san mono">{analysis.best.san}</div>
                      </div>
                      <EvalPill analysis={analysis} />
                    </div>
                    <div className="result-line">
                      <span className="eyebrow">Line</span>
                      <div className="mono line-text">{analysis.line.join('  ')}</div>
                    </div>
                  </div>
                )}
              </div>
            </aside>
          </div>
        </div>
      </section>
    </div>
  )
}
