import { useEffect, useState } from 'react'
import { botById, type BotId } from '../lib/bots'
import { analyzeCaptures } from '../lib/material'
import { useChessGame, type Side } from '../hooks/useChessGame'
import { Board } from './Board'
import { GameOverModal } from './GameOverModal'
import { GamePanel } from './GamePanel'
import { PlayerInfo } from './PlayerInfo'

export function Game({
  botId,
  humanSide,
  onExit,
}: {
  botId: BotId
  humanSide: Side
  onExit: () => void
}) {
  const bot = botById(botId)
  const game = useChessGame(botId, humanSide)
  const [orientation, setOrientation] = useState<Side>(humanSide)
  const [modalDismissed, setModalDismissed] = useState(false)

  // Reopen the result modal for each new game.
  useEffect(() => {
    if (!game.status.over) setModalDismissed(false)
  }, [game.status.over])

  const caps = analyzeCaptures(game.chess)
  const interactive = !game.status.over && !game.thinking && game.status.turn === humanSide
  const canUndo = !game.thinking && game.historySan.length > 0

  // Which player sits at the visual top depends on board orientation.
  const topSide: Side = orientation === 'white' ? 'black' : 'white'

  const infoFor = (side: Side) => {
    const isHuman = side === humanSide
    const advantage =
      side === 'white' ? caps.whiteAdvantage : -caps.whiteAdvantage
    return {
      name: isHuman ? 'You' : bot.name,
      sub: isHuman ? (side === 'white' ? 'White' : 'Black') : bot.tier,
      piece: isHuman ? null : bot.piece,
      isBot: !isHuman,
      captured: side === 'white' ? caps.whiteCaptured : caps.blackCaptured,
      advantage: advantage > 0 ? advantage : 0,
    }
  }

  const top = infoFor(topSide)
  const bottom = infoFor(topSide === 'white' ? 'black' : 'white')

  return (
    <div className="grow">
      <section className="game">
        <div className="container game-grid">
          <div className="board-col">
            <div className="board-players">
              <PlayerInfo {...top} />
            </div>
            <div className="board-wrap">
              <Board
                fen={game.fen}
                orientation={orientation}
                lastMove={game.lastMove}
                checkSquare={game.status.checkSquare}
                interactive={interactive}
                legalTargets={game.legalTargets}
                onMove={game.userMove}
              />
            </div>
            <div className="board-players">
              <PlayerInfo {...bottom} />
            </div>
          </div>

          <GamePanel
            bot={bot}
            humanSide={humanSide}
            status={game.status}
            thinking={game.thinking}
            moves={game.historySan}
            onNew={game.newGame}
            onFlip={() => setOrientation((o) => (o === 'white' ? 'black' : 'white'))}
            onUndo={game.undo}
            onResign={game.resign}
            canUndo={canUndo}
          />
        </div>
      </section>

      {game.status.over && !modalDismissed && (
        <GameOverModal
          status={game.status}
          botName={bot.name}
          onPlayAgain={game.newGame}
          onChooseBot={onExit}
          onReview={() => setModalDismissed(true)}
        />
      )}
    </div>
  )
}
