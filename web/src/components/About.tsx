// An interactive "meet the opponents" section. Pick any of the four bots to see a detailed
// breakdown — persona, approximate strength, and how the same engine is throttled for it.

import { useState } from 'react'
import { ArrowRight, Crown } from 'lucide-react'
import { BOTS, type BotId, botById } from '../lib/bots'
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

export function About({ onPlay }: { onPlay: (id: BotId) => void }) {
  const [active, setActive] = useState<BotId>('veer')
  const bot = botById(active)

  return (
    <div className="about" id="about">
      <div className="section-head" style={{ margin: '0 0 20px' }}>
        <h2>Meet the opponents</h2>
        <span className="hint">One engine, four personalities</span>
      </div>

      <div className="about-grid">
        <div className="about-tabs" role="tablist" aria-label="Choose a bot to read about">
          {BOTS.map((b) => (
            <button
              key={b.id}
              role="tab"
              aria-selected={b.id === active}
              className={'about-tab' + (b.id === active ? ' on' : '')}
              onClick={() => setActive(b.id)}
            >
              <PieceBadge piece={b.piece} size={34} accent={b.id === active} />
              <span className="about-tab-meta">
                <span className="name">{b.name}</span>
                <span className="tier">{b.tier}</span>
              </span>
            </button>
          ))}
        </div>

        <div className="about-detail">
          <div className="about-detail-head">
            <PieceBadge piece={bot.piece} size={56} accent />
            <div className="about-detail-id">
              <div className="about-detail-name">
                {bot.name}
                {bot.id === 'aman' && (
                  <span className="crown" style={{ position: 'static' }} title="Strongest">
                    <Crown strokeWidth={1.75} />
                  </span>
                )}
              </div>
              <div className="about-detail-tier">
                {bot.tier} · {bot.rating}
              </div>
            </div>
          </div>

          <p className="about-persona">{bot.persona}</p>

          <div className="about-meter">
            <span className="eyebrow">Strength</span>
            <StrengthMeter level={bot.strength} />
          </div>

          <div className="about-search">{bot.search}</div>

          <ul className="about-traits">
            {bot.traits.map((t) => (
              <li key={t}>{t}</li>
            ))}
          </ul>

          <button className="btn btn-primary" onClick={() => onPlay(bot.id)}>
            Play {bot.name}
            <ArrowRight strokeWidth={2} />
          </button>
        </div>
      </div>
    </div>
  )
}
