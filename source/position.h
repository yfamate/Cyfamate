#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <array>
#include <deque>
#include <memory> 
#include <optional>
#include <stdexcept>

#include "bitboard.h"
#include "evaluate.h"
#include "types.h"

#if defined(EVAL_NNUE)
#include "eval/nnue/nnue_accumulator.h"
#endif

#include "extra/key128.h"
#include "extra/long_effect.h"
#include "misc.h"

namespace YaneuraOu {


















struct StateInfo {

	
    

#if defined(USE_PARTIAL_KEY)
	
    
    Key materialKey;

	
    Key pawnKey;

	
	
    Key minorPieceKey;
    Key majorPieceKey;   

	
    Key nonPawnKey[COLOR_NB];
#endif

#if STOCKFISH
	
	
    Value nonPawnMaterial[COLOR_NB];

    int castlingRights;
    int rule50;
#endif

	
	int pliesFromNull;

#if STOCKFISH
    Square epSquare;
#else
    
    int continuousCheck[COLOR_NB];
#endif

#if defined(SFNNwoPSQT) && defined(NNUE_SFNN_PROGRESS_BUCKETS) && NNUE_SFNN_PROGRESS_BUCKETS != 1

#endif


#if defined(CYFAMATE_NNUE_5GP)
	
	
	double gp_s;
#endif

	

	
	

#if STOCKFISH
	Key        key;
#else
	
	

	

	Key board_key;
    Key hand_key;
#if defined(CYFAMATE_NNUE_5GP)
    
    
    mutable double gp_value;
    mutable std::uint8_t gp_king_code;
    mutable std::uint8_t gp_bucket;
    mutable bool gp_value_valid;
#endif
#if defined(CYF_GPU_POOL)
    
    
    std::uint8_t cyf_pool_req;
#endif

	
	
	
	
    

	Key key() const { return board_key ^ hand_key; }

#endif

	
	Bitboard checkersBB;

	
	
	
	
	
	
	
	
	
	StateInfo* previous;

	
	
	
	
	

	
	
	Bitboard blockersForKing[COLOR_NB];

	
	
	
	Bitboard pinners[COLOR_NB];

	
	Bitboard checkSquares[PIECE_TYPE_NB];

	
    
    Piece capturedPiece;

    
    
    bool checkSquaresReady;

#if !defined(ENABLE_QUICK_DRAW)
    
    
    
    
    int repetition;

#if !STOCKFISH
    
    
	
    int repetition_times;

    
    RepetitionState repetition_type;
#endif

#endif


#if !STOCKFISH
	
	Hand hand;
#endif

	

#if defined(USE_PIECE_VALUE)
        
        Value materialValue;
#endif

#if defined(USE_CLASSIC_EVAL)

#if defined(EVAL_KPPT) || defined(EVAL_KPP_KKPT)

	
	
	Eval::EvalSum sum;

#endif

#if defined(EVAL_NNUE)
	Eval::NNUE::Accumulator accumulator;
#endif

#if defined (USE_EVAL_LIST)
	
	Eval::DirtyPiece dirtyPiece;
#endif

#if defined(KEEP_LAST_MOVE)
	
	Move lastMove;

	
	PieceType lastMovedPieceType;
#endif

#endif
};













using StateList    = std::deque<StateInfo>;
using StateListPtr = std::unique_ptr<StateList>;



struct PositionSetError : std::runtime_error {
	using std::runtime_error::runtime_error;
};






struct PackedSfen {
	u8 data[32];

	
	
	void flip();
	PackedSfen flipped() const;

	
	Color color() const {
		
		return Color(data[0] & 1);
	}

	

	bool operator==(const PackedSfen& rhs) const {
		static_assert(sizeof(PackedSfen) % sizeof(u64) == 0);

		for (size_t i = 0; i < sizeof(PackedSfen); i += sizeof(size_t))
		{
			
			if (*(u64*)&data[i] != *(u64*)&rhs.data[i])
				return false;
		}
		return true;
	}

