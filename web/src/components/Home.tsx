import { useState } from 'react'
import { ArrowRight } from 'lucide-react'
import { botById, type Bot, type BotId } from '../lib/bots'
import { PieceBadge } from './Badge'
import { BotSelector } from './BotSelector'
import { Roadmap } from './Roadmap'
import { About } from './About'
import { SideSelect, type SideChoice } from './SideSelect'

// The little bar that appears once an opponent is picked: choose a side and start.
function StartBar({
  bot,
  side,
  onSide,
  onStart,
}: {
  bot: Bot
  side: SideChoice
  onSide: (s: SideChoice) => void
  onStart: () => void
}) {
  return (
    <div className="start-bar">
      <div className="start-bar-who">
        <PieceBadge piece={bot.piece} size={44} accent />
        <div>
          <div className="name">Play {bot.name}</div>
          <div className="sub">
            {bot.tier} · {bot.rating}
          </div>
        </div>
      </div>
      <div className="start-bar-side">
        <span className="eyebrow">Your side</span>
        <SideSelect value={side} onChange={onSide} />
      </div>
      <button className="btn btn-primary btn-lg" onClick={onStart}>
        Start game
        <ArrowRight strokeWidth={2} />
      </button>
    </div>
  )
}

export function Home({ onPlay }: { onPlay: (bot: BotId, side: SideChoice) => void }) {
  const [selected, setSelected] = useState<BotId | null>(null)
  const [side, setSide] = useState<SideChoice>('white')

  // Selecting from the About deep-dive brings you up to the picker with that bot chosen.
  const chooseFromAbout = (id: BotId) => {
    setSelected(id)
    document.getElementById('bots')?.scrollIntoView({ behavior: 'smooth', block: 'start' })
  }

  return (
    <div className="grow">
      <section className="home">
        <div className="container">
          <div className="hero" id="play">
            <div className="eyebrow">A chess engine built from scratch</div>
            <h1>A neural chess engine you can actually play.</h1>
            <p>
              PawnHub is one engine — bitboards, a full search, and a neural-network evaluation
              trained from self-play — dressed up as four opponents. Take a look at how it was
              built, then pick someone to play.
            </p>
            <div className="hero-actions">
              <a className="btn btn-primary btn-lg" href="#bots">
                Choose an opponent
                <ArrowRight strokeWidth={2} />
              </a>
              <a className="btn btn-lg" href="#how">
                See how it works
              </a>
            </div>
          </div>

          <Roadmap />

          <div className="section-head" id="bots">
            <h2>Choose your opponent</h2>
            <span className="hint">Aman is the strongest</span>
          </div>
          <BotSelector selected={selected} onSelect={setSelected} />

          {selected && (
            <StartBar
              bot={botById(selected)}
              side={side}
              onSide={setSide}
              onStart={() => onPlay(selected, side)}
            />
          )}

          <div className="about-wrap">
            <About onPlay={chooseFromAbout} />
          </div>
        </div>
      </section>
    </div>
  )
}
