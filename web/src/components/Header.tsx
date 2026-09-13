import { Crown } from 'lucide-react'

export type View = 'home' | 'game' | 'analyze'
export type NavTarget = 'play' | 'bots' | 'about'

export function Header({
  view,
  onHome,
  onNavigate,
  onAnalyze,
}: {
  view: View
  onHome: () => void
  onNavigate: (target: NavTarget) => void
  onAnalyze: () => void
}) {
  const nav = (target: NavTarget) => (e: React.MouseEvent) => {
    e.preventDefault()
    onNavigate(target)
  }

  return (
    <header className="header">
      <div className="container header-inner">
        <button
          className="wordmark"
          onClick={onHome}
          style={{ background: 'none', border: 'none', padding: 0, cursor: 'pointer' }}
        >
          <span className="glyph">
            <Crown strokeWidth={1.75} />
          </span>
          PawnHub
        </button>

        <nav className="nav">
          <a
            href="#play"
            className={'nav-extra' + (view === 'home' ? ' active' : '')}
            onClick={nav('play')}
          >
            Play
          </a>
          <a href="#bots" className="nav-extra" onClick={nav('bots')}>
            Bots
          </a>
          <a href="#about" className="nav-extra" onClick={nav('about')}>
            About
          </a>
          <a
            href="#analyze"
            className={'nav-extra' + (view === 'analyze' ? ' active' : '')}
            onClick={(e) => {
              e.preventDefault()
              onAnalyze()
            }}
          >
            Analyse
          </a>
        </nav>
      </div>
    </header>
  )
}
