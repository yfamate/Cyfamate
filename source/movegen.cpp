
#include "types.h"
#include "position.h"
#include "movegen.h"

#include <iostream>


using namespace std;
namespace YaneuraOu {

using namespace BB_Table;

#if !STOCKFISH
std::ostream& operator<<(std::ostream& os, ExtMove m)
{
    os << Move(m) << '(' << m.value << ')';
    return os;
}
#endif

bool pseudo_legal_check(const Position& pos, Move* mlist_start, Move* mlist_end)
{
	bool all_ok = true;

	for (auto it = mlist_start; it != mlist_end; ++it)
		all_ok &= pos.pseudo_legal_s<true>(*it);

#if 0
	if (!all_ok)
	{
		sync_cout << "Error! : A non-pseudo legal move was generated." << endl
			      << pos << sync_endl;

		for (auto it = mlist_start; it != mlist_end; ++it)
		{
			if (!pos.pseudo_legal(it->move))
				sync_cout << "move = " << it->move << sync_endl;
		}

		sync_cout << "stopped." << sync_endl;
	}
#endif

	return all_ok;
}


template <PieceType Pt, Color Us, bool All> struct make_move_target {
	FORCE_INLINE Move* operator()(const Position& pos, Square from, const Bitboard& target_, Move* mlist)
	{
		if (!target_)
			return mlist;

		Square to;
		Bitboard target = target_;
		Bitboard target2;

		switch (Pt)
		{
		case PAWN:
			if (target)
			{
				to = from + (Us == BLACK ? SQ_U : SQ_D); 
				if (canPromote(Us, to))
				{
					*mlist++ = make_move_promote(from, to , Us, Pt);
					if (All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))
						*mlist++ = make_move(from, to , Us, Pt);
				}
				else
					*mlist++ = make_move(from, to , Us, Pt);
			}
			break;

		case LANCE:
		{
			target2 = target & enemy_field(Us);
			target2.foreach([&](Square to) { *mlist++ = make_move_promote(from, to , Us , Pt); });

			target &= All ? (Us == BLACK ? BB_Table::ForwardRanksBB[WHITE][RANK_1] : BB_Table::ForwardRanksBB[BLACK][RANK_9]) :
							(Us == BLACK ? BB_Table::ForwardRanksBB[WHITE][RANK_2] : BB_Table::ForwardRanksBB[BLACK][RANK_8]);

			target.foreach([&](Square to) { *mlist++ = make_move(from,to , Us , Pt); });
		}
		break;

		case KNIGHT:
		{
			while (target)
			{
				to = target.pop();
				if (canPromote(Us, to))
					*mlist++ = make_move_promote(from, to  , Us, Pt);
				if ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7))
					*mlist++ = make_move(from, to , Us, Pt);
			}
		}
		break;

		case SILVER:
		{
			if (enemy_field(Us) & from) {
				target.foreach([&](Square to) {
					*mlist++ = make_move_promote(from, to , Us, Pt);
					*mlist++ = make_move(from, to  , Us, Pt);
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to) {
					*mlist++ = make_move_promote(from, to , Us, Pt);
					*mlist++ = make_move(from, to , Us, Pt);
				});
				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { *mlist++ = make_move(from,to , Us,Pt); });
			}
		}
		break;

		case GOLD: case PRO_PAWN: case PRO_LANCE: case PRO_KNIGHT: case PRO_SILVER: case HORSE: case DRAGON: case KING:
			target.foreach([&](Square to) { *mlist++ = make_move(from,to , Us, Pt); });
			break;

		case GPM_GHDK:
		{
			const Piece pc = pos.piece_on(from);
			target.foreach([&](Square to) { *mlist++ = make_move(from, to, pc); });
		}
			break;

		case BISHOP: case ROOK:
			if (canPromote(Us, from)) {
				target.foreach([&](Square to) {
					*mlist++ = make_move_promote(from,to , Us,Pt);
					if (All) *mlist++ = make_move(from,to , Us,Pt);
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to) {
					*mlist++ = make_move_promote(from,to , Us,Pt);
					if (All) *mlist++ = make_move(from,to , Us,Pt);
				});

				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { *mlist++ = make_move(from,to , Us,Pt); });
			}
			break;

		case GPM_BR:
		{
			const Piece pc = pos.piece_on(from);
			if (canPromote(Us, from)) {
				target.foreach([&](Square to){
					*mlist++ = make_move_promote(from,to , pc);
					if (All) *mlist++ = make_move(from,to , pc)  ;
				});
			}
			else
			{
				target2 = target & enemy_field(Us);
				target2.foreach([&](Square to){
					*mlist++ = make_move_promote(from,to, pc);
					if (All) *mlist++ = make_move(from,to, pc);
				});

				target &= ~enemy_field(Us);
				target.foreach([&](Square to) { *mlist++ = make_move(from,to , pc); });
			}
		}
			break;


		default: UNREACHABLE; break;
		}

		return mlist;
	}
};

