import { useEffect, useRef } from 'react'

export function MoveHistory({ moves }: { moves: string[] }) {
  const scroller = useRef<HTMLDivElement>(null)
  useEffect(() => {
    if (scroller.current) scroller.current.scrollTop = scroller.current.scrollHeight
  }, [moves.length])

  const pairs: { n: number; white?: string; black?: string; wi: number; bi: number }[] = []
  for (let i = 0; i < moves.length; i += 2) {
    pairs.push({ n: i / 2 + 1, white: moves[i], black: moves[i + 1], wi: i, bi: i + 1 })
  }
  const current = moves.length - 1

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
              <span className={'ply filled' + (current === p.wi ? ' current' : '')}>{p.white}</span>
              <span className={'ply' + (p.black ? ' filled' : '') + (current === p.bi ? ' current' : '')}>
                {p.black ?? ''}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  )
}
