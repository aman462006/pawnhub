import { Crown } from 'lucide-react'
import { BOTS, type Bot, type BotId } from '../lib/bots'
import { PieceBadge } from './Badge'

function StrengthMeter({ level }: { level: number }) {
  return (
    <div className="strength" aria-label={`Strength ${level} of 4`}>
      {[1, 2, 3, 4].map((i) => (
        <i key={i} className={i <= level ? 'on' : ''} />
      ))}
    </div>
  )
}

function BotCard({
  bot,
  selected,
  onSelect,
}: {
  bot: Bot
  selected: boolean
  onSelect: () => void
}) {
  return (
    <button
      className={'bot-card' + (selected ? ' selected' : '')}
      onClick={onSelect}
      aria-pressed={selected}
    >
      {bot.id === 'aman' && (
        <span className="crown" title="Strongest">
          <Crown strokeWidth={1.75} />
        </span>
      )}
      <div style={{ marginBottom: 16 }}>
        <PieceBadge piece={bot.piece} size={42} accent={selected} />
      </div>
      <div className="bot-name">{bot.name}</div>
      <div className="bot-tier">{bot.tier}</div>
      <StrengthMeter level={bot.strength} />
    </button>
  )
}

export function BotSelector({
  selected,
  onSelect,
}: {
  selected: BotId | null
  onSelect: (id: BotId) => void
}) {
  return (
    <div className="bot-grid">
      {BOTS.map((bot) => (
        <BotCard
          key={bot.id}
          bot={bot}
          selected={bot.id === selected}
          onSelect={() => onSelect(bot.id)}
        />
      ))}
    </div>
  )
}
