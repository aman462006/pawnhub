#include "movegen.h"
#include "magic.h"
#include <algorithm>

namespace {

// Castling-rights update masks. For each square, the bits of castling right to KEEP if a
// piece moves from OR to that square. A king/rook leaving its home square, or a rook being
// captured on its home square, all correctly clear the right via `rights &= mask[from] & mask[to]`.
int CastleMask[64];

inline void put_piece(Position& pos, Color c, PieceType pt, Square s) { set_bit(pos.pieces[c][pt], s); }
inline void remove_piece(Position& pos, Color c, PieceType pt, Square s) { clear_bit(pos.pieces[c][pt], s); }

void add_promotions(MoveList& list, Square from, Square to) {
    list.add(make_move(from, to, MF_PROMO, QUEEN));
    list.add(make_move(from, to, MF_PROMO, ROOK));
    list.add(make_move(from, to, MF_PROMO, BISHOP));
    list.add(make_move(from, to, MF_PROMO, KNIGHT));
}

// Rook's origin/destination for a castling king move to `to`.
void castle_rook(Square to, Square& rfrom, Square& rto) {
    if (to == G1)      { rfrom = H1; rto = F1; }
    else if (to == C1) { rfrom = A1; rto = D1; }
    else if (to == G8) { rfrom = H8; rto = F8; }
    else               { rfrom = A8; rto = D8; }  // C8
}

}  // namespace

void init_movegen() {
    for (int i = 0; i < 64; ++i) CastleMask[i] = 15;   // 15 = all four rights kept
    CastleMask[E1] &= ~(WHITE_OO | WHITE_OOO);
    CastleMask[H1] &= ~WHITE_OO;
    CastleMask[A1] &= ~WHITE_OOO;
    CastleMask[E8] &= ~(BLACK_OO | BLACK_OOO);
    CastleMask[H8] &= ~BLACK_OO;
    CastleMask[A8] &= ~BLACK_OOO;
}

void generate_pseudo_legal(const Position& pos, MoveList& list) {
    Color us = pos.side_to_move, them = ~us;
    Bitboard usBB = pos.occupied[us];
    Bitboard themBB = pos.occupied[them];
    Bitboard all = pos.all;
    Bitboard empty = ~all;

    // --- Pawns (the piece with all the special cases) ---
    int forward   = (us == WHITE) ? 8 : -8;
    int startRank = (us == WHITE) ? 1 : 6;
    int promoRank = (us == WHITE) ? 7 : 0;
    Bitboard pawns = pos.pieces[us][PAWN];
    while (pawns) {
        Square s = pop_lsb(pawns);
        Square one = Square(s + forward);
        if (empty & square_bb(one)) {                 // single push
            if (rank_of(one) == promoRank) add_promotions(list, s, one);
            else {
                list.add(make_move(s, one));
                if (rank_of(s) == startRank) {         // double push (only from home rank)
                    Square two = Square(s + 2 * forward);
                    if (empty & square_bb(two)) list.add(make_move(s, two));
                }
            }
        }
        Bitboard caps = PawnAttacks[us][s] & themBB;   // diagonal captures
        while (caps) {
            Square t = pop_lsb(caps);
            if (rank_of(t) == promoRank) add_promotions(list, s, t);
            else list.add(make_move(s, t));
        }
        if (pos.ep_square != SQ_NONE && (PawnAttacks[us][s] & square_bb(pos.ep_square)))
            list.add(make_move(s, pos.ep_square, MF_ENPASSANT));
    }

    // --- Knights ---
    Bitboard kn = pos.pieces[us][KNIGHT];
    while (kn) { Square s = pop_lsb(kn); Bitboard t = KnightAttacks[s] & ~usBB;
                 while (t) list.add(make_move(s, pop_lsb(t))); }

    // --- Bishops / Rooks / Queens (sliders via magic lookups) ---
    Bitboard bi = pos.pieces[us][BISHOP];
    while (bi) { Square s = pop_lsb(bi); Bitboard t = bishop_attacks(s, all) & ~usBB;
                 while (t) list.add(make_move(s, pop_lsb(t))); }
    Bitboard ro = pos.pieces[us][ROOK];
    while (ro) { Square s = pop_lsb(ro); Bitboard t = rook_attacks(s, all) & ~usBB;
                 while (t) list.add(make_move(s, pop_lsb(t))); }
    Bitboard qu = pos.pieces[us][QUEEN];
    while (qu) { Square s = pop_lsb(qu); Bitboard t = queen_attacks(s, all) & ~usBB;
                 while (t) list.add(make_move(s, pop_lsb(t))); }

    // --- King (plus castling) ---
    Square ksq = pos.king_square(us);
    Bitboard kt = KingAttacks[ksq] & ~usBB;
    while (kt) list.add(make_move(ksq, pop_lsb(kt)));

    if (us == WHITE) {
        if ((pos.castling_rights & WHITE_OO) && (empty & square_bb(F1)) && (empty & square_bb(G1))
            && !pos.is_attacked(E1, them) && !pos.is_attacked(F1, them) && !pos.is_attacked(G1, them))
            list.add(make_move(E1, G1, MF_CASTLE));
        if ((pos.castling_rights & WHITE_OOO) && (empty & square_bb(D1)) && (empty & square_bb(C1)) && (empty & square_bb(B1))
            && !pos.is_attacked(E1, them) && !pos.is_attacked(D1, them) && !pos.is_attacked(C1, them))
            list.add(make_move(E1, C1, MF_CASTLE));
    } else {
        if ((pos.castling_rights & BLACK_OO) && (empty & square_bb(F8)) && (empty & square_bb(G8))
            && !pos.is_attacked(E8, them) && !pos.is_attacked(F8, them) && !pos.is_attacked(G8, them))
            list.add(make_move(E8, G8, MF_CASTLE));
        if ((pos.castling_rights & BLACK_OOO) && (empty & square_bb(D8)) && (empty & square_bb(C8)) && (empty & square_bb(B8))
            && !pos.is_attacked(E8, them) && !pos.is_attacked(D8, them) && !pos.is_attacked(C8, them))
            list.add(make_move(E8, C8, MF_CASTLE));
    }
}