	bool operator!=(const PackedSfen& rhs) const {
		return !(this->operator==(rhs));
	}
};



struct PsvRecord {
	PackedSfen sfen;
	s16        score;
	u16        move;
	u16        gamePly;
	s8         game_result;
	u8         padding;
};

static_assert(sizeof(PsvRecord) == 40, "PsvRecord must be 40 bytes");



struct PackedSfenHash {
	size_t operator()(const PackedSfen& ps) const
	{
		static_assert(sizeof(PackedSfen) % sizeof(size_t) == 0);

		size_t s = 0;
		for (size_t i = 0; i < sizeof(PackedSfen) ; i+= sizeof(size_t))
		{
			
			s ^= *(size_t*)&ps.data[i];
		}
		return s;
	}
};


class Position
{
public:
    
    
    static void init();

	

	Position()                           = default;
	Position(const Position&)            = delete;
	Position& operator=(const Position&) = delete;

    
    
	
    

    
    
#if STOCKFISH
	Position& set(const std::string& fenStr,bool isChess960, StateInfo* si);
    Position& set(const std::string& code, Color c, StateInfo* si);
#else
    std::optional<PositionSetError> set(const std::string& sfenStr, StateInfo* si);

	
    
    std::optional<PositionSetError> set_hirate(StateInfo* si) { return set(StartSFEN, si); }
#endif

	
	
	
	
	

#if STOCKFISH
    std::string fen() const;
#else
	const std::string sfen() const { return sfen(game_ply()); }
	const std::string sfen(int gamePly) const;

	
	const std::string flipped_sfen() const { return flipped_sfen(game_ply()); }
	const std::string flipped_sfen(int gamePly) const;

	
	static const std::string sfen_to_flipped_sfen(std::string sfen);

#endif


	
	Hand hand_of(Color c) const { ASSERT_LV3(is_ok(c));  return hand[c]; }

	
	template <Color C>
	Hand hand_of() const { ASSERT_LV3(is_ok(C));  return hand[C]; }


	
	
	
	
	Piece moved_piece_before(Move m) const
	{
		ASSERT_LV3(m.is_ok());
#if defined( KEEP_PIECE_IN_GENERATE_MOVES)
		
		
		
		
		static_assert(MOVE_PROMOTE == (1 << 15) && PIECE_PROMOTE == 8, "");
		return (Piece)((m ^ ((m & MOVE_PROMOTE) << 4)) >> 16);

#else
		return m.is_drop() ? make_piece(sideToMove , m.move_dropped_piece()) : piece_on(m.from_sq());
#endif
	}

	
	
	
	Piece moved_piece_after(Move m) const
	{
		
		

		return m.moved_after_piece();
	}

	
    
    
	
	
    Piece moved_piece(Move m) const;


	
	
	
	
	
	
	
	Move to_move(Move16 m) const;

	
	
	
	
	
	
	
	
	void set_max_repetition_ply(int ply){ max_repetition_ply = ply;}

	
    
    
    

	
    

	
	Bitboard pieces() const;  

	
	template<typename... PieceTypes>
	Bitboard pieces(PieceTypes... pts) const;

	
    
    Bitboard pieces(Color c) const;

	
    template<typename... PieceTypes>
    Bitboard pieces(Color c, PieceTypes... pts) const;

	
    
    Piece piece_on(Square sq) const;

	
#if STOCKFISH
    const std::array<Piece, SQUARE_NB>& piece_array() const;
#else
    const std::array<Piece, SQ_NB_PLUS1>& piece_array() const;
#endif

#if STOCKFISH
    Square ep_square() const;
#else
	
    Bitboard empties() const { return pieces() ^ Bitboard(1); }
#endif

	
    bool empty(Square s) const;

#if STOCKFISH
	