template <Color Us>
FORCE_INLINE Bitboard ghdk_effect(PieceType pt, Square from, const Bitboard& occ)
{
	constexpr u32 GOLD_CLASS = (1u << GOLD) | (1u << PRO_PAWN) | (1u << PRO_LANCE) | (1u << PRO_KNIGHT) | (1u << PRO_SILVER);
	if ((1u << pt) & GOLD_CLASS)
		return goldEffect<Us>(from);
	return pt == HORSE  ? horseEffect (from, occ) :
	       pt == DRAGON ? dragonEffect(from, occ) :
	                      kingEffect  (from     ); 
}

template <GenType Type, PieceType Pt, Color Us, bool All> struct GeneratePieceMoves {
	FORCE_INLINE Move* operator()(const Position&pos, Move*mlist, const Bitboard& target) {
		auto pieces = pos.pieces(Us, Pt);
		const auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();

			auto target2 =
				Pt == LANCE  ? lanceEffect (Us, from, occ) :
				Pt == KNIGHT ? knightEffect(Us, from) :
				Pt == SILVER ? silverEffect(Us, from) :
				Bitboard(1); 

			target2 &= target;
			mlist = make_move_target<Pt, Us, All>()(pos, from, target2, mlist);
		}

		return mlist;
	}
};

template <GenType Type, Color Us, bool All> struct GeneratePieceMoves<Type, PAWN, Us, All> {
	FORCE_INLINE Move* operator()(const Position&pos, Move*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us, PAWN);

		auto target2 = pawnBbEffect<Us>(pieces) & target;

		const Rank T_RANK1 = (Us == BLACK) ? RANK_1 : RANK_9;

		while (target2)
		{
			auto to = target2.pop();
			auto from = to + (Us == BLACK ? SQ_D : SQ_U);
			if (canPromote(Us, to))
			{
				*mlist++ = make_move_promote(from, to , Us, PAWN);

				if (All && rank_of(to) != T_RANK1)
				{

					*mlist++ = make_move(from, to, Us, PAWN);
				}
			}
			else
				*mlist++ = make_move(from, to , Us, PAWN);
		}
		return mlist;
	}
};

template <GenType Type, Color Us, bool All> struct GeneratePieceMoves<Type, GPM_BR, Us, All> {
	FORCE_INLINE Move* operator()(const Position&pos, Move*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,BISHOP,ROOK);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();

			auto target2 = (type_of(pos.piece_on(from)) == BISHOP ? bishopEffect(from, occ) : rookEffect(from, occ)) & target;

			mlist = make_move_target<GPM_BR, Us, All>()(pos, from, target2, mlist);
		}
		return mlist;
	}
};

template <GenType Type, Color Us, bool All> struct GeneratePieceMoves<Type, GPM_GHDK, Us, All> {
	FORCE_INLINE Move* operator()(const Position&pos, Move*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,GOLDS,HDK);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();
			auto target2 = ghdk_effect<Us>(type_of(pos.piece_on(from)), from, occ) & target;

			mlist = make_move_target<GPM_GHDK, Us, All>()(pos, from, target2, mlist);
		}
		return mlist;
	}
};

template <GenType Type, Color Us, bool All> struct GeneratePieceMoves<Type, GPM_GHD, Us, All> {
	FORCE_INLINE Move* operator()(const Position&pos, Move*mlist, const Bitboard& target)
	{
		auto pieces = pos.pieces(Us,GOLDS,HORSE,DRAGON);
		auto occ = pos.pieces();

		while (pieces)
		{
			auto from = pieces.pop();
			const auto pc = pos.piece_on(from);
			auto target2 = ghdk_effect<Us>(type_of(pc), from, occ) & target;
			target2.foreach([&](Square to) { *mlist++ = make_move(from,to,pc); });
		}
		return mlist;
	}
};