void do_move(Position& pos, Move m, Undo& u) {
    Color us = pos.side_to_move, them = ~us;
    Square from = move_from(m), to = move_to(m);
    int flag = move_flag(m);
    PieceType pt = pos.piece_type_on(from);

    // Snapshot everything a move can change.
    u.castling_rights = pos.castling_rights;
    u.ep_square       = pos.ep_square;
    u.halfmove_clock  = pos.halfmove_clock;
    u.fullmove_number = pos.fullmove_number;
    u.captured        = PIECE_TYPE_NB;

    Square new_ep = SQ_NONE;
    pos.halfmove_clock++;
    remove_piece(pos, us, pt, from);

    if (flag == MF_ENPASSANT) {
        // The captured pawn sits beside the destination, on the moving pawn's rank.
        Square capsq = make_square(file_of(to), rank_of(from));
        remove_piece(pos, them, PAWN, capsq);
        u.captured = PAWN;
        pos.halfmove_clock = 0;
    } else {
        PieceType cap = pos.piece_type_on(to);
        if (cap != PIECE_TYPE_NB) {
            remove_piece(pos, them, cap, to);
            u.captured = cap;
            pos.halfmove_clock = 0;
        }
    }

    if (flag == MF_PROMO) put_piece(pos, us, move_promo(m), to);
    else                  put_piece(pos, us, pt, to);

    if (pt == PAWN) {
        pos.halfmove_clock = 0;
        int dr = rank_of(to) - rank_of(from);
        if (dr == 2 || dr == -2)     // double push: remember the en-passant target square
            new_ep = make_square(file_of(from), (rank_of(from) + rank_of(to)) / 2);
    }

    if (flag == MF_CASTLE) {
        Square rfrom, rto; castle_rook(to, rfrom, rto);
        remove_piece(pos, us, ROOK, rfrom);
        put_piece(pos, us, ROOK, rto);
    }

    pos.castling_rights &= CastleMask[from] & CastleMask[to];
    pos.ep_square = new_ep;
    if (us == BLACK) pos.fullmove_number++;
    pos.side_to_move = them;
    pos.refresh_aggregates();
}

void undo_move(Position& pos, Move m, const Undo& u) {
    pos.side_to_move = ~pos.side_to_move;      // back to the side that moved
    Color us = pos.side_to_move, them = ~us;
    Square from = move_from(m), to = move_to(m);
    int flag = move_flag(m);

    if (flag == MF_PROMO) {
        remove_piece(pos, us, move_promo(m), to);
        put_piece(pos, us, PAWN, from);
    } else {
        PieceType pt = pos.piece_type_on(to);
        remove_piece(pos, us, pt, to);
        put_piece(pos, us, pt, from);
    }

    if (flag == MF_ENPASSANT) {
        Square capsq = make_square(file_of(to), rank_of(from));
        put_piece(pos, them, PAWN, capsq);
    } else if (u.captured != PIECE_TYPE_NB) {
        put_piece(pos, them, u.captured, to);
    }

    if (flag == MF_CASTLE) {
        Square rfrom, rto; castle_rook(to, rfrom, rto);
        remove_piece(pos, us, ROOK, rto);
        put_piece(pos, us, ROOK, rfrom);
    }

    pos.castling_rights = u.castling_rights;
    pos.ep_square       = u.ep_square;
    pos.halfmove_clock  = u.halfmove_clock;
    pos.fullmove_number = u.fullmove_number;
    pos.refresh_aggregates();
}