    template<PieceType Pt>
    int count(Color c) const;

	
    template<PieceType Pt>
    int count() const;
#endif

	
    
	
    template<PieceType Pt>
    Square square(Color c) const;

	
    
    
    

	
#if STOCKFISH
    bool   can_castle(CastlingRights cr) const;
    bool   castling_impeded(CastlingRights cr) const;
    Square castling_rook_square(CastlingRights cr) const;
#endif

	
    
	
    

	
	Bitboard checkers() const { return st->checkersBB; }

	
	Bitboard blockers_for_king(Color c) const { return st->blockersForKing[c]; }

	
	template <Color C>
	Bitboard blockers_for_king() const { return st->blockersForKing[C]; }

	
	Bitboard check_squares(PieceType pt) const { ASSERT_LV3(pt!= NO_PIECE_TYPE && pt < PIECE_TYPE_NB); ensure_check_squares(); return st->checkSquares[pt]; }
	
	
	
	
	const Bitboard* check_squares_all() const { ensure_check_squares(); return st->checkSquares; }
	
	
	void fill_check_squares() const { ensure_check_squares(); }

	
	
	
	Bitboard pinners(Color c) const { return st->pinners[c]; }

#if !STOCKFISH
	
	bool is_discovery_check_on_king(Color c, Move m) const { return st->blockersForKing[c] & m.from_sq(); }

	
	
	
    bool in_check() const { return checkers(); }
#endif

    
    
	
    

	
	
	

	Bitboard attackers_to(Square sq) const { return attackers_to(sq, pieces()); }
    Bitboard attackers_to(Square sq, const Bitboard& occ) const;

#if STOCKFISH
    
    bool attackers_to_exist(Square s, Bitboard occupied, Color c) const;

#else

	Bitboard attackers_to(Color c, Square sq) const {
        return c == BLACK ? attackers_to<BLACK>(sq, pieces()) : attackers_to<WHITE>(sq, pieces());
    }
    Bitboard attackers_to(Color c, Square sq, const Bitboard& occ) const {
        return c == BLACK ? attackers_to<BLACK>(sq, occ) : attackers_to<WHITE>(sq, occ);
    }

    template<Color C>
    Bitboard attackers_to(Square sq) const {
        return attackers_to<C>(sq, pieces());
    }

    template<Color C>
    Bitboard attackers_to(Square sq, const Bitboard& occ) const;

    
    Bitboard attackers_to_pawn(Color c, Square pawn_sq) const;

    
    
#if !defined(LONG_EFFECT_LIBRARY)
    bool effected_to(Color c, Square sq) const { return attackers_to(c, sq, pieces()); }
    bool effected_to(Color c, Square sq, Square kingSq) const {
        return attackers_to(c, sq, pieces() ^ kingSq);
    }
#else
    bool effected_to(Color c, Square sq) const { return board_effect[c].effect(sq) != 0; }
    bool effected_to(Color c, Square sq, Square kingSq) const {
        return board_effect[c].effect(sq) != 0
            || ((long_effect.directions_of(c, kingSq) & Effect8::directions_of(kingSq, sq))
                != 0);  
    }
#endif

#endif

	
    
    

    
    
    
    

	
    
    

    void update_slider_blockers(Color c) const { update_slider_blockers(c, st); }
    
    
    void update_slider_blockers(Color c, StateInfo* si) const;

	
	template<Color C , PieceType Pt> Bitboard attacks_by() const;

    
	
	
    

	
	
	
	
	
	
    
#if STOCKFISH
    void do_move(Move m, StateInfo& newSt, bool givesCheck, const TranspositionTable* tt);
#else
	template <typename T = void>
    void do_move(Move m, StateInfo& newSt, bool givesCheck, const T* tt = nullptr);
#endif

	
	
    template<typename T = void>
    void do_move(Move m, StateInfo& newSt, const T* tt = nullptr) {
        do_move(m, newSt, gives_check(m), tt);
    }

	
	void undo_move(Move m);

	
	

	void do_null_move(StateInfo& st);

	template<typename T>
    void do_null_move(StateInfo& st, const T& tt);

	
	void undo_null_move();

	

	
	
	
	
	
	
	
	
	
	
	
	
	bool legal(Move m) const;

	
    