template <int N>
FORCE_INLINE void emit_drops(Move*& mlist, const u32* d, const Bitboard& t)
{
	static_assert(sizeof(Move) == sizeof(u32), "a Move is stored as its u32 value");
#if defined(__AVX512VL__)
	if constexpr (N >= 2)
	{
		constexpr __mmask8 k = __mmask8((1u << N) - 1);
		const __m256i dv = _mm256_maskz_loadu_epi32(k, d);
		t.foreach([&](Square sq) {
			_mm256_mask_storeu_epi32(mlist, k, _mm256_add_epi32(dv, _mm256_set1_epi32(int(sq))));
			mlist += N;
		});
		return;
	}
#endif
	t.foreach([&](Square sq) { Unroller<N>()([&](int i){ *mlist++ = Move(d[i] + sq); }); });
}

template <Color Us> struct GenerateDropMoves {
	Move* operator()(const Position&pos, Move*mlist, const Bitboard& target) {

		constexpr Color Them = ~Us;

		const Hand hand = pos.hand_of(Us);
		if (hand == 0)
			return mlist;
		if (!target)
			return mlist;

		if (hand_exists(hand, PAWN))
		{


			Bitboard target2 = target & pawn_drop_mask<Us>(pos.pieces(Us, PAWN));


			Bitboard pe = pawnEffect<Them>(pos.square<KING>(Them));
			if (pe & target2)
			{
				Square to = pe.pop_c();
				if (!pos.legal_drop(to))
					target2 ^= pe;
			}

			target2.foreach([&](Square sq) {
				*mlist++ = make_move_drop(PAWN , sq , Us );
			});

		}


		if (hand_except_pawn_exists(hand))
		{
			u32 drops[6];


			int num = 0;
			if (hand_exists(hand, KNIGHT)) drops[num++] = make_move_drop(KNIGHT, SQ_ZERO, Us).to_u32();

			int nextToKnight = num; 
			if (hand_exists(hand, LANCE )) drops[num++] = make_move_drop(LANCE , SQ_ZERO, Us).to_u32();

			int nextToLance  = num; 

			if (hand_exists(hand, SILVER)) drops[num++] = make_move_drop(SILVER, SQ_ZERO, Us).to_u32();
			if (hand_exists(hand, GOLD  )) drops[num++] = make_move_drop(GOLD  , SQ_ZERO, Us).to_u32();
			if (hand_exists(hand, BISHOP)) drops[num++] = make_move_drop(BISHOP, SQ_ZERO, Us).to_u32();
			if (hand_exists(hand, ROOK  )) drops[num++] = make_move_drop(ROOK  , SQ_ZERO, Us).to_u32();



			if (nextToLance == 0)
			{
				Bitboard target2 = target;

				switch (num)
				{
				case 1: emit_drops<1>(mlist, drops, target2); break;
				case 2: emit_drops<2>(mlist, drops, target2); break;
				case 3: emit_drops<3>(mlist, drops, target2); break;
				case 4: emit_drops<4>(mlist, drops, target2); break;
				default: UNREACHABLE;
				}
			}
			else
			{

				Bitboard target1 = target & rank1_n_bb(Us, RANK_1); 
				Bitboard target2 = target & (Us == BLACK ? RANK2_BB : RANK8_BB); 
				Bitboard target3 = target & rank1_n_bb(Them, RANK_7); 

				switch (num - nextToLance) 
				{
				case 0: break; 
				case 1: emit_drops<1>(mlist, drops + nextToLance, target1); break;
				case 2: emit_drops<2>(mlist, drops + nextToLance, target1); break;
				case 3: emit_drops<3>(mlist, drops + nextToLance, target1); break;
				case 4: emit_drops<4>(mlist, drops + nextToLance, target1); break;
				default: UNREACHABLE;
				}

				switch (num - nextToKnight) 
				{
				case 0: break; 
				case 1: emit_drops<1>(mlist, drops + nextToKnight, target2); break;
				case 2: emit_drops<2>(mlist, drops + nextToKnight, target2); break;
				case 3: emit_drops<3>(mlist, drops + nextToKnight, target2); break;
				case 4: emit_drops<4>(mlist, drops + nextToKnight, target2); break;
				case 5: emit_drops<5>(mlist, drops + nextToKnight, target2); break;
				default: UNREACHABLE;
				}

				switch (num) 
				{
				case 1: emit_drops<1>(mlist, drops, target3); break;
				case 2: emit_drops<2>(mlist, drops, target3); break;
				case 3: emit_drops<3>(mlist, drops, target3); break;
				case 4: emit_drops<4>(mlist, drops, target3); break;
				case 5: emit_drops<5>(mlist, drops, target3); break;
				case 6: emit_drops<6>(mlist, drops, target3); break;
				default: UNREACHABLE;
				}
			}

		}

		return mlist;
	}
};


