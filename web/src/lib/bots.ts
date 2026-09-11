// The four opponents. Same neural engine, increasing strength (Aman is the strongest).
// `strength` (1–4) drives the little meter in the UI; the real strength lives in the backend.
// Each bot is represented by a chess piece: Rook → Knight → Queen → King (rising power).

export type BotId = 'veer' | 'paramveer' | 'dharamveer' | 'aman'

// One of the chess piece letters we render as the bot's avatar.
export type Piece = 'R' | 'N' | 'Q' | 'K'

export interface Bot {
  id: BotId
  name: string
  tier: string
  piece: Piece
  blurb: string
  strength: 1 | 2 | 3 | 4
  rating: string // approximate playing strength, for flavour
  search: string // how the engine is throttled for this bot
  persona: string // a longer description used in the About deep-dive
  traits: string[] // three short characteristics
}

export const BOTS: Bot[] = [
  {
    id: 'veer',
    name: 'Veer',
    tier: 'Beginner',
    piece: 'R',
    blurb: 'Just learning the ropes. A relaxed first opponent to warm up against.',
    strength: 1,
    rating: '~800',
    search: 'Looks a few moves ahead, and sometimes plays for fun',
    persona:
      'Veer is finding his feet. He sees short tactics but happily wanders into trouble, so he is the perfect sparring partner while you get comfortable with the board.',
    traits: ['Forgives your mistakes', 'Misses deeper tactics', 'Great for a warm-up'],
  },
  {
    id: 'paramveer',
    name: 'Paramveer',
    tier: 'Intermediate',
    piece: 'N',
    blurb: 'Plays solid, sensible chess and punishes loose moves.',
    strength: 2,
    rating: '~1400',
    search: 'Calculates a healthy number of moves ahead',
    persona:
      'Paramveer plays principled, tidy chess. He develops sensibly, keeps his king safe, and will happily take any material you leave hanging. Loose moves get punished.',
    traits: ['Solid and sensible', 'Punishes blunders', 'Rarely gives away material'],
  },
  {
    id: 'dharamveer',
    name: 'Dharamveer',
    tier: 'Advanced',
    piece: 'Q',
    blurb: 'Calculates deeply and defends stubbornly. A real test.',
    strength: 3,
    rating: '~1900',
    search: 'Searches deep and defends stubbornly',
    persona:
      'Dharamveer calculates deeply and clings on in bad positions. He finds resources you did not expect and makes you convert every advantage cleanly. A genuine test.',
    traits: ['Deep calculation', 'Stubborn defence', 'Converts advantages'],
  },
  {
    id: 'aman',
    name: 'Aman',
    tier: 'Strongest',
    piece: 'K',
    blurb: 'The full-strength neural engine. Nothing held back.',
    strength: 4,
    rating: '~2300',
    search: 'Full strength — thinks on the clock, nothing held back',
    persona:
      'Aman is the engine at full power: the trained NNUE evaluation with the complete search, thinking on the clock. This is the same strength that plays ~2300 on Lichess. Bring your best.',
    traits: ['Full-strength NNUE', 'Sharp tactics', 'Punishing endgames'],
  },
]

export const botById = (id: BotId): Bot => BOTS.find((b) => b.id === id) ?? BOTS[0]