    bool pseudo_legal(const Move m, bool generate_all_legal_moves) const;

	
	template <bool All> bool pseudo_legal_s(const Move m) const;

	
	
	
	bool legal_drop(const Square to) const;

	
	bool legal_pawn_drop(const Color us, const Square to) const;

	
	
	
	
	
	bool legal_promote(Move m) const;

	

#if defined(USE_EVAL_LIST)
	
	const Eval::EvalList* eval_list() const { return &evalList; }
#endif

#if defined (USE_SEE)
	
	
	
	bool see_ge(Move m, Value threshold = VALUE_ZERO) const;
	
	bool see_ge_exchange(Square from, Square to, int swap) const;

#endif

    
    
	
    

	
	

    Key key() const;
    Key material_key() const;
    Key pawn_key() const;
    Key minor_piece_key() const;
    Key non_pawn_key(Color c) const;

#if !STOCKFISH
	
	
	
	
	Key key_after(Move m) const;
#endif

    
    
    
    

    
    Color side_to_move() const;

    
    
    int game_ply() const;

#if STOCKFISH
    bool is_chess960() const;
    bool is_draw(int ply) const;
    bool is_repetition(int ply) const;
    bool upcoming_repetition(int ply) const;
    bool has_repeated() const;
    int  rule50_count() const;

#else

    
    
    
    
    
    
    
    
    
    RepetitionState is_repetition(int ply = 16) const;
    
    
    RepetitionState is_repetition_walk(int ply, int end) const;

    
    
    
    
    
    static inline bool repetition_root_strict = true;

    
    
    
    
    
    
    
    static constexpr int kRepSlots = 4096;
    static size_t rep_slot(const Key& k) { return size_t(uint64_t(k) >> 1) & (kRepSlots - 1); }
    void rep_enter() { ++rep_cnt[rep_slot(st->board_key)]; ++rep_depth; }
    void rep_leave() { auto& c = rep_cnt[rep_slot(st->board_key)]; c -= (c != 0); if (--rep_depth < rep_lw) rep_lw = rep_depth; }
    uint16_t rep_cnt[kRepSlots];
    int rep_depth;
    int rep_lw;

    
    
    
    
    
    
    
    
    static constexpr int kHistoryTriplesMax = 16;
    static inline std::atomic<int> history_triples_n{0};
    static inline Key history_triples[kHistoryTriplesMax];
    static inline std::atomic<uint32_t> history_gen{0};
    static inline std::atomic<int> history_root_ply{0};
    static void set_repetition_history(const Position& root);
    void bind_repetition_history() { rep_hist_gen = history_gen.load(std::memory_order_acquire); }
    uint32_t rep_hist_gen = 0;

    
    
    
    
    const StateInfo* rep_root = nullptr;
    int rep_root_maxocc = 0;
    void prepare_repetition_root();

    
    
    RepetitionState is_repetition(int ply, int& found_ply) const;

#if !defined(ENABLE_QUICK_DRAW)
    
    
    bool has_repeated() const;
#endif
#endif

#if STOCKFISH
	
    Value non_pawn_material(Color c) const;
    Value non_pawn_material() const;
#endif

    
	
	

	bool pos_is_ok() const;

	
	void flip();

    
    
    StateInfo* state() const { return st; }

	
    
    

    
    

    void put_piece(Piece pc, Square sq);

    
    void remove_piece(Square sq);

#if !STOCKFISH
	
    
    void put_hand_piece(Color c, PieceType pt);

	
    
    void remove_hand_piece(Color c, PieceType pt);

    
    
    void update_bitboards();

    
    
    
    void update_kingSquare();
#endif

	

	
	
	
	Bitboard pinned_pieces(Color c) const { ASSERT_LV3(is_ok(c)); return st->blockersForKing[c] & pieces(c); }

	
	template<Color C>
	Bitboard pinned_pieces() const { ASSERT_LV3(is_ok(C)); return st->blockersForKing[C] & pieces<C>(); }

	
	
	Bitboard pinned_pieces(Color c, Square avoid) const { return c == BLACK ? pinned_pieces<BLACK>(avoid) : pinned_pieces<WHITE>(avoid); }