template <Color C>
FORCE_INLINE Bitboard evasion_checker_effect(PieceType pt, Square sq, const Bitboard& occ)
{
	switch (pt)
	{
	case PAWN:   return pawnEffect  <C>(sq);
	case LANCE:  return lanceEffect <C>(sq, occ);
	case KNIGHT: return knightEffect<C>(sq);
	case SILVER: return silverEffect<C>(sq);
	case GOLD: case PRO_PAWN: case PRO_LANCE: case PRO_KNIGHT: case PRO_SILVER: return goldEffect<C>(sq);
	case BISHOP: return bishopEffect(sq, occ);
	case ROOK:   return rookEffect  (sq, occ);
	case HORSE:  return horseEffect (sq, occ);
	case DRAGON: return dragonEffect(sq, occ);
	case KING:   return kingEffect  (sq     );
	default: UNREACHABLE; return Bitboard(1);
	}
}

template<Color Us, bool All>
Move* generate_evasions(const Position& pos, Move* mlist)
{
	Move* mlist_org = mlist;


	ASSERT_LV2(pos.in_check());

	Bitboard sliderAttacks = Bitboard(ZERO);

	Bitboard checkers = pos.checkers();

	int checkersCnt = 0;

    Square   ksq = pos.square<KING>(Us);
	Bitboard occ = pos.pieces() ^ Bitboard(ksq);

	Square checksq;

	do
	{
		++checkersCnt;

		checksq = checkers.pop();

		ASSERT_LV3(color_of(pos.piece_on(checksq)) == ~Us);

		sliderAttacks |= evasion_checker_effect<~Us>(type_of(pos.piece_on(checksq)), checksq, occ);

	} while (checkers);


	Bitboard bb = kingEffect(ksq) & ~(pos.pieces(Us) | sliderAttacks);
	while (bb) { Square to = bb.pop(); *mlist++ = make_move(ksq, to , Us, KING); }

	if (checkersCnt <= 1)
	{



		const Bitboard target1 = between_bb(checksq, ksq);
		const Bitboard target2 = target1 | checksq;

		mlist = GeneratePieceMoves<NON_EVASIONS, PAWN   , Us, All>()(pos, mlist, target2);
		mlist = GeneratePieceMoves<NON_EVASIONS, LANCE  , Us, All>()(pos, mlist, target2);
		mlist = GeneratePieceMoves<NON_EVASIONS, KNIGHT , Us, All>()(pos, mlist, target2);
		mlist = GeneratePieceMoves<NON_EVASIONS, SILVER , Us, All>()(pos, mlist, target2);
		mlist = GeneratePieceMoves<NON_EVASIONS, GPM_BR , Us, All>()(pos, mlist, target2);
		mlist = GeneratePieceMoves<NON_EVASIONS, GPM_GHD, Us, All>()(pos, mlist, target2); 
		mlist = GenerateDropMoves<Us>()(pos, mlist, target1);
	}

	ASSERT_LV5(pseudo_legal_check(pos, mlist_org, mlist));

	return mlist;
}


