import { useEffect, useRef } from 'react'
import { ChevronFirst, ChevronLast, ChevronLeft, ChevronRight } from 'lucide-react'

export function MoveHistory({
  moves,
  activePly,
  atStart,
  atLive,
  onGotoPly,
  onFirst,
  onPrev,
  onNext,
  onLive,
}: {
  moves: string[]
  activePly: number
  atStart: boolean
  atLive: boolean
  onGotoPly: (ply: number) => void
  onFirst: () => void
  onPrev: () => void
  onNext: () => void
  onLive: () => void
}) {
  const scroller = useRef<HTMLDivElement>(null)
  // Keep the highlighted move in view as you play or step through the game.
  useEffect(() => {
    scroller.current?.querySelector('.ply.current')?.scrollIntoView({ block: 'nearest' })
  }, [activePly, moves.length])

  const pairs: { n: number; white?: string; black?: string; wi: number; bi: number }[] = []
  for (let i = 0; i < moves.length; i += 2) {
    pairs.push({ n: i / 2 + 1, white: moves[i], black: moves[i + 1], wi: i, bi: i + 1 })
  }

  return (
    <div className="panel-sec">
      <div className="moves-head">
        <span className="eyebrow">Move history</span>
        <span className="eyebrow" style={{ color: 'var(--text-3)' }}>
          {moves.length} ply
        </span>
      </div>

      {moves.length === 0 ? (
        <div className="movelist-empty">No moves yet — make the first move.</div>
      ) : (
        <div className="movelist" ref={scroller}>
          {pairs.map((p) => (
            <div className="move-row" key={p.n}>
              <span className="n">{p.n}.</span>
              <button
                className={'ply filled' + (activePly === p.wi ? ' current' : '')}
                onClick={() => onGotoPly(p.wi)}
              >
                {p.white}
              </button>
              {p.black ? (
                <button
                  className={'ply filled' + (activePly === p.bi ? ' current' : '')}
                  onClick={() => onGotoPly(p.bi)}
                >
                  {p.black}
                </button>
              ) : (
                <span className="ply" />
              )}
            </div>
          ))}
        </div>
      )}

      {/* Review controls: jump to start, step back/forward, jump to the live position. These only
          change what the board *shows* — they never undo a move. */}
      <div className="move-nav">
        <button className="nav-btn" onClick={onFirst} disabled={atStart} title="Go to start" aria-label="Go to start">
          <ChevronFirst strokeWidth={2} />
        </button>
        <button className="nav-btn" onClick={onPrev} disabled={atStart} title="Previous move" aria-label="Previous move">
          <ChevronLeft strokeWidth={2} />
        </button>
        <button className="nav-btn" onClick={onNext} disabled={atLive} title="Next move" aria-label="Next move">
          <ChevronRight strokeWidth={2} />
        </button>
        <button className="nav-btn" onClick={onLive} disabled={atLive} title="Go to current position" aria-label="Go to current position">
          <ChevronLast strokeWidth={2} />
        </button>
      </div>
    </div>
  )
}