	template<Color C>
	Bitboard pinned_pieces(Square avoid) const;

	
	
	Bitboard pinned_pieces(Color c, Square from, Square to) const
	{
		return c == BLACK ? pinned_pieces<BLACK>(from,to) : pinned_pieces<WHITE>(from,to);
	}

	
	template<Color C>
	Bitboard pinned_pieces(Square from, Square to) const;


	
	
	
	bool gives_check(Move m) const;
	
	
	
	bool gives_check_ready(Move m) const;

	
	bool discovered(Square from, Square to, Square ourKing, const Bitboard& pinned) const
	{
		
		
		
		return pinned                        
			&& (pinned & from)               
			&& !aligned(from, to , ourKing); 
	}

	
	bool is_mated() const;

	
	Piece captured_piece() const { return st->capturedPiece; }

	
	bool capture_or_promotion(Move m) const { return m.is_promote() || capture(m); }

	
	bool pawn_promotion(Move m) const
	{
		
		return (m.is_promote() && raw_type_of(moved_piece_after(m)) == PAWN);
	}

	
	bool capture_or_pawn_promotion(Move m) const
	{
		return pawn_promotion(m) || capture(m);
	}

	
	bool capture_or_valuable_promotion(Move m) const
	{
		
		auto pr = raw_type_of(moved_piece_after(m));
		return (m.is_promote() && (pr == PAWN || pr == BISHOP || pr == ROOK)) || capture(m);
	}

	
    bool capture(Move m) const;

	
	
    bool capture_stage(Move m) const;

	
    
	
	
	Move DeclarationWin() const {
		if (ekr >= EKR_24_POINT && ekr <= EKR_27_POINT_H && !(enemy_field(sideToMove) & square<KING>(sideToMove)))
			return Move::none();
		return DeclarationWinFull();
	}
	Move DeclarationWinFull() const;

	
	
	
    void set_ekr(EnteringKingRule ekr) {
        this->ekr = ekr;
        update_entering_point();
    }

	
  
  
	void sfen_pack(PackedSfen& sfen);

	
	
	static std::string sfen_unpack(const PackedSfen& sfen);

	
	
	
	
	Tools::Result set_from_packed_sfen(const PackedSfen& sfen , StateInfo * si , bool mirror=false , int gamePly_ = 0);

	
	static std::string sfen_from_rawdata(Piece board[81], Hand hands[2], Color turn, int gamePly);

	
#if defined(LONG_EFFECT_LIBRARY)

	
	LongEffect::ByteBoard board_effect[COLOR_NB];

	
#if defined(USE_BOARD_EFFECT_PREV)
	

	
	
	
	
	LongEffect::ByteBoard board_effect_prev[COLOR_NB];
#endif

	
	LongEffect::WordBoard long_effect;

#endif

	

#if defined(KEEP_LAST_MOVE)
  
	std::string moves_from_start() const { return moves_from_start(false); }
	std::string moves_from_start_pretty() const { return moves_from_start(true); }
	std::string moves_from_start(bool is_pretty) const;
#endif

	
	friend std::ostream& operator<<(std::ostream& os, const Position& pos);

	
	static void UnitTest(Test::UnitTester& tester, IEngine& engine);

private:
    
	 

#if STOCKFISH
	void set_castling_right(Color c, Square rfrom);
#endif

	
	void set_state() const;

#if STOCKFISH
    void set_check_info() const;
#else
	
	
	template <bool doNullMove,Color Us>
	void set_check_info() const;

	template <bool doNullMove>
	void set_check_info() const
	{
		sideToMove == BLACK ? set_check_info<doNullMove, BLACK>() : set_check_info<doNullMove, WHITE>();
	}

	
	
	template <Color Us>
	void set_check_squares() const;

	void ensure_check_squares() const
	{
		if (!st->checkSquaresReady)
			sideToMove == BLACK ? set_check_squares<BLACK>() : set_check_squares<WHITE>();
	}
#endif

#if STOCKFISH
	