template<GenType Type, Color Us, bool All>
Move* generate_general(const Position& pos, Move* mlist, Square recapSq = SQ_NB) {



	static_assert(Type != EVASIONS_ALL && Type != NON_EVASIONS_ALL && Type != RECAPTURES_ALL, "*_ALL is not allowed.");

	Move* mlist_org = mlist;
	constexpr Color Them = ~Us;
	
	const Bitboard target =
		(Type == QUIETS            ) ?  pos.empties()      : 
		(Type == CAPTURES          ) ?  pos.pieces(Them)   : 
		(Type == QUIETS_PRO_MINUS  ) ?  pos.empties()      : 
		(Type == CAPTURES_PRO_PLUS ) ?  pos.pieces(Them)   : 
		(Type == NON_EVASIONS      ) ? ~pos.pieces(Us)     : 
		(Type == RECAPTURES        ) ?  Bitboard(recapSq)  : 
		Bitboard(1); 

	const Bitboard targetPawn =
		(Type == QUIETS_PRO_MINUS ) ?  enemy_field(Us).andnot(pos.empties()) : 
        (Type == CAPTURES_PRO_PLUS) ? (pos.pieces(Us).andnot(enemy_field(Us)) | pos.pieces(Them)) :  
		                               target;

	mlist = GeneratePieceMoves<Type, PAWN    , Us, All>()(pos, mlist, targetPawn);

	mlist = GeneratePieceMoves<Type, LANCE   , Us, All>()(pos, mlist, target);
	mlist = GeneratePieceMoves<Type, KNIGHT  , Us, All>()(pos, mlist, target);
	mlist = GeneratePieceMoves<Type, SILVER  , Us, All>()(pos, mlist, target);

	mlist = GeneratePieceMoves<Type, GPM_BR  , Us, All>()(pos, mlist, target);

	mlist = GeneratePieceMoves<Type, GPM_GHDK, Us, All>()(pos, mlist, target);

	if (Type == QUIETS || Type == QUIETS_PRO_MINUS || Type == NON_EVASIONS)
		mlist = GenerateDropMoves<Us>()(pos, mlist, pos.empties());

	ASSERT_LV5(pseudo_legal_check(pos,mlist_org, mlist));

	return mlist;
}


template <Color Us, bool All>
struct make_move_target_general {
	Move* operator()(const Position& pos, Piece pc, Square from, const Bitboard& target, Move* mlist) {

		ASSERT_LV2(pc != NO_PIECE);
		auto effect = effects_from(pc, from, pos.pieces());
		switch (type_of(pc))
		{
		case PAWN      : mlist = make_move_target<PAWN      , Us, All>()(pos, from, effect & target, mlist); break;
		case LANCE     : mlist = make_move_target<LANCE     , Us, All>()(pos, from, effect & target, mlist); break;
		case KNIGHT    : mlist = make_move_target<KNIGHT    , Us, All>()(pos, from, effect & target, mlist); break;
		case SILVER    : mlist = make_move_target<SILVER    , Us, All>()(pos, from, effect & target, mlist); break;
		case GOLD      : mlist = make_move_target<GOLD      , Us, All>()(pos, from, effect & target, mlist); break;
		case BISHOP    : mlist = make_move_target<BISHOP    , Us, All>()(pos, from, effect & target, mlist); break;
		case ROOK      : mlist = make_move_target<ROOK      , Us, All>()(pos, from, effect & target, mlist); break;
		case KING      : mlist = make_move_target<KING      , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_PAWN  : mlist = make_move_target<PRO_PAWN  , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_LANCE : mlist = make_move_target<PRO_LANCE , Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_KNIGHT: mlist = make_move_target<PRO_KNIGHT, Us, All>()(pos, from, effect & target, mlist); break;
		case PRO_SILVER: mlist = make_move_target<PRO_SILVER, Us, All>()(pos, from, effect & target, mlist); break;
		case HORSE     : mlist = make_move_target<HORSE     , Us, All>()(pos, from, effect & target, mlist); break;
		case DRAGON    : mlist = make_move_target<DRAGON    , Us, All>()(pos, from, effect & target, mlist); break;
		default: UNREACHABLE;
		}
		return mlist;
	}
};

template <PieceType Pt, Color Us, bool All, bool Promote>
Move* make_move_target_pro(Square from, const Bitboard& target, Move* mlist)
{
	auto bb = target;
	while (bb)
	{
		auto to = bb.pop();
		if (Promote)
			*mlist++ = make_move_promote(from, to , Us, Pt);
		else
		{
			if (   ((Pt == PAWN) &&
					((!All && !canPromote(Us, to)) ||
					(  All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))))
				|| ((Pt == LANCE) &&
					((!All && ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7))) ||
					 ( All && rank_of(to) != (Us == BLACK ? RANK_1 : RANK_9))))
				|| ( Pt == KNIGHT && ((Us == BLACK && rank_of(to) >= RANK_3) || (Us == WHITE && rank_of(to) <= RANK_7)))
				|| ( Pt == SILVER)
				|| ((Pt == BISHOP || Pt == ROOK) && (!(canPromote(Us, from) || canPromote(Us, to)) || All))
				)

				*mlist++ = make_move(from, to , Us, Pt);
		}
	}
	return mlist;
}

