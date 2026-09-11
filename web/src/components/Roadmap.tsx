// A short, pictorial "how it was built" story for the home page. Five steps, left to right,
// each a single icon + a couple of words. Restrained, not wordy.

import { Brain, Grid3x3, Network, Swords, Waypoints } from 'lucide-react'
import type { ComponentType } from 'react'

interface Step {
  icon: ComponentType<{ strokeWidth?: number }>
  title: string
  note: string
}

const STEPS: Step[] = [
  { icon: Grid3x3, title: 'Bitboards', note: 'The board as 64-bit integers' },
  { icon: Waypoints, title: 'Move generation', note: 'Magic bitboards, perft-verified' },
  { icon: Network, title: 'Search', note: 'Alpha-beta with modern pruning' },
  { icon: Brain, title: 'Neural eval', note: 'A trained NNUE, from self-play' },
  { icon: Swords, title: 'Play', note: '~2300 on Lichess' },
]

export function Roadmap() {
  return (
    <div className="roadmap" id="how">
      {STEPS.map((s, i) => (
        <div className="road-step" key={s.title}>
          <div className="road-node">
            <span className="road-icon">
              <s.icon strokeWidth={1.6} />
            </span>
            {i < STEPS.length - 1 && <span className="road-line" aria-hidden />}
          </div>
          <div className="road-title">{s.title}</div>
          <div className="road-note">{s.note}</div>
        </div>
      ))}
    </div>
  )
}
