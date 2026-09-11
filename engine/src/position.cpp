#include "position.h"
#include "magic.h"
#include <sstream>
#include <cctype>

namespace {
// Map a FEN piece letter to (color, type). Uppercase = White, lowercase = Black.
bool decode_piece(char c, Color& col, PieceType& pt) {
    col = std::isupper((unsigned char)c) ? WHITE : BLACK;
    switch (std::tolower((unsigned char)c)) {
        case 'p': pt = PAWN;   return true;
        case 'n': pt = KNIGHT; return true;
        case 'b': pt = BISHOP; return true;
        case 'r': pt = ROOK;   return true;
        case 'q': pt = QUEEN;  return true;
        case 'k': pt = KING;   return true;
        default:  return false;
    }
}
char encode_piece(Color col, PieceType pt) {
    const char* letters = "pnbrqk";
    char c = letters[pt];
    return col == WHITE ? char(std::toupper((unsigned char)c)) : c;
}
}  // namespace

void Position::refresh_aggregates() {
    for (int c = 0; c < COLOR_NB; ++c) {
        occupied[c] = 0;
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt)
            occupied[c] |= pieces[c][pt];
    }
    all = occupied[WHITE] | occupied[BLACK];
}

bool Position::is_attacked(Square s, Color by) const {
    // A square is attacked by a pawn of `by` if a `by` pawn stands where a pawn of the
    // OPPOSITE color, sitting on `s`, would capture — a neat symmetry that reuses the table.
    if (PawnAttacks[~by][s] & pieces[by][PAWN]) return true;
    if (KnightAttacks[s] & pieces[by][KNIGHT]) return true;
    if (KingAttacks[s] & pieces[by][KING]) return true;
    if (bishop_attacks(s, all) & (pieces[by][BISHOP] | pieces[by][QUEEN])) return true;
    if (rook_attacks(s, all) & (pieces[by][ROOK] | pieces[by][QUEEN])) return true;
    return false;
}

PieceType Position::piece_type_on(Square s) const {
    Bitboard bb = square_bb(s);
    for (int pt = 0; pt < PIECE_TYPE_NB; ++pt)
        if ((pieces[WHITE][pt] | pieces[BLACK][pt]) & bb)
            return PieceType(pt);
    return PIECE_TYPE_NB;  // empty
}

bool Position::set_fen(const std::string& fen) {
    *this = Position{};  // reset to empty

    std::istringstream ss(fen);
    std::string placement, stm, castling, ep;
    if (!(ss >> placement >> stm >> castling >> ep)) return false;
    ss >> halfmove_clock >> fullmove_number;  // optional; default to 0/1 if missing

    // Piece placement is given rank 8 first, file a first within a rank.
    int rank = 7, file = 0;
    for (char c : placement) {
        if (c == '/') { rank--; file = 0; }
        else if (std::isdigit((unsigned char)c)) { file += c - '0'; }
        else {
            Color col; PieceType pt;
            if (!decode_piece(c, col, pt)) return false;
            set_bit(pieces[col][pt], make_square(file, rank));
            file++;
        }
    }

    side_to_move = (stm == "w") ? WHITE : BLACK;

    castling_rights = NO_CASTLING;
    for (char c : castling) {
        if (c == 'K') castling_rights |= WHITE_OO;
        else if (c == 'Q') castling_rights |= WHITE_OOO;
        else if (c == 'k') castling_rights |= BLACK_OO;
        else if (c == 'q') castling_rights |= BLACK_OOO;
    }

    ep_square = string_to_square(ep);
    refresh_aggregates();
    return true;
}

std::string Position::to_fen() const {
    std::string out;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Square s = make_square(file, rank);
            PieceType pt = piece_type_on(s);
            if (pt == PIECE_TYPE_NB) { empty++; continue; }
            if (empty) { out += char('0' + empty); empty = 0; }
            Color col = (occupied[WHITE] & square_bb(s)) ? WHITE : BLACK;
            out += encode_piece(col, pt);
        }
        if (empty) out += char('0' + empty);
        if (rank) out += '/';
    }
    out += side_to_move == WHITE ? " w " : " b ";

    std::string c;
    if (castling_rights & WHITE_OO)  c += 'K';
    if (castling_rights & WHITE_OOO) c += 'Q';
    if (castling_rights & BLACK_OO)  c += 'k';
    if (castling_rights & BLACK_OOO) c += 'q';
    out += c.empty() ? "-" : c;

    out += " " + square_to_string(ep_square);
    out += " " + std::to_string(halfmove_clock) + " " + std::to_string(fullmove_number);
    return out;
}

std::string Position::to_board_string() const {
    std::string out;
    for (int rank = 7; rank >= 0; --rank) {
        out += char('1' + rank);
        out += ' ';
        for (int file = 0; file < 8; ++file) {
            Square s = make_square(file, rank);
            PieceType pt = piece_type_on(s);
            if (pt == PIECE_TYPE_NB) { out += " ."; continue; }
            Color col = (occupied[WHITE] & square_bb(s)) ? WHITE : BLACK;
            out += ' ';
            out += encode_piece(col, pt);
        }
        out += '\n';
    }
    out += "   a b c d e f g h\n";
    return out;
}