template <Color Us, bool All>
Move* make_move_check(const Position& pos, Piece pc, Square from, Square ksq, const Bitboard& target, Move* mlist)
{
#define GEN_MOVE_NONPRO_CHECK(X,X_Effect,Y_Effect) {                   \
    dst = X_Effect(Us, from) & Y_Effect(~Us, ksq) & target;            \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    dst = X_Effect(Us, from) & X_Effect(~Us, ksq) & target;            \
    mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); }

#define GEN_MOVE_LANCE_CHECK(X,X_Effect,Y_Effect) {                    \
    occ = pos.pieces();                                                \
    dst = X_Effect(Us, from,occ) & Y_Effect(~Us, ksq) & target;        \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    if (file_of(from) == file_of(ksq) && !(between_bb(from, ksq) & occ).more_than_one()){ \
      dst = pos.pieces(~Us) & between_bb(from, ksq) & target;            \
      mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); \
    }}

#define GEN_MOVE_NONPRO_PRO_CHECK_BR(X,X_Effect,Y_Effect) {            \
  occ = pos.pieces();                                                  \
  dst = X_Effect(from,occ) & Y_Effect(ksq,occ) & target;               \
    if (!(enemy_field(Us) & from))                                     \
      dst &= enemy_field(Us);                                          \
    mlist = make_move_target_pro<X, Us, All, true>(from, dst, mlist);  \
    dst = X_Effect(from,occ) & X_Effect(ksq,occ) & target;             \
    mlist = make_move_target_pro<X, Us, All, false>(from, dst, mlist); }

#define GEN_MOVE_GOLD_CHECK(X,X_Effect) {                              \
  dst = X_Effect(Us, from) & X_Effect(~Us, ksq) & target;              \
  mlist = make_move_target<X, Us, All>()(pos,from, dst, mlist); }

#define GEN_MOVE_HD_CHECK(X,X_Effect) {                                \
  occ = pos.pieces();                                                  \
  dst = X_Effect(from,occ) & X_Effect(ksq,occ) & target;               \
  mlist = make_move_target<X, Us, All>()(pos,from, dst, mlist); }

	Bitboard dst, occ;
	switch (type_of(pc))
	{
	case PAWN      : GEN_MOVE_NONPRO_CHECK(PAWN, pawnEffect, goldEffect); break;
	case LANCE     : GEN_MOVE_LANCE_CHECK(LANCE, lanceEffect, goldEffect); break;
	case KNIGHT    : GEN_MOVE_NONPRO_CHECK(KNIGHT, knightEffect, goldEffect); break;
	case SILVER    : GEN_MOVE_NONPRO_CHECK(SILVER, silverEffect, goldEffect); break;
	case BISHOP    : GEN_MOVE_NONPRO_PRO_CHECK_BR(BISHOP, bishopEffect, horseEffect); break;
	case ROOK      : GEN_MOVE_NONPRO_PRO_CHECK_BR(ROOK, rookEffect, dragonEffect); break;

	case PRO_PAWN  : GEN_MOVE_GOLD_CHECK(PRO_PAWN, goldEffect); break;
	case PRO_LANCE : GEN_MOVE_GOLD_CHECK(PRO_LANCE, goldEffect); break;
	case PRO_KNIGHT: GEN_MOVE_GOLD_CHECK(PRO_KNIGHT, goldEffect); break;
	case PRO_SILVER: GEN_MOVE_GOLD_CHECK(PRO_SILVER, goldEffect); break;
	case GOLD      : GEN_MOVE_GOLD_CHECK(GOLD, goldEffect); break;
	case HORSE     : GEN_MOVE_HD_CHECK(HORSE, horseEffect); break;
	case DRAGON    : GEN_MOVE_HD_CHECK(DRAGON, dragonEffect); break;

	default:
		UNREACHABLE;
	}
	return mlist;
}


template <Color Us, PieceType Pt> struct GenerateCheckDropMoves {
	Move* operator()(const Position& , const Bitboard& target, Move* mlist)
	{
		auto bb = target;
		while (bb)
		{
			auto to = bb.pop();
			*mlist++ = make_move_drop(Pt, to , Us);
		}
		return mlist;
	}
};

