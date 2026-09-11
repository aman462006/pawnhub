import type { Piece } from '../lib/bots'
import { PieceBadge, UserBadge } from './Badge'

export function PlayerInfo({
  name,
  sub,
  piece,
  isBot,
  captured,
  advantage,
}: {
  name: string
  sub: string
  piece: Piece | null
  isBot: boolean
  captured: string[]
  advantage: number
}) {
  return (
    <div className="player-row">
      <div className="player-id">
        {isBot && piece ? <PieceBadge piece={piece} size={34} accent /> : <UserBadge size={34} />}
        <div className="player-meta">
          <div className="name">{name}</div>
          <div className="sub">{sub}</div>
        </div>
      </div>
      <div className="captured">
        {captured.map((g, i) => (
          <span className="cp" key={i}>
            {g}
          </span>
        ))}
        {advantage > 0 && <span className="adv">+{advantage}</span>}
      </div>
    </div>
  )
}