    void move_piece(Square from, Square to);
    template<bool Do>
    void do_castling(Color             us,
                     Square            from,
                     Square&           to,
                     Square&           rfrom,
                     Square&           rto,
                     DirtyPiece* const dp = nullptr);
    Key  adjust_key50(Key k) const;

#endif

	
    template<Color Us, typename T>
    void do_move_impl(Move m, StateInfo& st, bool givesCheck, const T* tt);

	
    template<Color Us>
    void undo_move_impl(Move m);

	
	
	

	void update_entering_point();
    EnteringKingRule ekr = EKR_NULL;
	int enteringKingPoint[COLOR_NB];


#if defined (USE_EVAL_LIST)
	

	
	Eval::BonaPiece bona_piece_of(Color c, PieceType pt) const {
		
		int ct = hand_count(hand[c], pt);
		ASSERT_LV3(ct > 0);
		return (Eval::BonaPiece)(Eval::kpp_hand_index[c][pt].fb + ct - 1);
	}

	
	PieceNumber piece_no_of(Color c, PieceType pt) const { return evalList.piece_no_of_hand(bona_piece_of(c, pt)); }

	
	PieceNumber piece_no_of(Square sq) const
	{
		ASSERT_LV3(piece_on(sq) != NO_PIECE);
		PieceNumber n = evalList.piece_no_of_board(sq);
		ASSERT_LV3(is_ok(n));
		return n;
	}
#else
	
	PieceNumber piece_no_of(Color c, Piece pt) const { return PIECE_NUMBER_ZERO; }
	PieceNumber piece_no_of(Piece pc, Square sq) const { return PIECE_NUMBER_ZERO; }
	PieceNumber piece_no_of(Square sq) const { return PIECE_NUMBER_ZERO; }
#endif

	
    
    

	
    std::array<Piece, SQ_NB_PLUS1>    board;

	
    
    std::array<Bitboard, PIECE_BB_NB> byTypeBB;

    
    std::array<Bitboard, COLOR_NB>    byColorBB;

#if STOCKFISH
    
	
    int pieceCount[PIECE_NB];
	

    int      castlingRightsMask[SQUARE_NB];
    Square   castlingRookSquare[CASTLING_RIGHT_NB];
    Bitboard castlingPath[CASTLING_RIGHT_NB];
#endif

	
    
    
    
    StateInfo* st;

	
    int gamePly;

    
    Color sideToMove;

#if STOCKFISH
    bool chess960;
#else
    
    Hand hand[COLOR_NB];

    
    Square kingSquare[COLOR_NB];

	
    static int max_repetition_ply ;

#if defined(USE_EVAL_LIST)
    