void generate_legal(Position& pos, MoveList& list) {
    MoveList pseudo;
    generate_pseudo_legal(pos, pseudo);
    Color us = pos.side_to_move;
    for (int i = 0; i < pseudo.size; ++i) {
        Move m = pseudo.moves[i];
        Undo u;
        do_move(pos, m, u);
        // After do_move, side_to_move is the opponent. Keep the move only if OUR king isn't
        // sitting in check — that's the definition of a legal move.
        if (!pos.is_attacked(pos.king_square(us), pos.side_to_move))
            list.add(m);
        undo_move(pos, m, u);
    }
}

Bitboard attackers_to(const Position& pos, Square s, Bitboard occ) {
    Bitboard b = 0;
    b |= PawnAttacks[BLACK][s] & pos.pieces[WHITE][PAWN];   // white pawns attacking s
    b |= PawnAttacks[WHITE][s] & pos.pieces[BLACK][PAWN];   // black pawns attacking s
    b |= KnightAttacks[s] & (pos.pieces[WHITE][KNIGHT] | pos.pieces[BLACK][KNIGHT]);
    b |= KingAttacks[s]   & (pos.pieces[WHITE][KING]   | pos.pieces[BLACK][KING]);
    Bitboard bq = pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP]
                | pos.pieces[WHITE][QUEEN]  | pos.pieces[BLACK][QUEEN];
    Bitboard rq = pos.pieces[WHITE][ROOK]   | pos.pieces[BLACK][ROOK]
                | pos.pieces[WHITE][QUEEN]  | pos.pieces[BLACK][QUEEN];
    b |= bishop_attacks(s, occ) & bq;
    b |= rook_attacks(s, occ) & rq;
    return b;
}

int see(const Position& pos, Move m) {
    // Piece values for the exchange (king huge: a king can only take an undefended piece).
    static const int V[7] = { 100, 320, 330, 500, 900, 100000, 0 };
    Square to = move_to(m), from = move_from(m);
    int flag = move_flag(m);
    PieceType target = (flag == MF_ENPASSANT) ? PAWN : pos.piece_type_on(to);

    int gain[32];
    int d = 0;
    gain[0] = (target == PIECE_TYPE_NB) ? 0 : V[target];   // material grabbed by the first capture
    PieceType attacker_pt = pos.piece_type_on(from);

    Bitboard occ = pos.all ^ square_bb(from);              // the first attacker has moved off
    if (flag == MF_ENPASSANT) occ ^= square_bb(make_square(file_of(to), rank_of(from)));
    Bitboard attackers = attackers_to(pos, to, occ);
    Color side = ~pos.side_to_move;                        // opponent recaptures next

    while (true) {
        Bitboard side_att = attackers & pos.occupied[side] & occ;
        if (!side_att) break;
        // Recapture with the least valuable attacker.
        PieceType pt = PAWN;
        for (; pt <= KING; pt = PieceType(pt + 1))
            if (side_att & pos.pieces[side][pt]) break;
        if (++d >= 31) break;
        gain[d] = V[attacker_pt] - gain[d - 1];            // speculative: assume it gets recaptured
        Bitboard piece = side_att & pos.pieces[side][pt];
        occ ^= (piece & (~piece + 1));                     // remove that one piece (lowest bit)
        attacker_pt = pt;
        attackers = attackers_to(pos, to, occ);            // recompute to reveal X-ray attackers
        side = ~side;
    }
    // Minimax the gains back: each side stops capturing once continuing would lose material.
    for (int i = d; i > 0; --i)
        gain[i - 1] = -std::max(-gain[i - 1], gain[i]);
    return gain[0];
}

std::uint64_t perft(Position& pos, int depth) {
    MoveList list;
    generate_legal(pos, list);
    if (depth <= 1) return list.size;      // at depth 1, the answer is just the legal-move count
    std::uint64_t nodes = 0;
    for (int i = 0; i < list.size; ++i) {
        Undo u;
        do_move(pos, list.moves[i], u);
        nodes += perft(pos, depth - 1);
        undo_move(pos, list.moves[i], u);
    }
    return nodes;
}
