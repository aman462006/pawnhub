// Lets the player pick which colour to play. "Random" is resolved to white/black at game start.
// If the player takes Black, the bot moves first.

import { Shuffle } from 'lucide-react'

export type SideChoice = 'white' | 'black' | 'random'

const OPTIONS: { id: SideChoice; label: string }[] = [
  { id: 'white', label: 'White' },
  { id: 'black', label: 'Black' },
  { id: 'random', label: 'Random' },
]

export function SideSelect({
  value,
  onChange,
}: {
  value: SideChoice
  onChange: (s: SideChoice) => void
}) {
  return (
    <div className="side-select" role="radiogroup" aria-label="Choose your side">
      {OPTIONS.map((o) => (
        <button
          key={o.id}
          role="radio"
          aria-checked={value === o.id}
          className={'side-opt' + (value === o.id ? ' on' : '')}
          onClick={() => onChange(o.id)}
        >
          <span className={'side-chip side-' + o.id}>
            {o.id === 'random' ? <Shuffle strokeWidth={2} /> : null}
          </span>
          {o.label}
        </button>
      ))}
    </div>
  )
}
