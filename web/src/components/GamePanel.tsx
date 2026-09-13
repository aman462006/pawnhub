import { Crown, Search } from 'lucide-react'
import type { Bot } from '../lib/bots'
import type { GameStatus, Side } from '../hooks/useChessGame'
import { GameControls } from './GameControls'
import { PieceBadge } from './Badge'
import { MoveHistory } from './MoveHistory'

function StatusLine({
  status,
  thinking,
  botName,
  humanSide,
}: {
  status: GameStatus
  thinking: boolean
  botName: string
  humanSide: Side
}) {
  if (thinking) {
    return (
      <div className="thinking">
        <span className="eyebrow" style={{ color: 'var(--accent-ink)' }}>
          {botName} is thinking
        </span>
        <span className="dots">
          <i />
          <i />
          <i />
        </span>
      </div>
    )
  }
  if (status.over) {
    const txt =
      status.result === 'win' ? 'You won' : status.result === 'loss' ? `${botName} won` : 'Draw'
    return (
      <div className="status-line">
        <span className="status-dot idle" />
        <span>
          {txt} · {status.reason}
        </span>
      </div>
    )
  }
  const yourTurn = status.turn === humanSide
  if (status.inCheck) {
    return (
      <div className="status-line">
        <span className="status-dot warn" />
        <span>{yourTurn ? 'You are in check' : `${botName} is in check`}</span>
      </div>
    )
  }
  return (
    <div className="status-line">
      <span className={'status-dot' + (yourTurn ? '' : ' idle')} />
      <span>{yourTurn ? 'Your move' : `${botName} to move`}</span>
    </div>
  )
}

export function GamePanel({
  bot,
  humanSide,
  status,
  thinking,
  moves,
  onNew,
  onFlip,
  onUndo,
  onResign,
  canUndo,
  activePly,
  atStart,
  atLive,
  onGotoPly,
  onFirst,
  onPrev,
  onNext,
  onLive,
  onAnalyze,
}: {
  bot: Bot
  humanSide: Side
  status: GameStatus
  thinking: boolean
  moves: string[]
  onNew: () => void
  onFlip: () => void
  onUndo: () => void
  onResign: () => void
  canUndo: boolean
  activePly: number
  atStart: boolean
  atLive: boolean
  onGotoPly: (ply: number) => void
  onFirst: () => void
  onPrev: () => void
  onNext: () => void
  onLive: () => void
  onAnalyze: () => void
}) {
  return (
    <aside className="panel">
      <div className="panel-sec">
        <div className="opponent">
          <PieceBadge piece={bot.piece} size={40} accent />
          <div className="who">
            <div className="name">{bot.name}</div>
            <div className="tier">{bot.tier} · Opponent</div>
          </div>
          {bot.id === 'aman' && (
            <span className="crown" style={{ position: 'static' }} title="Strongest">
              <Crown strokeWidth={1.75} />
            </span>
          )}
        </div>
      </div>

      <div className="panel-sec">
        <StatusLine status={status} thinking={thinking} botName={bot.name} humanSide={humanSide} />
      </div>

      <MoveHistory
        moves={moves}
        activePly={activePly}
        atStart={atStart}
        atLive={atLive}
        onGotoPly={onGotoPly}
        onFirst={onFirst}
        onPrev={onPrev}
        onNext={onNext}
        onLive={onLive}
      />

      <div className="panel-sec">
        <GameControls
          onNew={onNew}
          onFlip={onFlip}
          onUndo={onUndo}
          onResign={onResign}
          canUndo={canUndo}
          canResign={!status.over && !thinking}
        />
        {/* Send the position currently on the board (live or a reviewed one) to the Analyse view. */}
        <button className="btn btn-block" onClick={onAnalyze} style={{ marginTop: 8 }}>
          <Search strokeWidth={1.9} /> Analyse position
        </button>
      </div>
    </aside>
  )
}
