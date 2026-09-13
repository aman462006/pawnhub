import { useState } from 'react'
import { Analyze } from './components/Analyze'
import { Game } from './components/Game'
import { Header, type NavTarget, type View } from './components/Header'
import { Home } from './components/Home'
import type { SideChoice } from './components/SideSelect'
import type { Side } from './hooks/useChessGame'
import { botById, type BotId } from './lib/bots'

interface Setup {
  bot: BotId
  side: Side
}

export default function App() {
  const [view, setView] = useState<View>('home')
  const [setup, setSetup] = useState<Setup | null>(null)

  const start = (bot: BotId, choice: SideChoice) => {
    const side: Side = choice === 'random' ? (Math.random() < 0.5 ? 'white' : 'black') : choice
    setSetup({ bot, side })
    setView('game')
  }

  const [analyzeSeed, setAnalyzeSeed] = useState<string | null>(null)

  const goHome = () => setView('home')
  const goAnalyze = (fen?: string) => {
    setAnalyzeSeed(fen ?? null)
    setView('analyze')
    window.scrollTo({ top: 0 })
  }

  const navigate = (target: NavTarget) => {
    const scroll = () =>
      document.getElementById(target)?.scrollIntoView({ behavior: 'smooth', block: 'start' })
    if (view !== 'home') {
      setView('home')
      // let Home mount before scrolling to its sections
      setTimeout(scroll, 60)
    } else {
      scroll()
    }
  }

  return (
    <div className="app">
      <Header view={view} onHome={goHome} onNavigate={navigate} onAnalyze={() => goAnalyze()} />

      {view === 'game' && setup ? (
        <Game
          key={`${setup.bot}-${setup.side}`}
          botId={setup.bot}
          humanSide={setup.side}
          onExit={goHome}
          onAnalyzePosition={goAnalyze}
        />
      ) : view === 'analyze' ? (
        <Analyze key={analyzeSeed ?? 'blank'} initialFen={analyzeSeed ?? undefined} />
      ) : (
        <Home onPlay={start} />
      )}

      <footer className="footer">
        <div className="container">
          <span>PawnHub — a neural chess engine, built from scratch</span>
          <span className="mono" style={{ fontSize: 12 }}>
            {view === 'game' && setup ? `You vs ${botById(setup.bot).name}` : 'Play the engine'}
          </span>
        </div>
      </footer>
    </div>
  )
}
