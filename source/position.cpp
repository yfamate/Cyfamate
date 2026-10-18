#include <iostream>
#include <sstream>
#include <cstring> 
#include <stack>

#include "position.h"
#include "misc.h"
#include "tt.h"
#include "mate/mate.h"
#include "book/book.h"
#include "movegen.h"
#include "testcmd/unit_test.h"

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT) || defined(EVAL_NNUE)
#include "eval/evaluate_common.h"
#endif

using namespace std;
namespace YaneuraOu {

#if !STOCKFISH
using namespace Effect8;
using namespace Eval;


int Position::max_repetition_ply = 16;


constexpr bool minor_piece_table[PIECE_NB] = {
  false,         false , true ,  true ,      true ,   false ,
  false ,  true ,  false , true ,      true , true ,
  true , false , false , false ,

  false,         false , true ,  true ,      true ,   false ,
  false ,  true ,  false , true ,      true , true ,
  true , false , false , false ,
};


constexpr bool is_minor_piece(Piece pc) { return minor_piece_table[pc]; }

constexpr bool is_major_piece(Piece pc) { const int t = int(pc) & 15; return t == 5 || t == 6 || t == 8 || t == 13 || t == 14; }

#endif



namespace Zobrist {

#if STOCKFISH

Key psq[PIECE_NB][SQUARE_NB];
Key enpassant[FILE_NB];
Key castling[CASTLING_RIGHT_NB];
Key side, noPawns;

#else



Key zero;


Key side;



Key psq[PIECE_NB][SQ_NB_PLUS1];



Key hand[COLOR_NB][PIECE_HAND_NB];

#if defined(USE_PARTIAL_KEY)

Key noPawns;
#endif

#endif
};





void Position::init() {
	PRNG rng(20151225); 

	
	auto set_rand = [&](Key& h) {
        auto r1 = rng.rand<Key64>();
        auto r2 = rng.rand<Key64>();
        auto r3 = rng.rand<Key64>();
        auto r4 = rng.rand<Key64>();
        SET_HASH(h, r1, r2, r3, r4);
    };

	SET_HASH(Zobrist::zero, 0, 0, 0, 0);

	set_rand(Zobrist::side);

#if defined(USE_PARTIAL_KEY)
    set_rand(Zobrist::noPawns);
#endif

	
	
	
	
	for (auto pc : Piece())
		for (auto sq : SQ)
			if (pc)
                set_rand(Zobrist::psq[pc][sq]);
	
	
	for (auto c : COLOR)
		for (PieceType pr = NO_PIECE_TYPE; pr < PIECE_HAND_NB; ++pr)
			if (pr)
                set_rand(Zobrist::hand[c][pr]);

}





namespace {
inline void xor_piece_for_partial_key(StateInfo* st, Piece pc, Square s) {
#if defined(USE_PARTIAL_KEY)
    
    if (type_of(pc) == PAWN)
    {
        st->pawnKey ^= Zobrist::psq[pc][s];
    }
    else
    {
        if (is_minor_piece(pc))
            st->minorPieceKey ^= Zobrist::psq[pc][s];
        if (is_major_piece(pc))
            st->majorPieceKey ^= Zobrist::psq[pc][s];   

        st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];
    }
#endif
}

inline void put_piece_for_partial_key(StateInfo* st, Piece pc, Square s) {
#if defined(USE_PARTIAL_KEY)
    xor_piece_for_partial_key(st, pc, s);
    
#if defined(CYF_MATERIAL_KEY)
    
    
    
    st->materialKey += Zobrist::psq[pc][8];
#endif
#endif
}

inline void remove_piece_for_partial_key(StateInfo* st, Piece pc, Square s) {
#if defined(USE_PARTIAL_KEY)
    xor_piece_for_partial_key(st, pc, s);
#if defined(CYF_MATERIAL_KEY)
    st->materialKey -= Zobrist::psq[pc][8];
#endif
#endif
}





inline void move_piece_for_partial_key(StateInfo* st, Piece pc, Key fromTo) {
#if defined(USE_PARTIAL_KEY)
    if (type_of(pc) == PAWN)
    {
        st->pawnKey ^= fromTo;
    }
    else
    {
        if (is_minor_piece(pc))
            st->minorPieceKey ^= fromTo;
        if (is_major_piece(pc))
            st->majorPieceKey ^= fromTo;   

        st->nonPawnKey[color_of(pc)] ^= fromTo;
    }
#endif
}
} 






#if !defined (PRETTY_JP)
std::string pretty(Piece pc) { return std::string(USI_PIECE).substr(pc * 2, 2); }
#else

std::string USI_PIECE_KANJI[] = {
	" 口"," 歩"," 香"," 桂"," 銀"," 角"," 飛"," 金"," 玉"," と"," 杏"," 圭"," 全"," 馬"," 龍"," 菌"," 王",
		  "^歩","^香","^桂","^銀","^角","^飛","^金","^玉","^と","^杏","^圭","^全","^馬","^龍","^菌","^王"
};
std::string pretty(Piece pc) {
#if 1
	return USI_PIECE_KANJI[pc];
#else
	
	
	

	std::string result;
	if (pc != NO_PIECE)
		result = (color_of(pc) == BLACK) ? "\\e[32;40;1m" : "\\e[33;40;1m";
	result += USI_PIECE_KANJI[pc];
	if (pc != NO_PIECE)
		result += "\\e[m";

	return result;
#endif
}
#endif



#if STOCKFISH


void Position::set_castling_right(Color c, Square rfrom) {

    Square         kfrom = square<KING>(c);
    CastlingRights cr    = c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    Square kto = relative_square(c, cr & KING_SIDE ? SQ_G1 : SQ_C1);
    Square rto = relative_square(c, cr & KING_SIDE ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)) & ~(kfrom | rfrom);
}
#endif



#if STOCKFISH
void Position::set_check_info() const {

    update_slider_blockers(WHITE);
    update_slider_blockers(BLACK);

    Square ksq = square<KING>(~sideToMove);

    st->checkSquares[PAWN]   = attacks_bb<PAWN>(ksq, ~sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb<KNIGHT>(ksq);
    st->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
    st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;
}
#else
template<bool doNullMove, Color Us>
void Position::set_check_info() const {

    
    
    StateInfo* const si = st;   
    if (!doNullMove)
    {
        update_slider_blockers(WHITE, si);
        update_slider_blockers(BLACK, si);
    }

    
    
    
    
    si->checkSquaresReady = false;
}

template<Color Us>
void Position::set_check_squares() const {

    constexpr Color Them = ~Us;

    Square ksq = square<KING>(Them);

    

    
    

    Bitboard occ = pieces();

    
    
    
    
    
    StateInfo* const si          = st;
    const Bitboard   bishopCheck = bishopEffect(ksq, occ);
    const Bitboard   rookCheck   = rookEffect(ksq, occ);
    const Bitboard   goldCheck   = goldEffect<Them>(ksq);
    const Bitboard   kingAround  = kingEffect(ksq);

    si->checkSquares[PAWN]   = pawnEffect<Them>(ksq);
    si->checkSquares[KNIGHT] = knightEffect<Them>(ksq);
    si->checkSquares[SILVER] = silverEffect<Them>(ksq);
    si->checkSquares[BISHOP] = bishopCheck;
    si->checkSquares[ROOK]   = rookCheck;
    si->checkSquares[GOLD]   = goldCheck;

    
    si->checkSquares[LANCE] = rookCheck & lanceStepEffect<Them>(ksq);

    
    si->checkSquares[KING] = Bitboard(ZERO);

    
    
    si->checkSquares[PRO_PAWN]   = goldCheck;
    si->checkSquares[PRO_LANCE]  = goldCheck;
    si->checkSquares[PRO_KNIGHT] = goldCheck;
    si->checkSquares[PRO_SILVER] = goldCheck;
    si->checkSquares[HORSE]      = bishopCheck | kingAround;
    si->checkSquares[DRAGON]     = rookCheck | kingAround;
    si->checkSquaresReady        = true;
}
#endif











void Position::set_state() const {

#if STOCKFISH

    st->key = st->materialKey = 0;
    st->minorPieceKey         = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    set_check_info();

#else

    
    st->board_key = Zobrist::zero;
    st->hand_key  = Zobrist::zero;

#if defined(USE_PARTIAL_KEY)

    st->materialKey       = Zobrist::zero;
    st->minorPieceKey     = Zobrist::zero;
    st->majorPieceKey     = Zobrist::zero;   
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = Zobrist::zero;
    st->pawnKey                                   = Zobrist::noPawns;

#endif
    
    

    
    st->checkersBB = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    
    set_check_info<false>();
#endif


#if STOCKFISH
    for (Bitboard b = pieces(); b;)
    {
        Square s  = pop_lsb(b);
        Piece  pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][s];

        else
        {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

            if (type_of(pc) != KING)
            {
                st->nonPawnMaterial[color_of(pc)] += PieceValue[pc];

                

                if (type_of(pc) <= BISHOP)
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
            }
        }
    }
#else
    for (auto s : pieces())
    {
        auto pc = piece_on(s);

        st->board_key ^= Zobrist::psq[pc][s];

		
        put_piece_for_partial_key(st, pc, s);
    }

    for (auto c : COLOR)
        for (PieceType pr = PAWN; pr < PIECE_HAND_NB; ++pr)
            st->hand_key +=
              Zobrist::hand[c][pr]
              * (int64_t) hand_count(hand[c], pr);  

    

	st->hand = hand[sideToMove];

#endif

#if STOCKFISH
    if (st->epSquare != SQ_NONE)
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;

    st->key ^= Zobrist::castling[st->castlingRights];

    for (Piece pc : Pieces)
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
            st->materialKey ^= Zobrist::psq[pc][8 + cnt];

#else

    
    if (sideToMove == WHITE)
        st->board_key ^= Zobrist::side;

	

#endif
}











#if defined(CYFAMATE_NNUE_5GP)




namespace Cyfamate {
	void gp_init_parts(const Position& pos, StateInfo* st);                       
	
	
	inline double gp_s_hand_by_pt[PIECE_TYPE_NB] = {};
	inline double gp_s_camp_by_pt[PIECE_TYPE_NB] = {};
	inline double gp_s_promo = 0.0;
	
