import { Crown, Minus, RotateCcw, Trophy } from 'lucide-react'
import type { GameStatus } from '../hooks/useChessGame'

export function GameOverModal({
  status,
  botName,
  onPlayAgain,
  onChooseBot,
  onReview,
}: {
  status: GameStatus
  botName: string
  onPlayAgain: () => void
  onChooseBot: () => void
  onReview: () => void
}) {
  const title =
    status.result === 'win' ? 'You won' : status.result === 'loss' ? `${botName} won` : 'Draw'
  const Icon = status.result === 'win' ? Trophy : status.result === 'draw' ? Minus : Crown

  return (
    <div className="scrim" onClick={onReview}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <div className="result-mark">
          <Icon strokeWidth={1.6} />
        </div>
        <h3>{title}</h3>
        <div className="reason">{status.reason}</div>
        <div className="modal-actions">
          <button className="btn btn-primary btn-block" onClick={onPlayAgain}>
            <RotateCcw strokeWidth={1.9} /> Play again
          </button>
          <button className="btn btn-block" onClick={onChooseBot}>
            Choose another bot
          </button>
          <button className="btn btn-ghost btn-block" onClick={onReview}>
            Review game
          </button>
        </div>
      </div>
    </div>
  )
}
