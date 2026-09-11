import { Flag, FlipVertical2, RotateCcw, Undo2 } from 'lucide-react'

export function GameControls({
  onNew,
  onFlip,
  onUndo,
  onResign,
  canUndo,
  canResign,
}: {
  onNew: () => void
  onFlip: () => void
  onUndo: () => void
  onResign: () => void
  canUndo: boolean
  canResign: boolean
}) {
  return (
    <div className="controls">
      <button className="btn" onClick={onNew}>
        <RotateCcw strokeWidth={1.9} /> New game
      </button>
      <button className="btn" onClick={onFlip}>
        <FlipVertical2 strokeWidth={1.9} /> Flip board
      </button>
      <button className="btn" onClick={onUndo} disabled={!canUndo}>
        <Undo2 strokeWidth={1.9} /> Undo
      </button>
      <button className="btn btn-danger" onClick={onResign} disabled={!canResign}>
        <Flag strokeWidth={1.9} /> Resign
      </button>
    </div>
  )
}