	inline void gp_on_move(const Position& pos, StateInfo* st, Move m, Piece captured, Color us){
		const Square to = m.to_sq();
		const PieceType ta = type_of(pos.piece_on(to));   
		const int rto = (int)rank_of(to);
		const bool to_camp = us == BLACK ? rto <= 2 : rto >= 6;
		double gs = st->gp_s;
		if (m.is_drop()) {
			gs -= gp_s_hand_by_pt[ta];
			if (to_camp) gs += gp_s_camp_by_pt[ta];
		} else {
			const PieceType tb = m.is_promote() ? PieceType(ta - PIECE_TYPE_PROMOTE) : ta;   
			if (tb != KING) {
				const int rfr = (int)rank_of(m.from_sq());
				if (us == BLACK ? rfr <= 2 : rfr >= 6) gs -= gp_s_camp_by_pt[tb];
				if (to_camp) gs += gp_s_camp_by_pt[ta];
			}
			if (m.is_promote()) gs += gp_s_promo;
			if (captured != NO_PIECE) {
				const PieceType tc = type_of(captured);
				if (us == BLACK ? rto >= 6 : rto <= 2) gs -= gp_s_camp_by_pt[tc];   
				if (tc > KING) gs -= gp_s_promo;
				gs += gp_s_hand_by_pt[raw_type_of(captured)];
			}
		}
        st->gp_s = gs;
        st->gp_king_code = st->gp_bucket = 255;
        st->gp_value_valid = false;
	}
}
#endif 
std::optional<PositionSetError> Position::set(const std::string& sfen, StateInfo* si) {
#if STOCKFISH

	std::memset(this, 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
#else

    
    
    std::memset(static_cast<void*>(this), 0, sizeof(Position));

    
    
    
    std::memset(static_cast<void*>(si), 0, sizeof(StateInfo));
#endif

    st = si;

    
    

    

    
    File f = FILE_9;
    Rank r = RANK_1;

    std::istringstream ss(sfen);
    
    ss >> std::noskipws;

    uint8_t token;
    bool    promote = false;
    size_t  idx;

#if defined(USE_EVAL_LIST)
    
    evalList.clear();

    
    
    PieceNumber piece_no_count[KING] = {
      PIECE_NUMBER_ZERO,   PIECE_NUMBER_PAWN,   PIECE_NUMBER_LANCE, PIECE_NUMBER_KNIGHT,
      PIECE_NUMBER_SILVER, PIECE_NUMBER_BISHOP, PIECE_NUMBER_ROOK,  PIECE_NUMBER_GOLD};

    
    
#endif

    
    
    constexpr int piece_limit[KING + 1] = {0, 18, 4, 4, 4, 2, 2, 4, 2};
    int piece_count[KING + 1] = {};
    int king_count[COLOR_NB] = {};
    bool hand_seen[COLOR_NB][PIECE_HAND_NB] = {};
    bool hand_piece_seen = false;

    kingSquare[BLACK] = kingSquare[WHITE] = SQ_NB;

    
    

    for (;;)
    {
        if (!(ss >> token))
            return PositionSetError("Invalid SFEN. Unexpected end of stream.");

        if (isspace(token))
            break;

        
        if (isdigit(token))
        {
            int skip = token - '0';
            if (skip < 1 || skip > 9)
                return PositionSetError("Invalid SFEN. Invalid number of squares to skip.");
            if (int(f) - skip < int(FILE_1) - 1)
                return PositionSetError("Invalid SFEN. Invalid file reached.");
            if (promote)
                return PositionSetError("Invalid SFEN. Unexpected promoted marker.");
            f -= File(skip);
        }
        
        else if (token == '/')
        {
            if (f != File(int(FILE_1) - 1))
                return PositionSetError("Invalid SFEN. Trying to end rank when not at the end of it.");
            if (r == RANK_9)
                return PositionSetError("Invalid SFEN. Invalid rank reached.");
            if (promote)
                return PositionSetError("Invalid SFEN. Unexpected promoted marker.");

            f = FILE_9;
            ++r;
        }
        
        else if (token == '+')
        {
            if (promote)
                return PositionSetError("Invalid SFEN. Unexpected promoted marker.");
            promote = true;
        }
        
        else if ((idx = PieceToCharBW.find(token)) != string::npos)
        {
            if (!is_ok(f) || !is_ok(r))
                return PositionSetError("Invalid SFEN. Invalid file reached.");

            
            auto sq = f | r;
            auto pc = Piece(idx);
            if (promote)
            {
                if (is_non_promotable_piece(pc))
                    return PositionSetError("Invalid SFEN. Invalid promoted piece.");
                pc = make_promoted_piece(pc);
            }
            const auto raw = type_of(pc) == KING ? KING : raw_type_of(pc);
            if (raw == KING && ++king_count[color_of(pc)] > 1)
                return PositionSetError("Invalid SFEN. Duplicate king.");
            if (++piece_count[raw] > piece_limit[raw])
                return PositionSetError("Invalid SFEN. Piece stock exceeded.");
            put_piece(pc, sq);

#if defined(USE_EVAL_LIST)
            PieceNumber piece_no = (idx == B_KING) ? PIECE_NUMBER_BKING :  
                                     (idx == W_KING) ? PIECE_NUMBER_WKING
                                                     :                           
                                     piece_no_count[raw_type_of(Piece(idx))]++;  
            evalList.put_piece(piece_no, sq, pc);  
#endif

            
            --f;

            
            promote = false;
        }
        else
            return PositionSetError(std::string("Invalid SFEN. Invalid piece: ")
                                    + std::string(1, char(token)));
    }

    if (r != RANK_9 || f != File(int(FILE_1) - 1))
        return PositionSetError("Invalid SFEN. Board state encoding ended but cursor not at end.");

    if (promote)
        return PositionSetError("Invalid SFEN. Unexpected promoted marker.");

    
    
    update_bitboards();

    
    update_kingSquare();

    
    

    if (!(ss >> token))
        return PositionSetError("Invalid SFEN. Unexpected end of stream.");
    if (token != 'b' && token != 'w')
        return PositionSetError(std::string("Invalid SFEN. Invalid side to move: ")
                                + std::string(1, char(token)));
    sideToMove = (token == 'w' ? WHITE : BLACK);
    if (!(ss >> token) || !isspace(token))
        return PositionSetError("Invalid SFEN. Expected whitespace after side to move.");

    

    hand[BLACK] = hand[WHITE] = (Hand) 0;
    int ct                    = 0;
    bool hand_token_read      = false;
    for (;;)
    {
        if (!(ss >> token))
        {
            if (hand_token_read)
                break;
            return PositionSetError("Invalid SFEN. Unexpected end of stream.");
        }

        hand_token_read = true;

        if (isspace(token))
            break;

        
        if (token == '-') {
            if (ct || hand_piece_seen || (ss.peek() != std::char_traits<char>::eof()
                                         && !isspace(static_cast<unsigned char>(ss.peek()))))
                return PositionSetError("Invalid SFEN. Invalid empty hand marker.");
            break;
        }

        if (isdigit(token)) {
            
            
            
            if ((!ct && token == '0') || ct > (18 - (token - '0')) / 10)
                return PositionSetError("Invalid SFEN. Invalid hand count.");
            ct = (token - '0') + ct * 10;
        }
        else if ((idx = PieceToCharBW.find(token)) != string::npos)
        {
            
            ct = max(ct, 1);
            Piece pc = Piece(idx);
            PieceType rpc = raw_type_of(Piece(idx));
            if (rpc < PIECE_HAND_ZERO || PIECE_HAND_NB <= rpc || is_promoted(pc))
                return PositionSetError(std::string("Invalid SFEN. Invalid hand piece: ")
                                        + std::string(1, char(token)));

            const auto color = color_of(pc);
            if (hand_seen[color][rpc])
                return PositionSetError("Invalid SFEN. Duplicate hand piece token.");
            hand_seen[color][rpc] = true;
            hand_piece_seen = true;
            if (ct > piece_limit[rpc] - piece_count[rpc])
                return PositionSetError("Invalid SFEN. Piece stock exceeded.");
            piece_count[rpc] += ct;

            
            for (int i = 0; i < ct; ++i)
            {
				
                put_hand_piece(color_of(pc), rpc);

#if defined(USE_EVAL_LIST)
                PieceNumber piece_no = piece_no_count[rpc]++;
                ASSERT_LV1(is_ok(piece_no));
                evalList.put_piece(piece_no, color_of(pc), rpc, i);
#endif
            }
            ct = 0;
        }
        else
            return PositionSetError(std::string("Invalid SFEN. Invalid hand piece: ")
                                    + std::string(1, char(token)));
    }

    if (ct != 0)
        return PositionSetError("Invalid SFEN. Missing hand piece after count.");

#if STOCKFISH
    
    
    
    
    

    while ((ss >> token) && !isspace(token))
    {
        Square rsq;
        Color  c    = islower(token) ? BLACK : WHITE;
        Piece  rook = make_piece(c, ROOK);

        token = char(toupper(token));

        if (token == 'K')
            for (rsq = relative_square(c, SQ_H1); piece_on(rsq) != rook; --rsq)
            {}

        else if (token == 'Q')
            for (rsq = relative_square(c, SQ_A1); piece_on(rsq) != rook; ++rsq)
            {}

        else if (token >= 'A' && token <= 'H')
            rsq = make_square(File(token - 'A'), relative_rank(c, RANK_1));

        else
            continue;

        set_castling_right(c, rsq);
    }

    
    
    bool enpassant = false;

    if (((ss >> col) && (col >= 'a' && col <= 'h'))
        && ((ss >> row) && (row == (sideToMove == WHITE ? '6' : '3'))))
    {
        st->epSquare = make_square(File(col - 'a'), Rank(row - '1'));

        
        
        
        
        enpassant = attacks_bb<PAWN>(st->epSquare, ~sideToMove) & pieces(sideToMove, PAWN)
                 && (pieces(~sideToMove, PAWN) & (st->epSquare + pawn_push(~sideToMove)))
                 && !(pieces() & (st->epSquare | (st->epSquare + pawn_push(sideToMove))));
    }

    if (!enpassant)
        st->epSquare = SQ_NONE;

#endif

    
    

#if STOCKFISH
    
    
    

    ss >> std::skipws >> st->rule50 >> gamePly;

    
    
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    chess960 = isChess960;

#else

    

    
    
    gamePly = 0;
    ss >> std::skipws >> gamePly;

#endif

    

    set_state();

    
    

    

#if defined(LONG_EFFECT_LIBRARY)
    
    LongEffect::calc_effect(*this);
#endif

    

#if defined(USE_PIECE_VALUE)
    st->materialValue = Eval::material(*this);
#endif

#if defined(CYFAMATE_NNUE_5GP)
    Cyfamate::gp_init_parts(*this, st);   
#endif

#if defined(USE_CLASSIC_EVAL)
    Eval::compute_eval(*this);
#endif

    

#if STOCKFISH
    assert(pos_is_ok());
#else

#if ASSERT_LV >= 3
    
    if (!is_ok(*this))
        std::cout << "info string Illigal Position?" << endl;
		
#endif
#endif

    return std::nullopt;
}








#if STOCKFISH

string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_8; r >= RANK_1; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
        {
            for (emptyCnt = 0; f <= FILE_H && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_H)
                ss << PieceToChar[piece_on(make_square(f, r))];
        }

        if (r > RANK_1)
            ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');

    if (can_castle(WHITE_OOO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

    if (can_castle(BLACK_OO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');

    if (can_castle(BLACK_OOO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

    if (!can_castle(ANY_CASTLING))
        ss << '-';

    ss << (ep_square() == SQ_NONE ? " - " : " " + UCIEngine::square(ep_square()) + " ")
       << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

    return ss.str();
}

#else

const std::string Position::sfen(int gamePly_) const
{
	std::ostringstream ss;

	
	int emptyCnt;
	for (Rank r = RANK_1; r <= RANK_9; ++r)
	{
		for (File f = FILE_9; f >= FILE_1; --f)
		{
			
			
			for (emptyCnt = 0; f >= FILE_1 && piece_on(f | r) == NO_PIECE; --f)
				++emptyCnt;

			
			if (emptyCnt)
				ss << emptyCnt;

			
			if (f >= FILE_1)
				ss << (piece_on(f | r));
		}

		
		if (r < RANK_9)
			ss << '/';
	}

	
	ss << (sideToMove == WHITE ? " w " : " b ");

	
	int n;
	bool found = false;
	for (Color c = BLACK; c <= WHITE; ++c)
		for (int pn = 0 ; pn < 7; ++ pn)
		{
			
			
			
			

			const PieceType USI_Hand[7] = { ROOK,BISHOP,GOLD,SILVER,KNIGHT,LANCE,PAWN };
			auto p = USI_Hand[pn];

			
			n = hand_count(hand[c], p);
			
			if (n != 0)
			{
				
				found = true;

				
				if (n != 1)
					ss << n;

				ss << PieceToCharBW[make_piece(c, p)];
			}
		}

	
	if (!found)
		ss << '-';

	

	
	if (gamePly_ >= 0)
		ss << ' ' << gamePly_;

	return ss.str();
}
#endif










void Position::update_slider_blockers(Color c, StateInfo* const si) const {

	Square ksq = square<KING>(c);

    
    
    Bitboard blockers = ZERO;
    Bitboard pinnersBB = ZERO;

    
    

#if STOCKFISH
    Bitboard snipers = (  (attacks_bb<  ROOK>(ksq) & pieces(QUEEN, ROOK))
                        | (attacks_bb<BISHOP>(ksq) & pieces(QUEEN, BISHOP))) & pieces(~c);
#else

	
    

    Bitboard snipers =
      ((pieces(ROOK_DRAGON) & rookStepEffect(ksq))
       | (pieces(BISHOP_HORSE) & bishopStepEffect(ksq))
       
       | (pieces(LANCE) & lanceStepEffect(c, ksq)))
      & pieces(~c);
#endif

    
    Bitboard occupancy = pieces() ^ snipers;

    

    while (snipers)
    {
        Square   sniperSq = snipers.pop();
        Bitboard b        = between_bb(ksq, sniperSq) & occupancy;

        
        if (b && !b.more_than_one())
        {
            blockers |= b;
            if (b & pieces(c))
                pinnersBB |= sniperSq;
        }
    }

    si->blockersForKing[c] = blockers;
    si->pinners[~c]        = pinnersBB;
}













FORCE_INLINE Bitboard Position::attackers_to(Square sq, const Bitboard& occ) const {
    

	ASSERT_LV3(sq <= SQ_NB);

    
    
    
    
    
    Bitboard bb =
      
      ((  (pawnEffect<WHITE>(sq)   & pieces(PAWN))
		| (knightEffect<WHITE>(sq) & pieces(KNIGHT))
        | (silverEffect<WHITE>(sq) & pieces(SILVER_HDK))
        | (goldEffect<WHITE>(sq)   & pieces(GOLDS_HDK)))
       & pieces(BLACK))
      |

      
      (((pawnEffect<BLACK>(sq)     & pieces(PAWN))
		| (knightEffect<BLACK>(sq) & pieces(KNIGHT))
        | (silverEffect<BLACK>(sq) & pieces(SILVER_HDK))
        | (goldEffect<BLACK>(sq)   & pieces(GOLDS_HDK)))
       & pieces(WHITE));

      
    const Bitboard bh = pieces(BISHOP_HORSE);
    if (bishopStepEffect(sq) & bh)
        bb |= bishopEffect(sq, occ) & bh;

    const Bitboard rl = pieces(ROOK_DRAGON) | (pieces(BLACK, LANCE) & lanceStepEffect<WHITE>(sq))
                      | (pieces(WHITE, LANCE) & lanceStepEffect<BLACK>(sq));
        
    if (rookStepEffect(sq) & rl)
        bb |= rookEffect(sq, occ) & rl;

    return bb;

	

}

#if STOCKFISH



bool Position::attackers_to_exist(Square s, Bitboard occupied, Color c) const {

    return ((attacks_bb<ROOK>(s) & pieces(c, ROOK, QUEEN))
            && (attacks_bb<ROOK>(s, occupied) & pieces(c, ROOK, QUEEN)))
        || ((attacks_bb<BISHOP>(s) & pieces(c, BISHOP, QUEEN))
            && (attacks_bb<BISHOP>(s, occupied) & pieces(c, BISHOP, QUEEN)))
        || (((attacks_bb<PAWN>(s, ~c) & pieces(PAWN)) | (attacks_bb<KNIGHT>(s) & pieces(KNIGHT))
             | (attacks_bb<KING>(s) & pieces(KING)))
            & pieces(c));
}
#endif







const std::string Position::flipped_sfen(int gamePly_) const
{
	std::ostringstream ss;

	
	int emptyCnt;
	for (Rank r = RANK_9; r >= RANK_1; --r)
	{
		for (File f = FILE_1; f <= FILE_9; ++f)
		{
			
			
			for (emptyCnt = 0; f <= FILE_9 && piece_on(f | r) == NO_PIECE; ++f)
				++emptyCnt;

			
			if (emptyCnt)
				ss << emptyCnt;

			
			if (f <= FILE_9)
				
				ss << Piece(piece_on(f | r) ^ PIECE_WHITE);
		}

		
		if (r > RANK_1)
			ss << '/';
	}

	
	
	ss << (~sideToMove == WHITE ? " w " : " b ");

	
	int n;
	bool found = false;
	for (Color c = BLACK; c <= WHITE; ++c)
		for (int pn = 0 ; pn < 7; ++ pn)
		{
			
			
			
			

			const PieceType USI_Hand[7] = { ROOK,BISHOP,GOLD,SILVER,KNIGHT,LANCE,PAWN };
			auto p = USI_Hand[pn];

			
			
			n = hand_count(hand[~c], p);
			
			if (n != 0)
			{
				
				found = true;

				
				if (n != 1)
					ss << n;

				ss << PieceToCharBW[make_piece(c, p)];
			}
		}

	
	if (!found)
		ss << '-';

	

	
	if (gamePly_ >= 0)
		ss << ' ' << gamePly_;

	return ss.str();
}


const std::string Position::sfen_to_flipped_sfen(std::string sfen)
{
#if 1
	Position pos;
	StateInfo si;
	pos.set(sfen, &si);
	return pos.flipped_sfen();
#else
	
	

	
#endif
}




void Position::update_bitboards()
{
	
	byTypeBB[HDK]			= pieces(KING , HORSE , DRAGON);

	
	byTypeBB[GOLDS]			= pieces(GOLD , PRO_PAWN , PRO_LANCE , PRO_KNIGHT , PRO_SILVER);

	

	
	byTypeBB[BISHOP_HORSE]	= pieces(BISHOP , HORSE);

	
	byTypeBB[ROOK_DRAGON]	= pieces(ROOK   , DRAGON);

	
	byTypeBB[SILVER_HDK]	= pieces(SILVER , HDK);

	
	byTypeBB[GOLDS_HDK]		= pieces(GOLDS  , HDK);
}


void Position::update_kingSquare()
{
	for (auto c : COLOR)
	{
		
		auto b = pieces(c, KING);
		kingSquare[c] = b ? b.pop() : SQ_NB;
	}
}






std::ostream& operator<<(std::ostream& os, const Position& pos)
{
	
	for (Rank r = RANK_1; r <= RANK_9; ++r)
	{
		for (File f = FILE_9; f >= FILE_1; --f)
			os << pretty(pos.board[f | r]);
		os << endl;
	}

#if !defined (PRETTY_JP)
	
	os << "BLACK HAND : " << pos.hand[BLACK] << " , WHITE HAND : " << pos.hand[WHITE] << endl;

	
	os << "Turn = " << pos.sideToMove << endl;
#else
	os << "先手 手駒 : " << pos.hand[BLACK] << " , 後手 手駒 : " << pos.hand[WHITE] << endl;
	os << "手番 = " << pos.sideToMove << endl;
#endif

	
	os << "sfen " << pos.sfen() << endl;

	return os;
}

#if defined (KEEP_LAST_MOVE)


std::string Position::moves_from_start(bool is_pretty) const
{
	StateInfo* p = st;
	std::stack<StateInfo*> states;
	while (p->previous != nullptr)
	{
		states.push(p);
		p = p->previous;
	}

	stringstream ss;
	while (states.size())
	{
		auto& top = states.top();
		if (is_pretty)
			ss << pretty(top->lastMove, top->lastMovedPieceType) << ' ';
		else
			ss << top->lastMove << ' ';
		states.pop();
	}
	return ss.str();
}
#endif





inline Bitboard Position::attackers_to_pawn(Color c, Square pawn_sq) const
{
	ASSERT_LV3(is_ok(c) && pawn_sq <= SQ_NB);

	Color them = ~c;
	const Bitboard& occ = pieces();

	
	const Bitboard bb_hd =  pieces(HORSE,DRAGON);
	
	

	
	
	return
		(     (knightEffect(them, pawn_sq) &  pieces(KNIGHT)          )
			| (silverEffect(them, pawn_sq) & (pieces(SILVER) | bb_hd) )
			| (goldEffect(them, pawn_sq)   & (pieces(GOLDS)  | bb_hd) )
			| (bishopEffect(pawn_sq, occ)  &  pieces(BISHOP_HORSE)    )
			| (rookEffect(pawn_sq, occ)    &  pieces(ROOK_DRAGON)     )
			) & pieces(c);
}



bool Position::gives_check(Move m) const
{
	ensure_check_squares();  
	return gives_check_ready(m);
}

bool Position::gives_check_ready(Move m) const
{
	
	ASSERT_LV3(m.is_ok());
	ASSERT_LV3(st->checkSquaresReady);   

	
	const Square to = m.to_sq();

	
	
	if (st->checkSquares[type_of(moved_piece_after(m))] & to)
		return true;

	

	
	const Square from = m.from_sq();

	
	
	return  !m.is_drop()
		&& (((blockers_for_king(~sideToMove) ) & from)
		&&  !aligned(from, to, square<KING>(~sideToMove)));
}


bool Position::is_mated() const
{
	
	return MoveList<LEGAL>(*this).size() == 0;
}





bool Position::legal_drop(const Square to) const
{
	const auto us = sideToMove;

	
	ASSERT_LV3(pawnEffect(us, to) == Bitboard(square<KING>(~us)));

	
	if (!effected_to(us, to))
		return true;

	
	
	Bitboard b = attackers_to_pawn(~us, to);

	
	Bitboard pinned = blockers_for_king(~us);

	
	if (b & (~pinned | file_bb(file_of(to))))
		return true;

	
	
	
	
	
	
	
	

	
	
	

	
	
	
	

	
	
	
	
	

	
	

	
	Square sq_king = square<KING>(~us);

#if !defined(LONG_EFFECT_LIBRARY)
	

	Bitboard escape_bb = kingEffect(sq_king) & ~pieces(~us);
	escape_bb ^= to;
	auto occ = pieces() ^ to; 
	while (escape_bb)
	{
		Square king_to = escape_bb.pop();
		if (!attackers_to(us, king_to, occ))
			return true; 
	}

	
	return false;
#else
	

	auto a8_effet_us = board_effect[us].around8(sq_king);
	
	
	auto a8_them_movable = Effect8::around8(~pieces(~us), sq_king) & Effect8::board_mask(sq_king);

	
	
	if (~a8_effet_us & a8_them_movable)
		return true;

	
	
	auto a8_long_effect_to = long_effect.directions_of(us, to);
	auto to_dir = (us == BLACK) ? DIRECT_D : DIRECT_U;  
	auto a8_cutoff_dir = Effect8::cutoff_directions(to_dir, a8_long_effect_to);
	auto a8_target = a8_cutoff_dir & a8_them_movable & ~board_effect[us].around8_greater_than_one(sq_king);

	return a8_target != 0;
#endif
}


bool Position::legal_pawn_drop(const Color us, const Square to) const
{
	return !((pieces(us, PAWN) & file_bb(file_of(to)))                               
		|| ((pawnEffect(us, to) == Bitboard(square<KING>(~us)) && !legal_drop(to)))); 
}














bool Position::pseudo_legal(const Move m, bool generate_all_legal_moves) const {
	return generate_all_legal_moves ? pseudo_legal_s<true>(m) : pseudo_legal_s<false>(m);
}


template <bool All>
bool Position::pseudo_legal_s(const Move m) const {

	const Color  us = sideToMove;
	const Square to = m.to_sq(); 

	if (m.is_drop())
	{
		const PieceType pr = m.move_dropped_piece();
		
		

		
		if (moved_piece_after(m) != make_piece(us, pr))
			return false;

		ASSERT_LV3(PAWN <= pr && pr < KING);

		
		if (piece_on(to) != NO_PIECE || hand_count(hand[us], pr) == 0)
			return false;

		if (in_check())
		{
			
			Bitboard target = checkers();
			Square checksq = target.pop();

			
			
			if (target)
				return false;

			
			
			if (!(between_bb(checksq, square<KING>(us)) & to))
				return false;
		}

		
		if (pr == PAWN && !legal_pawn_drop(us, to))
			return false;

		

		
		
		

		
		
		

	}
	else {

		const Square from = m.from_sq();
		const Piece pc    = piece_on(from);

		
		if (pc == NO_PIECE || color_of(pc) != us)
			return false;

		
		if (!(effects_from(pc, from, pieces()) & to))
			return false;

		
		if (pieces(us) & to)
			return false;

		PieceType pt = type_of(pc);
		if (m.is_promote())
		{
			

			
			if (is_non_promotable_piece(pc))
				return false;

			
			if (moved_piece_after(m) != make_promoted_piece(pc))
				return false;
		}
		else {

			
			

			
			if (moved_piece_after(m) != pc)
				return false;

			
			
			
			
			
			if (pt == NO_PIECE_TYPE)
				return false;

			
			
			
			
			

			if (All)
			{
				
				
				
				

				if (pt == PAWN || pt == LANCE)
					if ((us == BLACK && rank_of(to) == RANK_1) || (us == WHITE && rank_of(to) == RANK_9))
						return false;
			}
			else {

				
				
				switch (pt)
				{
				case PAWN:
					if (enemy_field(us) & to)
						return false;
					break;

				case LANCE:
					if ((us == BLACK && rank_of(to) <= RANK_2) || (us == WHITE && rank_of(to) >= RANK_8))
						return false;
					break;

				case BISHOP:
				case ROOK:
					if (enemy_field(us) & (Bitboard(from) | Bitboard(to)))
						return false;
					break;

				default:
					break;
				}
			}

		}

		
		if (checkers())
		{
			

			
			if (type_of(pc) != KING)
			{
				
				if (checkers().more_than_one())
					return false;

				
				
				
				
				
				if (!((between_bb(checkers().pop(), square<KING>(us)) | checkers()) & to))
					return false;
			}

			

		}
	}

	return true;
}


bool Position::legal(Move m) const
{
	if (m.is_drop())
		
		return true;
	else
	{
		Color us    = sideToMove;
		Square from = m.from_sq();

		ASSERT_LV5(color_of(piece_on(m.from_sq())) == us);
		ASSERT_LV5(piece_on(square<KING>(us)) == make_piece(us, KING));

		
		if (type_of(piece_on(from)) == KING)
			return !effected_to(~us, m.to_sq(), from);

		
		
		return !(blockers_for_king(us) & from)
			 || aligned(from, m.to_sq(), square<KING>(us));
	}
}






bool Position::legal_promote(Move m) const
{
	
	if (!m.is_promote())
		return true;

	Color us = sideToMove;
	Square from = m.from_sq();
	Square to   = m.to_sq();

	
	return enemy_field(us) & (Bitboard(from) | Bitboard(to));
}


Move Position::to_move(Move16 m16) const
{
	
	

	
	

	Move m = (Move)m16.to_u16();

	
	
	
	
	if (!m.is_ok())
		return m;

	if (m.is_drop())
		return Move(m.to_u16() + (u32(make_piece(side_to_move(), m.move_dropped_piece())) << 16));
		
		

	
	
	Piece moved_piece = piece_on(m.from_sq());
	if (color_of(moved_piece) != side_to_move() || moved_piece == NO_PIECE)
		return Move::none();

	
	if (m.is_promote())
	{
		
		if (is_non_promotable_piece(moved_piece))
			return Move::none();

		return Move(m.to_u16() + (u32(make_promoted_piece(moved_piece)) << 16));
	}

	
	return Move(m.to_u16() + (u32(moved_piece) << 16));
}








template<Color Us, typename T>
void Position::do_move_impl(Move m, StateInfo& newSt, bool givesCheck, const T* tt) {

    ASSERT_LV3(m.is_ok());
    ASSERT_LV3(&newSt != st);

    
    
    
    
    if constexpr (std::is_same_v<T, TranspositionTable>)
    {
#if STOCKFISH
#else
        const Square pfTo    = m.to_sq();
        const Piece  pfAfter = moved_piece_after(m);
        auto         pk      = st->board_key ^ Zobrist::side;
        auto         ph      = st->hand_key;
        if (m.is_drop())
        {
            ph -= Zobrist::hand[Us][raw_type_of(pfAfter)];
            pk ^= Zobrist::psq[pfAfter][pfTo];
        }
        else
        {
            const Square pfFrom = m.from_sq();
            const Piece  pfCap  = piece_on(pfTo);
            if (pfCap)
            {
                pk ^= Zobrist::psq[pfCap][pfTo];
                ph += Zobrist::hand[Us][raw_type_of(pfCap)];
            }
            pk ^= Zobrist::psq[piece_on(pfFrom)][pfFrom] ^ Zobrist::psq[pfAfter][pfTo];
        }
        prefetch(tt->first_entry(pk ^ ph, ~Us));
#endif
    }

    
    
    if (givesCheck && !st->checkSquaresReady)
        set_check_squares<Us>();

    
    
    

    
    
#if STOCKFISH
    Key k = st->key ^ Zobrist::side;
#else
    Key k = st->board_key ^ Zobrist::side;

    
    Key h = st->hand_key;
#endif

    
    
    

    
    
    

    

#if STOCKFISH
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
#else
    std::memcpy(static_cast<void*>(&newSt), st, offsetof(StateInfo, board_key));
#endif
    
    
    
    StateInfo* const prevSt = st;
    newSt.previous = st;
    st             = &newSt;

    

    
    

    
    

    ++gamePly;

#if STOCKFISH
    ++st->rule50;
#endif
    ++st->pliesFromNull;

#if STOCKFISH
    Color us   = sideToMove;
    Color them = ~us;
#else
    
    constexpr Color us   = Us;
    constexpr Color them = ~Us;
#endif


    
#if defined(USE_CLASSIC_EVAL)

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)
    st->sum.p[0][0] = VALUE_NOT_EVALUATED;
#endif
#if defined(EVAL_NNUE)
    st->accumulator.computed_accumulation = false;
    st->accumulator.computed_score        = false;
#endif
#if defined(CYF_GPU_POOL)
    st->cyf_pool_req = 0;
#endif

#if defined(USE_BOARD_EFFECT_PREV)
    
    
    std::memcpy(board_effect_prev, board_effect, sizeof(board_effect));
#endif

    

#if defined(KEEP_LAST_MOVE)
    st->lastMove           = m;
    st->lastMovedPieceType = m.is_drop() ? PieceType(m.from_sq()) : type_of(piece_on(m.from_sq()));
#endif

#endif

    
    
    

	


#if STOCKFISH
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = m.type_of() == EN_PASSANT ? make_piece(them, PAWN) : piece_on(to);
#else
    

    
    Square to = m.to_sq();
    ASSERT_LV2(is_ok(to));

    
    Piece captured = piece_on(to);

    
    

    ASSERT_LV3(type_of(captured) != KING);

#endif

#if defined(USE_PIECE_VALUE)
    
    int materialDiff;
#endif

#if defined(USE_CLASSIC_EVAL)

#if defined(USE_EVAL_LIST)
    auto& dp = st->dirtyPiece;
#endif
#endif

    if (m.is_drop())
    {
        

        
        ASSERT_LV3(empty(to));

        Piece     pc = moved_piece_after(m);
        PieceType pr = raw_type_of(pc);
        ASSERT_LV3(PAWN <= pr && pr < PIECE_HAND_NB);

        
        h -= Zobrist::hand[Us][pr];
        k ^= Zobrist::psq[pc][to];

        


#if defined(USE_PARTIAL_KEY) & 0
        
        if (pr == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][to];
#endif

        put_piece(pc, to);
        put_piece_for_partial_key(&newSt, pc, to);

        
#if defined(USE_EVAL_LIST)
        PieceNumber piece_no = piece_no_of(Us, pr);
        ASSERT_LV3(is_ok(piece_no));

        
        dp.dirty_num                  = 1;  
        dp.pieceNo[0]                 = piece_no;
        dp.changed_piece[0].old_piece = evalList.bona_piece(piece_no);
        evalList.put_piece(piece_no, to, pc);
        dp.changed_piece[0].new_piece = evalList.bona_piece(piece_no);
#endif

        
        remove_hand_piece(Us, pr);

        
        
		
        if (givesCheck)
        {
            newSt.checkersBB = Bitboard(to);
            newSt.continuousCheck[Us] += 2;
        }
        else
        {
            newSt.checkersBB          = Bitboard(ZERO);
            newSt.continuousCheck[Us] = 0;
        }

        
        newSt.capturedPiece = NO_PIECE;

        
        update_bitboards();

#if defined(USE_PIECE_VALUE)
        
        materialDiff = 0;
#endif

#if defined(LONG_EFFECT_LIBRARY)
        
        LongEffect::update_by_dropping_piece<Us>(*this, to, pc);
#endif
    }
    else
    {
        

		Square from = m.from_sq();
        ASSERT_LV2(is_ok(from));

        
        Piece moved_pc = piece_on(from);
        ASSERT_LV2(moved_pc != NO_PIECE);

        
        
        Piece moved_after_pc = moved_piece_after(m);

#if defined(USE_PIECE_VALUE)
        materialDiff = m.is_promote() ? Eval::ProDiffPieceValue[moved_pc] : 0;
#endif
        

		
        if (captured)
        {
            

			
            Square capsq = to;

#if defined(LONG_EFFECT_LIBRARY)
            
            LongEffect::update_by_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc,
                                                      captured);
#endif

			PieceType pr = raw_type_of(captured);

            
#if defined(USE_EVAL_LIST)
			
			PieceNumber piece_no = piece_no_of(to);
			ASSERT_LV3(is_ok(piece_no));
			dp.dirty_num                  = 2;  
			dp.pieceNo[1]                 = piece_no;
			dp.changed_piece[1].old_piece = evalList.bona_piece(piece_no);
			evalList.put_piece(piece_no, Us, pr, hand_count(hand[Us], pr));
			dp.changed_piece[1].new_piece = evalList.bona_piece(piece_no);
#endif

            
            
            
            put_hand_piece(Us, pr);

#if defined(USE_SFNN)
            dp.remove_pc = captured;
            dp.remove_sq = capsq;
#endif

	        
            
            remove_piece(to);
            remove_piece_for_partial_key(&newSt, captured, to);

            
            k ^= Zobrist::psq[captured][capsq];
#if !STOCKFISH
            h += Zobrist::hand[Us][pr];
#endif

#if defined(USE_PIECE_VALUE)
            
            materialDiff += Eval::CapturePieceValue[captured];
#endif

			
            newSt.capturedPiece = captured;
        }
        else
        {
            

            newSt.capturedPiece = NO_PIECE;

#if defined(LONG_EFFECT_LIBRARY)
            
            LongEffect::update_by_no_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc);
#endif
#if defined(USE_EVAL_LIST)
            dp.dirty_num = 1;  
#endif
        }

        

        const Key moveKey = Zobrist::psq[moved_pc][from] ^ Zobrist::psq[moved_after_pc][to];
        k ^= moveKey;

        


#if defined(USE_EVAL_LIST)
        
        PieceNumber piece_no2         = piece_no_of(from);
        dp.pieceNo[0]                 = piece_no2;
        dp.changed_piece[0].old_piece = evalList.bona_piece(piece_no2);
#endif

        
        remove_piece(from);

        
        put_piece(moved_after_pc, to);

        
        
        
        if (moved_pc == moved_after_pc)
            move_piece_for_partial_key(&newSt, moved_pc, moveKey);
        else
        {
            remove_piece_for_partial_key(&newSt, moved_pc, from);
            put_piece_for_partial_key(&newSt, moved_after_pc, to);
        }

#if defined(USE_EVAL_LIST)
        evalList.put_piece(piece_no2, to, moved_after_pc);
        dp.changed_piece[0].new_piece = evalList.bona_piece(piece_no2);
#endif

        
        
        
        

        if (type_of(moved_pc) == KING)
            kingSquare[Us] = to;

        
        
        update_bitboards();

        
        if (givesCheck)
        {
#if 1
            


            
            ASSERT_LV3(prevSt->checkSquaresReady);   
            newSt.checkersBB = prevSt->checkSquares[type_of(moved_after_pc)] & to;

            
            const Square ksq = square<KING>(them);
            
            
            

            if (discovered(from, to, ksq, prevSt->blockersForKing[them] & from))
            {
                
                
                

                
                

                
                
                
                
                
                
                

                newSt.checkersBB |= directEffect(from, direct_of(ksq, from), pieces()) & pieces(Us);
            }

            
            ASSERT_LV3(st->checkersBB == attackers_to<Us>(square<KING>(them)));
#else
            
            st->checkersBB = attackers_to<Us>(square<KING>(Them));
#endif
            
            newSt.continuousCheck[Us] += 2;
        }
        else
        {

            newSt.checkersBB          = Bitboard(ZERO);
            newSt.continuousCheck[Us] = 0;
        }
    }
    
    
    

#if defined(USE_PIECE_VALUE)
    newSt.materialValue =
      (Value) (prevSt->materialValue + (Us == BLACK ? materialDiff : -materialDiff));
    
#endif
#if defined(CYFAMATE_NNUE_5GP)
    Cyfamate::gp_on_move(*this, st, m, captured, Us);
#endif

    
    sideToMove = them;

    
    newSt.board_key = k;
    newSt.hand_key  = h;
    rep_enter();

    newSt.hand = hand[them];

    
    set_check_info<false>();

    
    
    

    
    
    

#if !defined(ENABLE_QUICK_DRAW)
    st->repetition       = 0;
    st->repetition_times = 0;
    st->repetition_type  = REPETITION_NONE;

    
    int end = std::min(max_repetition_ply , st->pliesFromNull);  

    
    
    

    
    if (end >= 4)
    {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2)
        {
            stp = stp->previous->previous;
            if (stp->board_key == st->board_key)
            {
                
                if (stp->hand == st->hand)
                {
                    

                    

                    
                    st->repetition_times = stp->repetition_times + 1;

                    
                    
                    
                    
                    
                    
                    st->repetition = st->repetition_times >= 3 ? -i : i;

                    
                    
                    st->repetition_type = (i <= st->continuousCheck[sideToMove])  ? REPETITION_LOSE
                                        : (i <= st->continuousCheck[~sideToMove]) ? REPETITION_WIN
                                                                                  : REPETITION_DRAW;

                    
                    
                    
                    
                    
                    
                    
                    
                    

                    
                    
                    
                    
                    

                    if (stp->repetition_times && st->repetition_type != stp->repetition_type)
                        st->repetition_type = REPETITION_DRAW;

                    break;
                }
                else
                {

                    

                    

                    if (hand_is_equal_or_superior(st->hand, stp->hand))
                    {
                        st->repetition_type = REPETITION_SUPERIOR;
                        st->repetition      = i;
                        
                        break;
                    }

                    if (hand_is_equal_or_superior(stp->hand, st->hand))
                    {
                        st->repetition_type = REPETITION_INFERIOR;
                        st->repetition      = i;
                        break;
                    }

                    
                }
            }
        }
    }
#endif

	ASSERT_LV5(pos_is_ok());
}


Key Position::key_after(Move m) const {

    Color Us = side_to_move();
    auto  k  = st->board_key ^ Zobrist::side;
    auto  h  = st->hand_key;

    
    Square to = m.to_sq();
    ASSERT_LV2(is_ok(to));

    if (m.is_drop())
    {
        
        PieceType pr = m.move_dropped_piece();
        ASSERT_LV2(PAWN <= pr && pr < PIECE_HAND_NB);

        Piece pc = make_piece(Us, pr);

        
        k ^= Zobrist::psq[pc][to];
        h -= Zobrist::hand[Us][pr];
    }
    else
    {
        
        Square from = m.from_sq();
        ASSERT_LV2(is_ok(from));

        
        Piece moved_pc = piece_on(from);
        ASSERT_LV2(moved_pc != NO_PIECE);

        
        
        Piece moved_after_pc = m.is_promote() ? make_promoted_piece(moved_pc) : moved_pc;

        
        Piece captured = piece_on(to);
        if (captured != NO_PIECE)
        {
            PieceType pr = raw_type_of(captured);

            
            k ^= Zobrist::psq[captured][to];
            h += Zobrist::hand[Us][pr];
        }

        
        k ^= Zobrist::psq[moved_pc][from];
        k ^= Zobrist::psq[moved_after_pc][to];
    }

    return k ^ h;
}


template <Color Us>
void Position::undo_move_impl(Move m) {
    

    auto to = m.to_sq();
    ASSERT_LV2(is_ok(to));

    

    Piece moved_after_pc = moved_piece_after(m);

#if defined(USE_EVAL_LIST)
    PieceNumber piece_no = piece_no_of(to);  
    ASSERT_LV3(is_ok(piece_no));
#endif

    
    

    
    static_assert((u32) MOVE_PROMOTE / (u32) PIECE_PROMOTE == 4096, "");
    
    Piece moved_pc = Piece(moved_after_pc ^ ((m.to_u16() & MOVE_PROMOTE) >> 12));

    if (m.is_drop())
    {
        

        
        PieceType pt = raw_type_of(moved_after_pc);

#if defined(USE_EVAL_LIST)
        evalList.put_piece(piece_no, Us, pt, hand_count(hand[Us], pt));
#endif
		
        put_hand_piece(Us, pt);

        
        remove_piece(to);

#if defined(LONG_EFFECT_LIBRARY)
        
        LongEffect::rewind_by_dropping_piece<Us>(*this, to, moved_after_pc);
#endif
    }
    else
    {

        

        auto from = m.from_sq();
        ASSERT_LV2(is_ok(from));

        
        remove_piece(to);
        
        
        put_piece(moved_pc, from);

        
        
        
        if (st->capturedPiece != NO_PIECE)
        {
            Piece to_pc = st->capturedPiece;

            
            put_piece(to_pc, to);

#if defined(USE_EVAL_LIST)
            PieceNumber piece_no2 =
              piece_no_of(Us, raw_type_of(to_pc));  
            ASSERT_LV3(is_ok(piece_no2));

            evalList.put_piece(piece_no2, to, to_pc);

            
            remove_hand_piece(Us, raw_type_of(to_pc));

            
            
            
            evalList.put_piece(piece_no, from, moved_pc);
#else
            
            remove_hand_piece(Us, raw_type_of(to_pc));
#endif

#if defined(LONG_EFFECT_LIBRARY)
            
            LongEffect::rewind_by_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc,
                                                      to_pc);
#endif
        }
        else
        {


#if defined(USE_EVAL_LIST)
            
            evalList.put_piece(piece_no, from, moved_pc);
#endif

#if defined(LONG_EFFECT_LIBRARY)
            
            
            LongEffect::rewind_by_no_capturing_piece<Us>(*this, from, to, moved_pc, moved_after_pc);
#endif
        }

        if (type_of(moved_pc) == KING)
            kingSquare[Us] = from;
    }

    
    
    update_bitboards();

    
    sideToMove = Us;  

    
    rep_leave();
    st = st->previous;

    --gamePly;

    
    

    ASSERT_LV5(pos_is_ok());
}


template <typename T>
void Position::do_move(Move m, StateInfo& newSt, bool givesCheck, const T* tt)
{
    if (sideToMove == BLACK)
        do_move_impl<BLACK, T>(m, newSt, givesCheck, tt);
    else
        do_move_impl<WHITE, T>(m, newSt, givesCheck, tt);
}


void Position::undo_move(Move m)
{
	if (sideToMove == BLACK)
		undo_move_impl<WHITE>(m); 
	else
		undo_move_impl<BLACK>(m);
}


template <typename T>
void Position::do_null_move(StateInfo& newSt, const T& tt) {

	ASSERT_LV3(!checkers());
	ASSERT_LV3(&newSt != st);

#if STOCKFISH
    std::memcpy(&newSt, st, sizeof(StateInfo));
#else
	std::memcpy(static_cast<void*>(& newSt), st, sizeof(StateInfo));
#endif

	newSt.previous = st;
    st             = &newSt;

#if STOCKFISH
	if (st->epSquare != SQ_NONE)
    {
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    st->key ^= Zobrist::side;
    prefetch(tt.first_entry(key()));

	st->pliesFromNull = 0;

    sideToMove = ~sideToMove;

    set_check_info();

    st->repetition = 0;

    assert(pos_is_ok());

#else

#if defined(USE_CLASSIC_EVAL) && defined(EVAL_NNUE)
    
    st->accumulator.computed_score = false;
#if defined(CYF_GPU_POOL)
    st->cyf_pool_req = 0;
#endif
#if !defined(CYF_NULL_DIRTY_LEGACY) 
    
    
    st->dirtyPiece.dirty_num = 0;
#endif
#endif

	
    
    
    

	
    
    sideToMove = ~sideToMove;

	st->board_key ^= Zobrist::side;
	rep_enter();

	
	
	if constexpr (std::is_same_v<T, TranspositionTable>)
    {
        const auto key = st->key();
        prefetch(tt.first_entry(key, sideToMove));
    }

	st->pliesFromNull = 0;

	
    st->hand = hand[sideToMove];

	
	
    
    
    

	ASSERT_LV3(st->continuousCheck[sideToMove] == 0);
    st->continuousCheck[~sideToMove] = 0;

	set_check_info<true>();

#if !defined(ENABLE_QUICK_DRAW)
	st->repetition       = 0;

	
	st->repetition_times = 0;
#endif

	ASSERT_LV5(pos_is_ok());
#endif
}

void Position::do_null_move(StateInfo& newSt) {
    
    
    do_null_move<int>(newSt, 0);
}

void Position::undo_null_move()
{
	ASSERT_LV3(!checkers());

	rep_leave();
	st = st->previous;
	sideToMove = ~sideToMove;
}


#if defined (USE_SEE)
























bool Position::see_ge(Move m, Value threshold) const
{
	ASSERT_LV3(m.is_ok());

#if STOCKFISH
    
    if (type_of(m) != NORMAL)
        return VALUE_ZERO >= threshold;
#endif

	bool drop = m.is_drop();

	

#if STOCKFISH
    Square from = m.from_sq(), to = m.to_sq();
#else
	
	
	
	Square from = drop ? SQ_NB : m.from_sq();
	Square to   = m.to_sq();
#endif

	

	
	

    int swap = PieceValue[piece_on(to)] - threshold;
	if (swap < 0)
        return false;

	
	
	

#if STOCKFISH
	swap = PieceValue[piece_on(from)] - swap;
#else
	
	PieceType from_pt = drop ? m.move_dropped_piece() : type_of(piece_on(from));
    swap = PieceValue[from_pt] - swap;
#endif

	if (swap <= 0)
        return true;

#if STOCKFISH
	assert(color_of(piece_on(from)) == sideToMove);
#else
	ASSERT_LV3(drop || color_of(piece_on(from)) == sideToMove);
#endif

    
    
    
    
    
    return see_ge_exchange(from, to, swap);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
bool Position::see_ge_exchange(Square from, Square to, int swap) const
{
    Bitboard occupied  = pieces() ^ from ^ to;  

    
    
    
    
    if (sideToMove == BLACK ? !has_attacker<WHITE>(*this, to, occupied, Bitboard(ZERO))
                            : !has_attacker<BLACK>(*this, to, occupied, Bitboard(ZERO)))
        return true;

    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int      res = 1;

    
    
    
    
    
    {
        const Color them          = ~sideToMove;
        Bitboard    themAttackers = attackers & occupied & pieces(them);
        if (pinners(sideToMove) & occupied)
            themAttackers &= ~blockers_for_king(them);
        if (!themAttackers)
            return true;
        const int v = (themAttackers & pieces(PAWN))   ? int(PawnValue)
                    : (themAttackers & pieces(LANCE))  ? int(LanceValue)
                    : (themAttackers & pieces(KNIGHT)) ? int(KnightValue)
                    : (themAttackers & pieces(SILVER)) ? int(SilverValue)
                    : (themAttackers & pieces(GOLDS))  ? int(GoldValue)
                    : (themAttackers & pieces(BISHOP)) ? int(BishopValue)
                    : (themAttackers & pieces(ROOK))   ? int(RookValue)
                    : (themAttackers & pieces(HORSE))  ? int(Eval::HorseValue)
                    : (themAttackers & pieces(DRAGON)) ? int(Eval::DragonValue)
                    : 0;  
        if (v == 0)
            return bool(attackers & occupied & ~pieces(them));
        if (v - swap < 0)
            return false;
    }

    while (true)
    {
        stm = ~stm;
        attackers &= occupied;

        
        
        if (!(stmAttackers = attackers & pieces(stm)))
            break;

        
        
        
        if (pinners(~stm) & occupied)
        {
            stmAttackers &= ~blockers_for_king(stm);

            if (!stmAttackers)
                break;
        }

        res ^= 1;

        
        
        
        

        
        if ((bb = stmAttackers & pieces(PAWN)))
        {
            

            
            if ((swap = PawnValue - swap) < res)
                break;

#if STOCKFISH
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
            
            
#endif
        }

#if !STOCKFISH
        
        else if ((bb = stmAttackers & pieces(LANCE)))
        {
            if ((swap = LanceValue - swap) < res)
                break;
        }
#endif
        else if ((bb = stmAttackers & pieces(KNIGHT)))
        {
            if ((swap = KnightValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            
            
            continue;
        }

#if !STOCKFISH
        
        else if ((bb = stmAttackers & pieces(SILVER)))
        {
            if ((swap = SilverValue - swap) < res)
                break;
        }
        
        else if ((bb = stmAttackers & pieces(GOLDS)))
        {
            
            
            if ((swap = GoldValue - swap) < res)
                break;
        }
#endif

        else if ((bb = stmAttackers & pieces(BISHOP)))
        {
            if ((swap = BishopValue - swap) < res)
                break;
#if STOCKFISH
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
#endif
        }

        else if ((bb = stmAttackers & pieces(ROOK)))
        {
            if ((swap = RookValue - swap) < res)
                break;
#if STOCKFISH
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
#endif
        }

#if !STOCKFISH
        
        else if ((bb = stmAttackers & pieces(HORSE)))
        {
            if ((swap = Eval::HorseValue - swap) < res)
                break;
            
            
        }

        
        else if ((bb = stmAttackers & pieces(DRAGON)))
        {
            if ((swap = Eval::DragonValue - swap) < res)
                break;
            
            
        }
#endif

#if STOCKFISH
        else if ((bb = stmAttackers & pieces(QUEEN)))
        {
            if ((swap = QueenValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                       | (attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN));
        }
#endif
        else  
              
              
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;

#if !STOCKFISH
        

        
        Square sq = bb.pop();
        
        occupied ^= sq;

        

        auto dirs = directions_of(to, sq);

        
        ASSERT_LV3(dirs);

        

		switch(pop_directions(dirs))
		{
		
		case DIRECT_RU: attackers |= rayEffect<DIRECT_RU>(to, occupied) & pieces(BISHOP_HORSE); break;
		case DIRECT_LD: attackers |= rayEffect<DIRECT_LD>(to, occupied) & pieces(BISHOP_HORSE); break;
		case DIRECT_RD: attackers |= rayEffect<DIRECT_RD>(to, occupied) & pieces(BISHOP_HORSE); break;
		case DIRECT_LU: attackers |= rayEffect<DIRECT_LU>(to, occupied) & pieces(BISHOP_HORSE); break;

		
		case DIRECT_U : attackers |= rayEffect<DIRECT_U >(to, occupied) & (pieces(ROOK_DRAGON) | pieces(WHITE, LANCE)); break;

		
		case DIRECT_D : attackers |= rayEffect<DIRECT_D >(to, occupied) & (pieces(ROOK_DRAGON) | pieces(BLACK, LANCE)); break;

		
		case DIRECT_L : attackers |= rayEffect<DIRECT_L >(to, occupied) & pieces(ROOK_DRAGON); break;
		case DIRECT_R : attackers |= rayEffect<DIRECT_R >(to, occupied) & pieces(ROOK_DRAGON); break;

		default: UNREACHABLE; break;
		}

        

        
        
#endif
	}

    return bool(res);
}

#endif 





#if 0




bool Position::is_draw(int ply) const {

    if (st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
        return true;

    
    
    return st->repetition && st->repetition < ply;
}
#endif


RepetitionState Position::is_repetition(int ply) const
{
#if !defined(ENABLE_QUICK_DRAW)

	
    
	
	
	

	
	
	

	
	
	

    if (st->repetition && st->repetition < ply)
		return st->repetition_type;

	return REPETITION_NONE;
#else
	
	ASSERT_LV3(st->pliesFromNull >= 0);

    
    
    
    bool walk_history = false;
    if (repetition_root_strict)
    {
        const bool table_ok = rep_hist_gen != 0 && rep_hist_gen == history_gen.load(std::memory_order_acquire)
                              && ply == gamePly - history_root_ply.load(std::memory_order_relaxed);
        const int n = table_ok ? history_triples_n.load(std::memory_order_acquire) : -1;
        walk_history = n < 0;
        for (int k = 0; k < n && !walk_history; ++k)
            walk_history = history_triples[k] == st->key();
    }
    
    
    const int pfn = st->pliesFromNull;   
    int end = !repetition_root_strict ? std::min(16, pfn)
            : walk_history ? pfn
                           : std::min(std::min(16, pfn), ply - 1);
    
    if (repetition_root_strict && ply > end)
        end = std::min(end, 16);

	
	if (end < 4)
		return REPETITION_NONE;

	StateInfo* stp = st->previous->previous;

	
	
	
	
	if (!repetition_root_strict || (end < ply && end <= 16))
	{
		const Key  bk = st->board_key;
		
		if (rep_depth - end > rep_lw && rep_cnt[rep_slot(bk)] < 2)
			return REPETITION_NONE;
		const Hand h  = st->hand;
		for (int i = 4; i <= end; i += 2)
		{
			stp = stp->previous->previous;
			if (stp->board_key == bk)
			{
				if (stp->hand == h)
				{
					if (i <= st->continuousCheck[ sideToMove])
						return REPETITION_LOSE;
					if (i <= st->continuousCheck[~sideToMove])
						return REPETITION_WIN;
					return REPETITION_DRAW;
				}
				if (hand_is_equal_or_superior(h, stp->hand))
					return REPETITION_SUPERIOR;
				if (hand_is_equal_or_superior(stp->hand, h))
					return REPETITION_INFERIOR;
			}
		}
		return REPETITION_NONE;
	}

	return is_repetition_walk(ply, end);
#endif
}

#if defined(ENABLE_QUICK_DRAW)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
RepetitionState Position::is_repetition_walk(int ply, int end) const
{
	StateInfo* stp = st->previous->previous;

	
	int history_matches = 0;
	bool past_root = false;

	for (int i = 4; i <= end ; i += 2)
	{
		StateInfo* mid = stp->previous;
		stp = mid->previous;

		
		
		if (repetition_root_strict && rep_root)
		{
			past_root = past_root || mid == rep_root || stp == rep_root;
			if (past_root && i >= ply && history_matches + rep_root_maxocc < 3)
				break;
		}

		
		
		
		
		
		const bool historical = i >= ply;
		const bool in_tree_shortcut = i < ply && i <= 16;
		if (repetition_root_strict && !historical && !in_tree_shortcut)
			continue;
		if (stp->board_key == st->board_key)
		{
			
			if (stp->hand == st->hand)
			{
				
				if (repetition_root_strict && historical && ++history_matches < 3)
					continue;

				
				if (i <= st->continuousCheck[ sideToMove])
					return REPETITION_LOSE;

				
				if (i <= st->continuousCheck[~sideToMove])
					return REPETITION_WIN;

				return REPETITION_DRAW;
			}
			else if (!repetition_root_strict || in_tree_shortcut) {
				
				if (hand_is_equal_or_superior(st ->hand , stp->hand))
					return REPETITION_SUPERIOR;
				if (hand_is_equal_or_superior(stp->hand , st ->hand))
					return REPETITION_INFERIOR;
			}
		}
	}

	
	return REPETITION_NONE;
}
#endif

void Position::prepare_repetition_root()
{
	rep_root = st;
	rep_root_maxocc = 0;
	std::vector<const StateInfo*> chain;
	const StateInfo* s = st;
	for (int i = 0; s && i <= st->pliesFromNull; ++i, s = s->previous)
		chain.push_back(s);
	for (size_t a = 0; a < chain.size() && rep_root_maxocc < 3; ++a)
	{
		int occ = 0;
		for (size_t b = a; b < chain.size(); b += 2)   
			if (chain[b]->board_key == chain[a]->board_key && chain[b]->hand == chain[a]->hand)
				++occ;
		rep_root_maxocc = std::max(rep_root_maxocc, occ);
	}
}

#if !defined(ENABLE_QUICK_DRAW)


bool Position::has_repeated() const {

    StateInfo* stc = st;
    
    int        end = std::min(max_repetition_ply, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}
#endif


void Position::set_repetition_history(const Position& root) {
    history_triples_n.store(0, std::memory_order_release);
    std::unordered_map<Key, int> count;
    const StateInfo* s = root.st;
    for (int k = 0; s && k <= root.st->pliesFromNull; ++k, s = s->previous)
        ++count[s->key()];
    int n = 0;
    for (const auto& [key, c] : count)
        if (c >= 3)
        {
            if (n == kHistoryTriplesMax) { n = -1; break; }
            history_triples[n++] = key;
        }
    history_triples_n.store(n, std::memory_order_release);
    history_root_ply.store(root.game_ply(), std::memory_order_relaxed);
    history_gen.fetch_add(1, std::memory_order_acq_rel);
    if (history_gen.load(std::memory_order_relaxed) == 0)   
        history_gen.fetch_add(1, std::memory_order_acq_rel);
}



RepetitionState Position::is_repetition(int ply, int& found_ply) const
{
#if !defined(ENABLE_QUICK_DRAW)
	
	
    if (st->repetition && st->repetition < ply)
	{
		
		found_ply = abs(st->repetition);
		return st->repetition_type;
	}

	return REPETITION_NONE;
#else
	
	ASSERT_LV3(st->pliesFromNull >= 0);

	
	
	int end = std::min(ply, std::min(max_repetition_ply, st->pliesFromNull));

	found_ply = 0;

	
	if (end < 4)
		return REPETITION_NONE;

	StateInfo* stp = st->previous->previous;

	for (found_ply = 4; found_ply <= end ; found_ply += 2)
	{
		stp = stp->previous->previous;

		
		
		if (stp->board_key == st->board_key)
		{
			
			if (stp->hand == st->hand)
			{
				
				if (found_ply <= st->continuousCheck[ sideToMove])
					return REPETITION_LOSE;

				
				if (found_ply <= st->continuousCheck[~sideToMove])
					return REPETITION_WIN;

				return REPETITION_DRAW;
			}
			else {
				
				if (hand_is_equal_or_superior(st ->hand, stp->hand))
					return REPETITION_SUPERIOR;
				if (hand_is_equal_or_superior(stp->hand, st ->hand))
					return REPETITION_INFERIOR;
			}
		}
	}

	
	return REPETITION_NONE;
#endif
}






void Position::update_entering_point() {

	int points[COLOR_NB];

	switch (ekr)
	{
	case EKR_24_POINT:   
	case EKR_24_POINT_H: 
		points[BLACK] = points[WHITE] = 31;
		break;

	case EKR_27_POINT:   
	case EKR_27_POINT_H: 
		points[BLACK] = 28;
		points[WHITE] = 27;
		break;

    case EKR_NULL :
		
        sync_cout << "ekr == EKR_NULL in update_entering_point()" << sync_endl;
        Tools::exit();
        return;

	default:
		
		return;
	}

	

	
	auto p1 = pieces().pop_count();

	
	auto p2 = pieces(BISHOP_HORSE,ROOK_DRAGON).pop_count();

	
	
	auto p = p1 + p2 * 4;

	
	for (auto c : COLOR )
	{
		auto h = hand[c];
		p += hand_count(h, PAWN) + hand_count(h, LANCE) + hand_count(h, KNIGHT) + hand_count(h, SILVER) + hand_count(h, GOLD)
			+ hand_count(h, BISHOP) * 5 + hand_count(h, ROOK) * 5;
	}
	
	
        if (p != 56 && (ekr == EKR_24_POINT_H || ekr == EKR_27_POINT_H))
	{
		
		

		
		
		
		
		

		
		points[WHITE] -= 56 - p;
	}

	enteringKingPoint[BLACK] = points[BLACK];
	enteringKingPoint[WHITE] = points[WHITE];
}

Move Position::DeclarationWinFull() const
{
    switch (ekr)
	{
		
	case EKR_NONE: return Move::none();

		
		
		
	case EKR_24_POINT: 
	case EKR_27_POINT: 
	case EKR_24_POINT_H: 
	case EKR_27_POINT_H: 
	{
		
		
		

		Color us = sideToMove;

		
		Bitboard ef = enemy_field(us);

		
		if (!(ef & square<KING>(us)))
			return Move::none();

		
		if (checkers())
			return Move::none();


		
		int p1 = (pieces(us) & ef).pop_count();
		
		if (p1 < 11)
			return Move::none();

		
		int p2 = ((pieces(us, BISHOP_HORSE, ROOK_DRAGON)) & ef).pop_count();

		
		

		
		
		
		Hand h = hand[us];
		int score = p1 + p2 * 4 - 1
			+ hand_count(h, PAWN) + hand_count(h, LANCE) + hand_count(h, KNIGHT) + hand_count(h, SILVER)
			+ hand_count(h, GOLD) + (hand_count(h, BISHOP) + hand_count(h, ROOK)) * 5;

		
		
			

		

		if (score < enteringKingPoint[us])
			return Move::none();

		
		return Move::win();
	}

	
	case EKR_TRY_RULE:
	{
		Color us = sideToMove;
		Square king_try_sq = (us == BLACK ? SQ_51 : SQ_59);
		Square king_sq = square<KING>(us);

		
		if (!(kingEffect(king_sq) & king_try_sq))
			return Move::none();

		
		if (pieces(us) & king_try_sq)
			return Move::none();

		
		if (effected_to(~us, king_try_sq, king_sq))
			return Move::none();

		
		return make_move(king_sq, king_try_sq, us,KING);
	}

	default:
		UNREACHABLE;
		return Move::none();
	}
}









void Position::flip() {

#if STOCKFISH
    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_8; r >= RANK_1; --r)  
    {
        std::getline(ss, token, r > RANK_1 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));
    }

    ss >> token;                        
    f += (token == "w" ? "B " : "W ");  

    ss >> token;  
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;  
    f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

    std::getline(ss, token);  
    f += token;

    set(f, is_chess960(), st);

    assert(pos_is_ok());

#else

	auto f_sfen = flipped_sfen();
    set(f_sfen, st);

	ASSERT_LV5(pos_is_ok());

#endif
}















bool Position::pos_is_ok() const
{
	

#if ASSERT_LV >= 5
	
	

	
	const int ptc0[KING] = { 2,18,4,4,4,2,2,4 };
	
	int ptc[PIECE_WHITE] = { 0 };

	int count = 0;
	for (auto sq : SQ)
	{
		Piece pc = piece_on(sq);
		if (pc != NO_PIECE)
		{
			++count;
			++ptc[raw_type_of(pc)];
		}
	}
	for (auto c : COLOR)
		for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr)
		{
			int ct = hand_count(hand[c], pr);
			count += ct;
			ptc[pr] += ct;
		}

	if (count != 40)
		return false;

	
	for (Piece pt = PIECE_ZERO; pt < KING; ++pt)
		if (ptc[pt] != ptc0[pt])
			return false;


#endif
	
	if (st->hand != hand[sideToMove])
		return false;

	
	if (st->checkersBB != attackers_to(~sideToMove, square<KING>(sideToMove)))
		return false;

	
	if (effected_to(sideToMove, square<KING>(~sideToMove)))
		return false;

	
	if ((pieces() != (pieces(BLACK) | pieces(WHITE))) || (pieces(BLACK) & pieces(WHITE)))
		return false;

	
	if (checkers() & pieces(side_to_move()))
		return false;

	

	return true;
}





namespace {
	
	
	u64 perft(Position& pos, Depth depth)
	{
		StateInfo st;
		u64 cnt, nodes = 0;

		auto ml = MoveList<LEGAL_ALL>(pos);
		if (depth == 1)
			return ml.size();

		const bool leaf = (depth == 2);
		for (const auto& m : ml)
		{
			pos.do_move(m, st);
			cnt = leaf ? MoveList<LEGAL_ALL>(pos).size() : perft(pos, depth - 1);
			nodes += cnt;
			pos.undo_move(m);
		}
		return nodes;
	};
}

void Position::UnitTest(Test::UnitTester& tester, IEngine& engine) {
    auto section1 = tester.section("Position");

    Position  pos;
    StateInfo si;

    
    auto pos_init = [&](const std::string& sfen_) { pos.set(sfen_, &si); };

    
    auto hirate_init = [&] { pos.set_hirate(&si); };
    
    auto handi2_sfen = "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1";
    auto handi2_init = [&] { pos.set(handi2_sfen, &si); };

    
    auto handi4_sfen = "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1";
    auto handi4_init = [&] { pos.set(handi4_sfen, &si); };

    
    auto matsuri_sfen = "l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w GR5pnsg 1";
    auto matsuri_init = [&] { pos.set(matsuri_sfen, &si); };

    Move16 m16;
    Move   m;

    
    {
        auto section2 = tester.section("to_move()");

        
        hirate_init();

        
        tester.test("MOVE_NONE", pos.to_move(Move16::none()) == Move::none());
        tester.test("MOVE_WIN", pos.to_move(Move16::win()) == Move::win());
        tester.test("MOVE_NULL", pos.to_move(Move16::null()) == Move::null());

        
        m16 = make_move16(SQ_88, SQ_22);
        tester.test("make_move16(SQ_88, SQ_22)",
                    pos.to_move(m16) == (Move) ((u32) m16.to_u16() + (u32) (B_BISHOP << 16)));

        
        m16 = make_move_promote16(SQ_88, SQ_22);
        tester.test("make_move_promote16(SQ_88, SQ_22)",
                    pos.to_move(m16) == (Move) ((u32) m16.to_u16() + (u32) (B_HORSE << 16)));

        
        m16 = make_move16(SQ_22, SQ_88);
        tester.test("make_move16(SQ_22, SQ_88)", pos.to_move(m16) == Move::none());

        
        m16 = make_move_promote16(SQ_22, SQ_88);
        tester.test("make_move_promote16(SQ_22, SQ_88)", pos.to_move(m16) == Move::none());

        matsuri_init();
        m16 = make_move_drop16(GOLD, SQ_55);
        tester.test("make_move_drop(SQ_55,GOLD)",
                    pos.to_move(m16) == (Move) ((u32) m16.to_u16() + (u32) (W_GOLD << 16)));
    }

    
    {
        auto section2 = tester.section("legality");

        
        hirate_init();

        
        
        m16 = make_move16(SQ_77, SQ_76);
        m   = pos.to_move(m16);
        tester.test("make_move(SQ_77, SQ_76) is pseudo_legal == true",
                    pos.pseudo_legal(m, true) == true);

#if 0
		
		m16 = make_move16(SQ_83, SQ_84);
		m = pos.to_move(m16);
		
		tester.test("make_move(SQ_83, SQ_84) is pseudo_legal == false", pos.pseudo_legal(m) == false);
#endif

        
        
        m16 = make_move16(SQ_88, SQ_22);
        m   = pos.to_move(m16);
        tester.test("make_move(SQ_88, SQ_22) is pseudo_legal == false",
                    pos.pseudo_legal(m, true) == false);
    }

    
    {
        auto section2 = tester.section("attacks_bb");
        hirate_init();
        tester.test("attacks_by<BLACK,PAWN>", pos.attacks_by<BLACK, PAWN>() == BB_Table::RANK6_BB);
        tester.test("attacks_by<WHITE,PAWN>", pos.attacks_by<WHITE, PAWN>() == BB_Table::RANK4_BB);
        tester.test("attacks_by<BLACK,KNIGHT>",
                    pos.attacks_by<BLACK, KNIGHT>()
                      == (Bitboard(SQ_97) | Bitboard(SQ_77) | Bitboard(SQ_37) | Bitboard(SQ_17)));
        tester.test("attacks_by<BLACK,GOLDS>",
                    pos.attacks_by<BLACK, GOLDS>()
                      == (goldEffect<BLACK>(SQ_69) | goldEffect<BLACK>(SQ_49)));
        tester.test("attacks_by<WHITE,GOLDS>",
                    pos.attacks_by<WHITE, GOLDS>()
                      == (goldEffect<WHITE>(SQ_61) | goldEffect<WHITE>(SQ_41)));
    }

#if defined(ENABLE_QUICK_DRAW)
	

    
    {
        auto section2 = tester.section("is_repetition");

        std::deque<StateInfo> sis;

        
        BookTools::feed_position_string(pos, "startpos moves 5i5h 5a5b 5h5i 5b5a", sis);

        int  found_ply;
        auto rep = pos.is_repetition(16, found_ply);

        tester.test("REPETITION_DRAW", rep == REPETITION_DRAW && found_ply == 4);

        StateInfo s[512];
        
        
        pos_init("lnsg1gsnl/1r5b1/ppppppppp/4k4/9/5R3/PPPPPPPPP/1B7/LNSGKGSNL b - 1");

        m = pos.to_move(make_move16(SQ_46, SQ_56));
        pos.do_move(m, s[0]);
        m = pos.to_move(make_move16(SQ_54, SQ_44));
        pos.do_move(m, s[1]);
        m = pos.to_move(make_move16(SQ_56, SQ_46));
        pos.do_move(m, s[2]);
        m = pos.to_move(make_move16(SQ_44, SQ_54));
        pos.do_move(m, s[3]);

        
        auto draw_value = pos.is_repetition();
        tester.test("REPETITION_LOSE", draw_value == REPETITION_LOSE);

        
        
        pos_init("lnsg1gsnl/1r5b1/ppppppppp/4k4/9/4R4/PPPPPPPPP/1B7/LNSGKGSNL w - 1");

        m = pos.to_move(make_move16(SQ_54, SQ_44));
        pos.do_move(m, s[0]);
        m = pos.to_move(make_move16(SQ_56, SQ_46));
        pos.do_move(m, s[1]);
        m = pos.to_move(make_move16(SQ_44, SQ_54));
        pos.do_move(m, s[2]);
        m = pos.to_move(make_move16(SQ_46, SQ_56));
        pos.do_move(m, s[3]);

        draw_value = pos.is_repetition();
        tester.test("REPETITION_WIN", draw_value == REPETITION_WIN);
    }
#endif

    
    {
        auto section2 = tester.section("EnteringKing");

        {
            
            auto section3 = tester.section("EKR_27_POINT");

            hirate_init();
            pos.set_ekr(EKR_27_POINT);

            tester.test("hirate",
                        pos.enteringKingPoint[BLACK] == 28 && pos.enteringKingPoint[WHITE] == 27);

            
            handi2_init();
            pos.set_ekr(EKR_27_POINT);

            tester.test("handi2",
                        pos.enteringKingPoint[BLACK] == 28 && pos.enteringKingPoint[WHITE] == 27);
        }

        {
            
            auto section3 = tester.section("EKR_24_POINT");

            hirate_init();
            pos.set_ekr(EKR_24_POINT);

            tester.test("hirate",
                        pos.enteringKingPoint[BLACK] == 31 && pos.enteringKingPoint[WHITE] == 31);

            
            handi2_init();
            pos.set_ekr(EKR_24_POINT);

			tester.test("handi2",
                        pos.enteringKingPoint[BLACK] == 31 && pos.enteringKingPoint[WHITE] == 31);
        }

        {
            
            auto section3 = tester.section("EKR_27_POINT_H");

            hirate_init();
            pos.set_ekr(EKR_27_POINT_H);

            tester.test("hirate",
                        pos.enteringKingPoint[BLACK] == 28 && pos.enteringKingPoint[WHITE] == 27);

            
            handi2_init();
            pos.set_ekr(EKR_27_POINT_H);
            tester.test("handi2",
                        pos.enteringKingPoint[BLACK] == 28 && pos.enteringKingPoint[WHITE] == 17);

            
            handi4_init();
            pos.set_ekr(EKR_27_POINT_H);
            tester.test("handi4",
                        pos.enteringKingPoint[BLACK] == 28 && pos.enteringKingPoint[WHITE] == 15);
        }

        {
            
            auto section3 = tester.section("EKR_24_POINT_H");

            hirate_init();
            pos.set_ekr(EKR_24_POINT_H);

            tester.test("hirate",
                        pos.enteringKingPoint[BLACK] == 31 && pos.enteringKingPoint[WHITE] == 31);

            
            handi2_init();
            pos.set_ekr(EKR_24_POINT_H);
            tester.test("handi2",
                        pos.enteringKingPoint[BLACK] == 31 && pos.enteringKingPoint[WHITE] == 21);

            
            handi4_init();
            pos.set_ekr(EKR_24_POINT_H);
            tester.test("handi4",
                        pos.enteringKingPoint[BLACK] == 31 && pos.enteringKingPoint[WHITE] == 19);
        }
    }

    {
        
        auto section2 = tester.section("GenMove");

        {
            
            pos_init("lnsgk1snl/1r4g2/p1ppppb1p/6pP1/7R1/2P6/P2PPPP1P/1SG6/LN2KGSNL b BP2p 21");
            Move move1 = make_move(SQ_24, SQ_23, B_PAWN);
            Move move2 = make_move_promote(SQ_24, SQ_23, B_PAWN);

            Move move_buf[MAX_MOVES], *move_last;
            
            auto find_move = [&](Move m) {
                for (Move* em = &move_buf[0]; em != move_last; ++em)
                    if (*em == m)
                        return true;
                return false;
            };

            bool all = true;

            move_last = generate<QUIETS>(pos, move_buf);
            all &= !find_move(move1);
            all &= find_move(move2);

            move_last = generate<CAPTURES>(pos, move_buf);
            all &= !find_move(move1);
            all &= !find_move(move2);

            move_last = generate<NON_EVASIONS>(pos, move_buf);
            all &= !find_move(move1);
            all &= find_move(move2);

            move_last = generate<NON_EVASIONS_ALL>(pos, move_buf);
            all &= find_move(move1);
            all &= find_move(move2);

            move_last = generate<CAPTURES>(pos, move_buf);
            all &= !find_move(move1);
            all &= !find_move(move2);

            move_last = generate<CAPTURES_PRO_PLUS>(pos, move_buf);
            all &= !find_move(move1);
            all &= find_move(move2);

            move_last = generate<CAPTURES_PRO_PLUS_ALL>(pos, move_buf);
            all &= find_move(
              move1);  
            all &= find_move(move2);

            move_last = generate<QUIETS_PRO_MINUS>(pos, move_buf);
            all &= !find_move(move1);
            all &= !find_move(move2);

            move_last = generate<QUIETS_PRO_MINUS_ALL>(pos, move_buf);
            all &= !find_move(move1);  
            all &= !find_move(move2);

            tester.test("pawn's unpromoted move", all);

            
            
            pos_init("5B1n1/8k/6Rpp/9/9/9/1+p7/9/K8 b rb4g4s3n4l15p 1");
            
            move1 = make_move(SQ_41, SQ_23, B_BISHOP);
            move2 = make_move_promote(SQ_41, SQ_23, B_BISHOP);
            all   = true;

            move_last = generate<LEGAL_ALL>(pos, move_buf);
            all &= find_move(move1);
            all &= find_move(move2);

            move_last = generate<CAPTURES_PRO_PLUS>(pos, move_buf);
            all &= !find_move(move1);
            all &= find_move(move2);

            move_last = generate<QUIETS_PRO_MINUS>(pos, move_buf);
            all &= !find_move(move1);
            all &= !find_move(move2);

            move_last = generate<CAPTURES_PRO_PLUS>(pos, move_buf);
            all &= !find_move(move1);
            all &= find_move(move2);

            move_last = generate<QUIETS_PRO_MINUS_ALL>(pos, move_buf);
            all &= !find_move(move1);
            all &= !find_move(move2);

            move_last = generate<CAPTURES_PRO_PLUS_ALL>(pos, move_buf);
            all &= find_move(move1);
            all &= find_move(move2);

            tester.test("bishop's unpromoted move", all);
        }
    }
#if defined(USE_SEE)
    {
        
        auto      section = tester.section("see_ge");
        StateInfo s[512];

        
        hirate_init();

        
        
        auto see_ge_th = [&](int v) {
            Value th     = Value(v);
            bool  all_ok = true;
            all_ok &= pos.see_ge(m, th);       
            all_ok &= !pos.see_ge(m, th + 1);  
            all_ok &= pos.see_ge(m, th - 1);   
            return all_ok;
        };

        
        m = pos.to_move(make_move16(SQ_77, SQ_76));
        pos.do_move(m, s[0]);
        m = pos.to_move(make_move16(SQ_33, SQ_34));
        pos.do_move(m, s[1]);
        
        m = pos.to_move(make_move_promote16(SQ_88, SQ_22));
        

        tester.test("pos1move", see_ge_th(0));

        pos.do_move(m, s[2]);
        
        m = pos.to_move(make_move16(SQ_83, SQ_24));
        pos.do_move(m, s[3]);

        
        m = pos.to_move(make_move16(SQ_22, SQ_31));
        tester.test("pos2move", see_ge_th(-Eval::HorseValue + Eval::SilverValue));

        
        m = pos.to_move(make_move_drop16(BISHOP, SQ_33));
        tester.test("pos2drop", see_ge_th(-Eval::BishopValue + Eval::KnightValue));

        
        m = pos.to_move(make_move16(SQ_22, SQ_33));
        tester.test("pos2move", see_ge_th(-Eval::HorseValue));
    }
#endif

    {
        
        auto section = tester.section("nullmove");
        matsuri_init();
        StateInfo s[512];

        
        pos.do_null_move(s[0]);
        tester.test("pos_is_ok()", pos.pos_is_ok());
    }

    {
        
        auto section = tester.section("PackedSfen");

        vector<string> test_sfens = {
          "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w -",
          "lns1kgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w -",
          "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGK4 w -",
          "lnsgk4/9/ppppppppp/9/9/9/PPPPPPPPP/9/LNSGK4 w GBRgbr",
        };

        
        
        vector<PackedSfen> packed_sfens = {
          {89,  164, 81, 34, 12,  171, 68, 252, 44,  167, 68, 56, 94,  137, 240, 72,
           132, 87,  34, 60, 167, 68,  56, 86,  137, 248, 88, 70, 137, 48,  188, 126},
          {89, 164, 81,  34, 12, 171, 68,  252, 44,  167, 68, 56, 94,  137, 240, 72,
           4,  18,  225, 57, 37, 194, 177, 74,  196, 199, 50, 74, 132, 97,  191, 126},
          {89,  164, 81, 34,  88, 37,  226, 199, 41,  17,  188, 18, 129, 68,  120, 37,
           194, 115, 74, 132, 99, 149, 136, 143, 101, 148, 8,   67, 106, 107, 191, 126},
          {89,  36,  18, 1,  137, 128, 68,  64,  34, 144, 8,   175, 68,  120, 78,  137,
           112, 172, 18, 97, 25,  37,  194, 112, 30, 159, 251, 252, 166, 212, 218, 90}};

        bool success = true;
        for (size_t i = 0; i < test_sfens.size(); ++i)
        {
            auto  sfen        = test_sfens[i];
            auto& packed_sfen = packed_sfens[i];

            StateInfo si;
            pos.set(sfen, &si);

            PackedSfen ps;
            pos.sfen_pack(ps);

            
            success &= ps == packed_sfen;

            
            
        }
        tester.test("handicapped sfen", success);
    }

#if defined(USE_PARTIAL_KEY)
    {
        
        auto section = tester.section("PartialKey");
        {
            PRNG     my_rand(114514);
            Position pos2;

            bool ok = true;

            for (int i = 0; i < 1000; ++i)
            {
                StateInfo si[MAX_PLY];
                pos.set_hirate(&si[0]);
                for (int j = 1; j < MAX_PLY; ++j)
                {
                    MoveList<LEGAL_ALL> ml(pos);

                    
                    if (ml.size() == 0)
                        break;

                    Move m = Move(ml.at(size_t(my_rand.rand(ml.size()))));
                    pos.do_move(m, si[j + 1]);

					
					

					auto      sfen = pos.sfen();
                    StateInfo si2;
                    pos2.set(sfen, &si2);

                    

                    
                    ok &= pos.state()->board_key     == pos2.state()->board_key;
                    ok &= pos.state()->hand_key      == pos2.state()->hand_key;
                    ok &= pos.state()->pawnKey       == pos2.state()->pawnKey;
                    ok &= pos.state()->nonPawnKey[0] == pos2.state()->nonPawnKey[0];
                    ok &= pos.state()->nonPawnKey[1] == pos2.state()->nonPawnKey[1];
#if defined(CYF_MATERIAL_KEY)
                    
                    ok &= pos.state()->materialKey   == pos2.state()->materialKey;
#endif
                    ok &= pos.state()->minorPieceKey == pos2.state()->minorPieceKey;

                    

                    
                    
                }
            }
            tester.test("partial hash key", ok);
        }
    }
#endif

    {
        
        auto section = tester.section("misc");
        {
            

            
            pos_init("lnsgk1snl/1r4g2/p1ppppb1p/6pP1/7R1/2P6/P2PPPP1P/1SG6/LN2KGSNL b BP2p 21");
            auto flipped = pos.flipped_sfen();
            tester.test(
              "flip sfen",
              flipped == "lnsgk2nl/6gs1/p1pppp2p/6p2/1r7/1pP6/P1BPPPP1P/2G4R1/LNS1KGSNL w 2Pbp 21");
        }
    }

    {
        

        auto section2 = tester.section("Perft");

        {
            auto section3 = tester.section("hirate");
            hirate_init();
            const s64 p_nodes[] = {0, 30, 900, 25470, 719731, 19861490, 547581517};

            for (Depth d = 1; d <= 6; ++d)
            {
                u64 nodes = perft(pos, d);
                u64 pn    = p_nodes[d];
                tester.test("depth " + to_string(d) + " = " + to_string(pn),
                            nodes == pn && pos.pos_is_ok());
            }
        }

        {
            auto section3 = tester.section("matsuri");
            matsuri_init();

            const s64 p_nodes[] = {0, 207, 28684, 4809015, 516925165};

            for (Depth d = 1; d <= 4; ++d)
            {
                u64 nodes = perft(pos, d);
                u64 pn    = p_nodes[d];
                tester.test("depth " + to_string(d) + " = " + to_string(nodes),
                            nodes == pn && pos.pos_is_ok());
            }
        }
    }

    

    
    auto extra_test1 = [&](Position& pos) {
        PackedSfen ps;
        StateInfo  si;
        string     sfen     = pos.sfen();
        int        game_ply = pos.game_ply();
        pos.sfen_pack(ps);

        Position pos2;
        pos2.set_from_packed_sfen(ps, &si);
        string sfen2 = pos2.sfen(game_ply);

        return sfen == sfen2;
    };

    
    auto extra_test2 = [&](Position& pos) {
        PackedSfen ps;
        StateInfo  si;
        string     sfen     = pos.sfen();
        int        game_ply = pos.game_ply();
        pos.sfen_pack(ps);

        Position pos2;
        pos2.set_from_packed_sfen(ps, &si);
        
        int count = 0;
        for (auto sq : SQ)
        {
            auto pc = pos2.piece_on(sq);
            if (pc != NO_PIECE && type_of(pc) != KING)
            {
                pos2.board[sq] = NO_PIECE;  
                if (++count >= 5)
                    break;
            }
        }
        string sfen2 = pos2.sfen(game_ply);
        pos2.sfen_pack(ps);  

        Position pos3;
        pos3.set_from_packed_sfen(ps, &si);

        string sfen3 = pos3.sfen(game_ply);

        return sfen2 == sfen3;
    };

    {
        
        s64 random_player_loop = tester.options["random_player_loop"];
        if (random_player_loop)
        {
            auto section2 = tester.section("GamesOfRandomPlayer");

            
            PRNG      my_rand(114514);
            StateInfo s[512];

            for (s64 i = 0; i < random_player_loop; ++i)
            {
                
                hirate_init();
                bool fail = false;

                
                for (int ply = 0; ply < 512; ++ply)
                {
                    MoveList<LEGAL_ALL> ml(pos);

                    
                    if (ml.size() == 0)
                        break;

                    Move m = Move(ml.at(size_t(my_rand.rand(ml.size()))));

                    pos.do_move(m, s[ply]);

                    if (!pos.pos_is_ok() || !extra_test1(pos) || !extra_test2(pos))
                        fail = true;
                }

                
                tester.test(string("game ") + to_string(i + 1), !fail);
            }
        }
    }
}





template bool Position::pseudo_legal_s<false>(const Move m) const;
template bool Position::pseudo_legal_s< true>(const Move m) const;

template void Position::do_move(Move m, StateInfo& newSt, bool givesCheck, const TranspositionTable* tt);
template void Position::do_move(Move m, StateInfo& newSt, bool givesCheck, const void* tt);

template void Position::do_null_move(StateInfo& st, const TranspositionTable& tt);
template void Position::do_null_move(StateInfo& st, const int& tt);

} 