    Eval::EvalList evalList;
#endif

#endif
};



std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const { return sideToMove; }

inline Piece Position::piece_on(Square s) const {
    ASSERT_LV3(is_ok(s));
    return board[s];
}

inline const std::array<Piece, SQ_NB_PLUS1>& Position::piece_array() const { return board; }

inline bool Position::empty(Square s) const { return piece_on(s) == NO_PIECE; }

inline Piece Position::moved_piece(Move m) const {
#if STOCKFISH
    return piece_on(m.from_sq());
#else
    
    
    

	return moved_piece_after(m);
#endif
}

inline Bitboard Position::pieces() const { return byTypeBB[ALL_PIECES]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(PieceTypes... pts) const {
	
    return (byTypeBB[pts] | ...);
}

inline Bitboard Position::pieces(Color c) const { return byColorBB[c]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(Color c, PieceTypes... pts) const {
    return pieces(c) & pieces(pts...);
}

#if STOCKFISH
template<PieceType Pt>
inline int Position::count(Color c) const {
    return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt>
inline int Position::count() const {
    return count<Pt>(WHITE) + count<Pt>(BLACK);
}

template<PieceType Pt>
inline Square Position::square(Color c) const {
    assert(count<Pt>(c) == 1);
    return lsb(pieces(c, Pt));
}
#else
template<PieceType Pt>
inline Square Position::square(Color c) const {
    
    static_assert(Pt == KING);

    
    return kingSquare[c];
}
#endif

#if STOCKFISH
inline Square Position::ep_square() const { return st->epSquare; }

inline bool Position::can_castle(CastlingRights cr) const { return st->castlingRights & cr; }

inline bool Position::castling_impeded(CastlingRights cr) const {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return pieces() & castlingPath[cr];
}

inline Square Position::castling_rook_square(CastlingRights cr) const {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return castlingRookSquare[cr];
}
#endif




#if STOCKFISH
inline Key Position::key() const { return adjust_key50(st->key); }

inline Key Position::adjust_key50(Key k) const {
    return st->rule50 < 14 ? k : k ^ make_key((st->rule50 - 14) / 8);
}
#else
inline Key Position::key() const { return st->key(); }
#endif

#if defined(USE_PARTIAL_KEY)
inline Key Position::pawn_key() const { return st->pawnKey; }

#if defined(CYF_MATERIAL_KEY)


inline Key Position::material_key() const { return st->materialKey; }
#endif

inline Key Position::minor_piece_key() const { return st->minorPieceKey; }

inline Key Position::non_pawn_key(Color c) const { return st->nonPawnKey[c]; }
#else



inline Key Position::pawn_key() const { return Key(); }
inline Key Position::material_key() const { return Key(); }
inline Key Position::minor_piece_key() const { return Key(); }
inline Key Position::non_pawn_key(Color c) const { return Key(); }

#endif

#if STOCKFISH
inline Value Position::non_pawn_material(Color c) const { return st->nonPawnMaterial[c]; }

inline Value Position::non_pawn_material() const {
    return non_pawn_material(WHITE) + non_pawn_material(BLACK);
}
#endif

inline int Position::game_ply() const { return gamePly; }

#if STOCKFISH
inline int Position::rule50_count() const { return st->rule50; }

inline bool Position::is_chess960() const { return chess960; }

inline bool Position::capture(Move m) const {
    assert(m.is_ok());
    return (!empty(m.to_sq()) && m.type_of() != CASTLING) || m.type_of() == EN_PASSANT;
}
#else
inline bool Position::capture(Move m) const {
    ASSERT_LV3(m.is_ok());
    return !m.is_drop() && piece_on(m.to_sq()) != NO_PIECE;
}
#endif











inline bool Position::capture_stage(Move m) const
{
#if STOCKFISH
    assert(is_ok(m));
    return  capture(m) || promotion_type(m) == QUEEN;
#else

    
    
    
    

    return capture(m);
#endif
}


inline void Position::put_piece(Piece pc, Square s) {

	board[s] = pc;

    
	
	
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;

	
    byColorBB[color_of(pc)] |= s;

#if STOCKFISH
	
    pieceCount[pc]++;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
#endif
}


inline void Position::remove_piece(Square s) {

	Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
#if STOCKFISH
    pieceCount[pc]--;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
#endif
}

#if !STOCKFISH


inline void Position::put_hand_piece(Color c, PieceType pr)
{
	add_hand(hand[c], pr);
    
    
}


inline void Position::remove_hand_piece(Color c, PieceType pr)
{
	sub_hand(hand[c], pr);
    
    
}

inline bool is_ok(Position& pos) { return pos.pos_is_ok(); }
#endif






template<Color C>
inline Bitboard Position::attackers_to(Square sq, const Bitboard& occ) const {
    ASSERT_LV3(is_ok(C) && sq <= SQ_NB);

    constexpr Color Them = ~C;

    

	
	
	
	
	
	
	const Bitboard us = pieces(C);
	Bitboard bb =
		(     (pawnEffect  <Them>(sq)	&  pieces(PAWN)        )
			| (knightEffect<Them>(sq)	&  pieces(KNIGHT)      )
			| (silverEffect<Them>(sq)	&  pieces(SILVER_HDK)  )
			| (goldEffect  <Them>(sq)	&  pieces(GOLDS_HDK)   )
			
			
			) & us; 

	const Bitboard bh = pieces(BISHOP_HORSE) & us;
	if (bishopStepEffect(sq) & bh)
		bb |= bishopEffect(sq, occ) & bh;

	const Bitboard rl = (pieces(ROOK_DRAGON) | (lanceStepEffect<Them>(sq) & pieces(LANCE))) & us;
	if (rookStepEffect(sq) & rl)
		bb |= rookEffect(sq, occ) & rl;

	return bb;
    
}










template <Color C>
FORCE_INLINE bool has_attacker_impl(const Position& pos, Square sq, const Bitboard& occ, const Bitboard& exclude)
{
	constexpr Color Them = ~C;
	const Bitboard ours = exclude.andnot(pos.pieces(C));
	if (  ((pawnEffect  <Them>(sq) & pos.pieces(PAWN)      )
		|  (knightEffect<Them>(sq) & pos.pieces(KNIGHT)    )
		|  (silverEffect<Them>(sq) & pos.pieces(SILVER_HDK))
		|  (goldEffect  <Them>(sq) & pos.pieces(GOLDS_HDK) )) & ours)
		return true;
	const Bitboard bh = bishopStepEffect(sq) & pos.pieces(BISHOP_HORSE) & ours;
	if (bh && (bishopEffect(sq, occ) & bh))
		return true;
	const Bitboard rl = rookStepEffect(sq) & (pos.pieces(ROOK_DRAGON) | (lanceStepEffect<Them>(sq) & pos.pieces(LANCE))) & ours;
	return rl && (rookEffect(sq, occ) & rl);
}
template <Color C>
FORCE_INLINE bool has_attacker(const Position& pos, Square sq, const Bitboard& occ, const Bitboard& exclude)
{
	const bool r = has_attacker_impl<C>(pos, sq, occ, exclude);
	ASSERT_LV3(r == bool(pos.attackers_to<C>(sq, occ) & ~exclude));   
	return r;
}



template<Color C, PieceType Pt>
Bitboard Position::attacks_by() const {
    if constexpr (Pt == PAWN)
        return C == WHITE ? pawn_attacks_bb<WHITE>(pieces(WHITE, PAWN))
                          : pawn_attacks_bb<BLACK>(pieces(BLACK, PAWN));
    else
    {
        Bitboard threats   = Bitboard(ZERO);
        Bitboard attackers = pieces(C, Pt);
        while (attackers)
            threats |= attacks_bb<make_piece(C, Pt)>(attackers.pop(), pieces());
        return threats;
    }
}



template<Color C>
Bitboard Position::pinned_pieces(Square avoid) const {
    

	Bitboard b, pinners, result = Bitboard(ZERO);
    Square   ksq = square<KING>(C);

	
	Bitboard avoid_bb = ~Bitboard(avoid);

	pinners = (
		(  pieces(ROOK_DRAGON)   & rookStepEffect    (ksq))
		| (pieces(BISHOP_HORSE)  & bishopStepEffect  (ksq))
		| (pieces(LANCE)         & lanceStepEffect<C>(ksq)))
                & avoid_bb & pieces(~C);

	while (pinners)
	{
		b = between_bb(ksq, pinners.pop()) & pieces() & avoid_bb;
		if (!b.more_than_one())
            result |= b & pieces(C);
	}
	return result;

    
}



template<Color C>
Bitboard Position::pinned_pieces(Square from, Square to) const {
    

	Bitboard b, pinners, result = Bitboard(ZERO);
    Square   ksq = square<KING>(C);

	
	Bitboard avoid_bb = ~Bitboard(from);

	pinners = (
		(  pieces(ROOK_DRAGON)  & rookStepEffect    (ksq))
		| (pieces(BISHOP_HORSE) & bishopStepEffect  (ksq))
		| (pieces(LANCE)        & lanceStepEffect<C>(ksq)))
            & avoid_bb & pieces(~C);

	
	Bitboard new_pieces = (pieces() & avoid_bb) | to;
	while (pinners)
	{
		b = between_bb(ksq, pinners.pop()) & new_pieces;
		if (!b.more_than_one())
			result |= b & pieces(C);
	}
	return result;

    
}



} 

#endif 