template <Color Us> struct GenerateCheckDropMoves<Us, PAWN> {
	Move* operator()(const Position& pos, const Bitboard& target, Move* mlist)
	{
		auto bb = target;
		if (bb) 
		{
			auto to = bb.pop_c();

			if (pos.legal_pawn_drop(Us, to))
				*mlist++ = make_move_drop(PAWN, to , Us);
		}
		return mlist;
	}
};


template<GenType Type, Color Us, bool All>
Move* generate_checks(const Position& pos, Move* mlist)
{




	constexpr Color Them = ~Us;
    const Square    themKing = pos.square<KING>(Them);

	const Bitboard x =
		(
			(pos.pieces(PAWN)   & check_candidate_bb(Us, PAWN  , themKing)) |
			(pos.pieces(LANCE)  & check_candidate_bb(Us, LANCE , themKing)) |
			(pos.pieces(KNIGHT) & check_candidate_bb(Us, KNIGHT, themKing)) |
			(pos.pieces(SILVER) & check_candidate_bb(Us, SILVER, themKing)) |
			(pos.pieces(GOLDS)  & check_candidate_bb(Us, GOLD  , themKing)) |
			(pos.pieces(BISHOP) & check_candidate_bb(Us, BISHOP, themKing)) |
			(pos.pieces(ROOK_DRAGON)) | 
			(pos.pieces(HORSE)  & check_candidate_bb(Us, ROOK  , themKing)) 
		) & pos.pieces(Us);


	const Bitboard y = pos.blockers_for_king(Them) & pos.pieces(Us);

	const Bitboard target =
		(Type == CHECKS       || Type == CHECKS_ALL      ) ? ~pos.pieces(Us) :           
		(Type == QUIET_CHECKS || Type == QUIET_CHECKS_ALL) ?  pos.empties()  :           
		                                                      Bitboard(1);               

	auto src = y;
	while (src)
	{
		auto from = src.pop();


		auto pin_line = line_bb(themKing, from);
		
		mlist = make_move_target_general<Us, All>()(pos, pos.piece_on(from), from, pin_line.andnot(target) , mlist);

		if (x & from)
			mlist = make_move_check<Us, All>(pos, pos.piece_on(from), from, themKing, pin_line & target, mlist);
	}

	src = (x | y) ^ y;
	while (src)
	{
		auto from = src.pop();

		mlist = make_move_check<Us, All>(pos, pos.piece_on(from), from, themKing, target, mlist);
	}


	const Bitboard& empties = pos.empties();

	Hand h = pos.hand_of(Us);
	if (hand_exists(h, PAWN))
		mlist = GenerateCheckDropMoves<Us, PAWN>()(pos, pos.check_squares(PAWN) & empties, mlist);
	if (hand_exists(h, LANCE))
		mlist = GenerateCheckDropMoves<Us, LANCE>()(pos, pos.check_squares(LANCE) & empties, mlist);
	if (hand_exists(h, KNIGHT))
		mlist = GenerateCheckDropMoves<Us, KNIGHT>()(pos, pos.check_squares(KNIGHT) & empties, mlist);
	if (hand_exists(h, SILVER))
		mlist = GenerateCheckDropMoves<Us, SILVER>()(pos, pos.check_squares(SILVER) & empties, mlist);
	if (hand_exists(h, GOLD))
		mlist = GenerateCheckDropMoves<Us, GOLD>()(pos, pos.check_squares(GOLD) & empties, mlist);
	if (hand_exists(h, BISHOP))
		mlist = GenerateCheckDropMoves<Us, BISHOP>()(pos, pos.check_squares(BISHOP) & empties, mlist);
	if (hand_exists(h, ROOK))
		mlist = GenerateCheckDropMoves<Us, ROOK>()(pos, pos.check_squares(ROOK) & empties, mlist);

	return mlist;
}



template<GenType Type, bool All>
Move* generate(const Position& pos, Move* mlist, Square sq = SQ_NB)
{
	return pos.side_to_move() == BLACK ? generate_general<Type, BLACK, All>(pos, mlist, sq) : generate_general<Type, WHITE, All>(pos, mlist, sq);
}

template<bool All>
Move* generateEvasionMoves(const Position& pos, Move* mlist)
{
	return pos.side_to_move() == BLACK ? generate_evasions<BLACK, All>(pos, mlist) : generate_evasions<WHITE, All>(pos, mlist);
}

template<GenType Type, bool All>
Move* generateChecksMoves(const Position& pos, Move* mlist)
{
	return pos.side_to_move() == BLACK ? generate_checks<Type, BLACK, All>(pos, mlist) : generate_checks<Type, WHITE, All>(pos, mlist);
}



template<GenType Type>
Move* generate(const Position& pos, Move* mlist, Square recapSq)
{
	const bool All = (Type == EVASIONS_ALL) || (Type == CHECKS_ALL)     || (Type == LEGAL_ALL)
		|| (Type == NON_EVASIONS_ALL)       || (Type == RECAPTURES_ALL) || (Type == QUIET_CHECKS_ALL)
		|| (Type == CAPTURES_ALL)           || (Type == QUIETS_ALL)
		|| (Type == CAPTURES_PRO_PLUS_ALL)  || (Type == QUIETS_PRO_MINUS_ALL)
		;

	if (Type == LEGAL || Type == LEGAL_ALL)
	{

		auto last = pos.in_check() ? generateEvasionMoves<All>(pos, mlist) : generate<NON_EVASIONS, All>(pos, mlist);

		while (mlist != last)
		{
			if (!pos.legal(*mlist))
				*mlist = *(--last);
			else
				++mlist;
		}
		return last;
	}

	if (Type == CHECKS || Type == CHECKS_ALL || Type == QUIET_CHECKS || Type == QUIET_CHECKS_ALL)
	{
		auto last = generateChecksMoves<Type, All>(pos, mlist);

		if (pos.in_check())
			while (mlist != last)
			{
				if (!pos.pseudo_legal_s<All>(*mlist))
					*mlist = *(--last);
				else
					++mlist;
			}
		return last;
	}

	if (Type == EVASIONS || Type == EVASIONS_ALL)
		return generateEvasionMoves<All>(pos, mlist);

	const auto Type2 =
		Type == NON_EVASIONS_ALL      ? NON_EVASIONS      :
		Type == RECAPTURES_ALL        ? RECAPTURES        :
		Type == EVASIONS_ALL          ? EVASIONS          :
		Type == CAPTURES_ALL          ? CAPTURES          :
		Type == CAPTURES_PRO_PLUS_ALL ? CAPTURES_PRO_PLUS :
		Type == QUIETS_ALL            ? QUIETS            :
		Type == QUIETS_PRO_MINUS_ALL  ? QUIETS_PRO_MINUS  :
		Type; 
	return generate<Type2, All>(pos, mlist, recapSq);
}

template<GenType Type>
Move* generate(const Position& pos, Move* mlist)
{
	static_assert(Type != RECAPTURES && Type != RECAPTURES_ALL, "RECAPTURES , not allowed.");
	return generate<Type>(pos, mlist, SQ_NB);
}



template Move* generate<QUIETS                >(const Position& pos, Move* mlist);
template Move* generate<CAPTURES              >(const Position& pos, Move* mlist);

template Move* generate<QUIETS_ALL            >(const Position& pos, Move* mlist);
template Move* generate<CAPTURES_ALL          >(const Position& pos, Move* mlist);

template Move* generate<QUIETS_PRO_MINUS      >(const Position& pos, Move* mlist);
template Move* generate<QUIETS_PRO_MINUS_ALL  >(const Position& pos, Move* mlist);

template Move* generate<CAPTURES_PRO_PLUS     >(const Position& pos, Move* mlist);
template Move* generate<CAPTURES_PRO_PLUS_ALL >(const Position& pos, Move* mlist);

template Move* generate<EVASIONS              >(const Position& pos, Move* mlist);
template Move* generate<EVASIONS_ALL          >(const Position& pos, Move* mlist);

template Move* generate<NON_EVASIONS          >(const Position& pos, Move* mlist);
template Move* generate<NON_EVASIONS_ALL      >(const Position& pos, Move* mlist);

template Move* generate<LEGAL                 >(const Position& pos, Move* mlist);
template Move* generate<LEGAL_ALL             >(const Position& pos, Move* mlist);

template Move* generate<CHECKS                >(const Position& pos, Move* mlist);
template Move* generate<CHECKS_ALL            >(const Position& pos, Move* mlist);

template Move* generate<QUIET_CHECKS          >(const Position& pos, Move* mlist);
template Move* generate<QUIET_CHECKS_ALL      >(const Position& pos, Move* mlist);

template Move* generate<RECAPTURES            >(const Position& pos, Move* mlist, Square recapSq);
template Move* generate<RECAPTURES_ALL        >(const Position& pos, Move* mlist, Square recapSq);

} 

