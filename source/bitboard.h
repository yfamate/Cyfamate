#ifndef _BITBOARD_H_
#define _BITBOARD_H_

#include "types.h"

namespace YaneuraOu {

namespace Bitboards
{
	
	void init();
}







enum BitboardZero{ ZERO };



#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif



struct alignas(16) Bitboard
{
#if defined (USE_SSE2)

	union
	{
		
		

		u64 p[2];

		
		
		
		
		
		

		

		__m128i m;
	};

#else 
	u64 p[2];
#endif

#if defined (USE_SSE2)
	

	Bitboard& operator = (const Bitboard& rhs) { _mm_store_si128(&this->m, rhs.m); return *this; }

	Bitboard(const Bitboard& bb) { _mm_store_si128(&this->m, bb.m); }
#endif

	

	
	Bitboard() {}

	
	
	
	Bitboard(const int N)
	{
		
		ASSERT_LV3(N == 0 || N == 1);

		if (N == 0)
		{
#if defined (USE_SSE2)
			m = _mm_setzero_si128();
#else
			p[0] = p[1] = 0;
#endif
		}
		else if (N == 1)
		{
			
			

#if defined (USE_SSE2)
			m = _mm_set_epi64x(UINT64_C(0x000000000003FFFF), UINT64_C(0x7FFFFFFFFFFFFFFF));
#else
			p[0] = UINT64_C(0x7FFFFFFFFFFFFFFF);
			p[1] = UINT64_C(0x000000000003FFFF);
#endif
		}
	}

	
	Bitboard(u64 p0, u64 p1);

	
	Bitboard(Square sq);

	
	void set(u64 p0, u64 p1);

	

	
	operator bool() const;

	
	
	

	bool test(Bitboard rhs) const;
	bool test(Square sq) const { return test(Bitboard(sq)); }

	
	template <int n> u64 extract64() const;

	
	u64 extract64(int n) const { return n == 0 ? extract64<0>() : extract64<1>(); }

	
	template <int n> Bitboard& insert64(u64 u);

	
	u64 merge() const { return extract64<0>() | extract64<1>(); }

	
	
	bool cross_over() const { return extract64<0>() & extract64<1>(); }

	
	
	
	
	constexpr static int part(Square sq) { return static_cast<int>(SQ_79 < sq); }

	

	
	
	
	
	
	Square pop();

	
	Square pop_c() const { u64 q0 = extract64<0>();  return (q0 != 0) ? Square(LSB64(q0)) : Square(LSB64(extract64<1>()) + 63); }

	
	int pop_count() const { return (int)(POPCNT64(extract64<0>()) + POPCNT64(extract64<1>())); }

	

#if defined (USE_SSE2)
	Bitboard& operator |= (const Bitboard& b1) { this->m = _mm_or_si128 (m, b1.m); return *this; }
	Bitboard& operator &= (const Bitboard& b1) { this->m = _mm_and_si128(m, b1.m); return *this; }
	Bitboard& operator ^= (const Bitboard& b1) { this->m = _mm_xor_si128(m, b1.m); return *this; }
	Bitboard& operator += (const Bitboard& b1) { this->m = _mm_add_epi64(m, b1.m); return *this; }
	Bitboard& operator -= (const Bitboard& b1) { this->m = _mm_sub_epi64(m, b1.m); return *this; }

	
	
	Bitboard& operator <<= (int shift) {  m = _mm_slli_epi64(m, shift); return *this; }

	
	Bitboard& operator >>= (int shift) {  m = _mm_srli_epi64(m, shift); return *this; }

#else
	Bitboard& operator |= (const Bitboard& b1) { this->p[0] |= b1.p[0]; this->p[1] |= b1.p[1]; return *this; }
	Bitboard& operator &= (const Bitboard& b1) { this->p[0] &= b1.p[0]; this->p[1] &= b1.p[1]; return *this; }
	Bitboard& operator ^= (const Bitboard& b1) { this->p[0] ^= b1.p[0]; this->p[1] ^= b1.p[1]; return *this; }
	Bitboard& operator += (const Bitboard& b1) { this->p[0] += b1.p[0]; this->p[1] += b1.p[1]; return *this; }
	Bitboard& operator -= (const Bitboard& b1) { this->p[0] -= b1.p[0]; this->p[1] -= b1.p[1]; return *this; }

	Bitboard& operator <<= (int shift) {  this->p[0] <<= shift; this->p[1] <<= shift; return *this; }
	Bitboard& operator >>= (int shift) {  this->p[0] >>= shift; this->p[1] >>= shift; return *this; }

#endif

	

	bool operator == (const Bitboard& rhs) const;
	bool operator != (const Bitboard& rhs) const { return !(*this == rhs); }

	

	Bitboard operator & (const Bitboard& rhs) const { return Bitboard(*this) &= rhs; }
	Bitboard operator | (const Bitboard& rhs) const { return Bitboard(*this) |= rhs; }
	Bitboard operator ^ (const Bitboard& rhs) const { return Bitboard(*this) ^= rhs; }
	Bitboard operator + (const Bitboard& rhs) const { return Bitboard(*this) += rhs; }
	Bitboard operator - (const Bitboard& rhs) const { return Bitboard(*this) -= rhs; }
	Bitboard operator << (const int i) const { return Bitboard(*this) <<= i; }
	Bitboard operator >> (const int i) const { return Bitboard(*this) >>= i; }

	

#if defined (USE_SSE2)
	
	
	
	
	Bitboard andnot(const Bitboard& b1) const { Bitboard b0; b0.m = _mm_andnot_si128(m, b1.m); return b0; }
#else
	Bitboard andnot(const Bitboard& b1) const { Bitboard b0; b0.p[0] = ~p[0] & b1.p[0]; b0.p[1] = ~p[1] & b1.p[1]; return b0; }
#endif

	
	
	Bitboard byte_reverse() const;

	
	
	
	static void unpack(const Bitboard hi_in,const Bitboard lo_in, Bitboard& hi_out, Bitboard& lo_out);

	
	
	static void decrement(const Bitboard hi_in,const Bitboard lo_in, Bitboard& hi_out, Bitboard& lo_out);

	
	Bitboard decrement() const;

	
	
	bool more_than_one() const;

	
	Square operator*() { return pop(); }
	void operator++() {}

	
	
	
	template <typename T> FORCE_INLINE void foreach(T t) const
	{
		u64 p0 = this->extract64<0>();
		while (p0) t( (Square)pop_lsb(p0));

		u64 p1 = this->extract64<1>();
		while (p1) t( (Square)(pop_lsb(p1) + 63));
	}

	
	
	
	
	
	template <typename T> FORCE_INLINE void foreach_part(T t) const
	{
		u64 p0 = this->extract64<0>();
		while (p0) { t( (Square)(pop_lsb(p0)     ),0); }

		u64 p1 = this->extract64<1>();
		while (p1) { t( (Square)(pop_lsb(p1) + 63),1); }
	}

	
	static void UnitTest(Test::UnitTester& tester, IEngine& engine);
};


#if defined (__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
#endif


template <int n>
inline u64 Bitboard::extract64() const
{
	static_assert(n == 0 || n == 1, "");
#if defined(USE_SSE41) && defined(IS_64BIT)
	return (u64)(_mm_extract_epi64(m, n));
#else
	return p[n];
#endif
}

template <int n>
inline Bitboard& Bitboard::insert64(u64 u)
{
	static_assert(n == 0 || n == 1, "");
#if defined(USE_SSE41) && defined(IS_64BIT)
	m = _mm_insert_epi64(m, u, n);
	
	
#else
	p[n] = u;
#endif
	return *this;
}


Bitboard operator|(const Bitboard& b, Square s);
Bitboard operator&(const Bitboard& b, Square s);
Bitboard operator^(const Bitboard& b, Square s);



Bitboard operator ~ (const Bitboard& a);


const Bitboard begin(const Bitboard& b);
const Bitboard end(const Bitboard&);


std::ostream& operator<<(std::ostream& os, const Bitboard& board);









struct alignas(32) Bitboard256
{
	
#if defined (USE_AVX2)
	union
	{
		
		u64 p[4];

		__m256i m;
	};
#else 
	u64 p[4];
#endif

	Bitboard256() {}
#if defined (USE_AVX2)
	Bitboard256& operator = (const Bitboard256& rhs) { _mm256_store_si256(&this->m, rhs.m); return *this; }
	Bitboard256(const Bitboard256& bb) { _mm256_store_si256(&this->m, bb.m); }

	
	Bitboard256(const Bitboard& b1) { m = _mm256_broadcastsi128_si256(b1.m); }

	
	Bitboard256(const Bitboard& b1, const Bitboard& b2) {
		
		m = _mm256_castsi128_si256(b1.m);        
		m = _mm256_inserti128_si256(m, b2.m, 1); 
	}
#else
	Bitboard256(const Bitboard& b1, const Bitboard& b2) { p[0] = b1.p[0]; p[1] = b1.p[1]; p[2] = b2.p[0]; p[3]=b2.p[1]; }
	Bitboard256(const Bitboard& b1) { p[0] = p[2] = b1.p[0]; p[1] = p[3] = b1.p[1]; }
#endif

#if defined (USE_AVX2)
	Bitboard256& operator |= (const Bitboard256& b1) { this->m = _mm256_or_si256( m, b1.m); return *this; }
	Bitboard256& operator &= (const Bitboard256& b1) { this->m = _mm256_and_si256(m, b1.m); return *this; }
	Bitboard256& operator ^= (const Bitboard256& b1) { this->m = _mm256_xor_si256(m, b1.m); return *this; }
	Bitboard256& operator += (const Bitboard256& b1) { this->m = _mm256_add_epi64(m, b1.m); return *this; }
	Bitboard256& operator -= (const Bitboard256& b1) { this->m = _mm256_sub_epi64(m, b1.m); return *this; }

	
	
	Bitboard256& operator <<= (int shift) {  m = _mm256_slli_epi64(m, shift); return *this; }

	
	Bitboard256& operator >>= (int shift) {  m = _mm256_srli_epi64(m, shift); return *this; }

	
	
	
	
	Bitboard256 andnot(const Bitboard256& b1) const { Bitboard256 b0; b0.m = _mm256_andnot_si256(m, b1.m); return b0; }

#else
	Bitboard256& operator |= (const Bitboard256& b1) { this->p[0] |= b1.p[0]; this->p[1] |= b1.p[1]; this->p[2] |= b1.p[2]; this->p[3] |= b1.p[3]; return *this; }
	Bitboard256& operator &= (const Bitboard256& b1) { this->p[0] &= b1.p[0]; this->p[1] &= b1.p[1]; this->p[2] &= b1.p[2]; this->p[3] &= b1.p[3]; return *this; }
	Bitboard256& operator ^= (const Bitboard256& b1) { this->p[0] ^= b1.p[0]; this->p[1] ^= b1.p[1]; this->p[2] ^= b1.p[2]; this->p[3] ^= b1.p[3]; return *this; }
	Bitboard256& operator += (const Bitboard256& b1) { this->p[0] += b1.p[0]; this->p[1] += b1.p[1]; this->p[2] += b1.p[2]; this->p[3] += b1.p[3]; return *this; }
	Bitboard256& operator -= (const Bitboard256& b1) { this->p[0] -= b1.p[0]; this->p[1] -= b1.p[1]; this->p[2] -= b1.p[2]; this->p[3] -= b1.p[3]; return *this; }

	Bitboard256& operator <<= (int shift) {  this->p[0] <<= shift; this->p[1] <<= shift; this->p[2] <<= shift; this->p[3] <<= shift; return *this; }
	Bitboard256& operator >>= (int shift) {  this->p[0] >>= shift; this->p[1] >>= shift; this->p[2] >>= shift; this->p[3] >>= shift; return *this; }

	Bitboard256 andnot(const Bitboard256& b1) const { Bitboard256 b0; b0.p[0] = ~p[0] & b1.p[0]; b0.p[1] = ~p[1] & b1.p[1]; b0.p[2] = ~p[2] & b1.p[2]; b0.p[3] = ~p[3] & b1.p[3]; return b0; }

#endif

	

	bool operator == (const Bitboard256& rhs) const;
	bool operator != (const Bitboard256& rhs) const { return !(*this == rhs); }

	

	Bitboard256 operator & (const Bitboard256& rhs) const { return Bitboard256(*this) &= rhs; }
	Bitboard256 operator | (const Bitboard256& rhs) const { return Bitboard256(*this) |= rhs; }
	Bitboard256 operator ^ (const Bitboard256& rhs) const { return Bitboard256(*this) ^= rhs; }
	Bitboard256 operator + (const Bitboard256& rhs) const { return Bitboard256(*this) += rhs; }
	Bitboard256 operator - (const Bitboard256& rhs) const { return Bitboard256(*this) -= rhs; }
	Bitboard256 operator << (const int i) const { return Bitboard256(*this) <<= i; }
	Bitboard256 operator >> (const int i) const { return Bitboard256(*this) >>= i; }

	

	
	void toBitboard(Bitboard& b1, Bitboard& b2) const { b1 = Bitboard(p[0], p[1]); b2 = Bitboard(p[2], p[3]); }

	
	
	Bitboard256 byte_reverse() const;

	
	
	
	static void unpack(const Bitboard256 hi_in,const Bitboard256 lo_in, Bitboard256& hi_out, Bitboard256& lo_out);

	
	
	static void decrement(const Bitboard256 hi_in,const Bitboard256 lo_in, Bitboard256& hi_out, Bitboard256& lo_out);

	
	Bitboard merge() const;

	
	static void UnitTest(Test::UnitTester& tester, IEngine& engine);
};

inline bool Bitboard256::operator == (const Bitboard256& rhs) const
{
#if defined (USE_AVX2)
	__m256i neq = _mm256_xor_si256(this->m, rhs.m);
	return  _mm256_testz_si256(neq, neq) ? true : false;
#else
	return (this->p[0] == rhs.p[0]) && (this->p[1] == rhs.p[1]) && (this->p[2] == rhs.p[2]) && (this->p[3] == rhs.p[3]);
	
	
#endif
}


std::ostream& operator<<(std::ostream& os, const Bitboard256& board);





namespace BB_Table
{
	
	extern const Bitboard FILE1_BB;
	extern const Bitboard FILE2_BB;
	extern const Bitboard FILE3_BB;
	extern const Bitboard FILE4_BB;
	extern const Bitboard FILE5_BB;
	extern const Bitboard FILE6_BB;
	extern const Bitboard FILE7_BB;
	extern const Bitboard FILE8_BB;
	extern const Bitboard FILE9_BB;

	
	extern const Bitboard RANK1_BB;
	extern const Bitboard RANK2_BB;
	extern const Bitboard RANK3_BB;
	extern const Bitboard RANK4_BB;
	extern const Bitboard RANK5_BB;
	extern const Bitboard RANK6_BB;
	extern const Bitboard RANK7_BB;
	extern const Bitboard RANK8_BB;
	extern const Bitboard RANK9_BB;

	
	extern const Bitboard FILE_BB[FILE_NB];

	
	extern const Bitboard RANK_BB[RANK_NB];

	
	
	

	
	
	
	
}

static const Bitboard file_bb(File f) { return BB_Table::FILE_BB[f]; }
static const Bitboard rank_bb(Rank r) { return BB_Table::RANK_BB[r]; }








namespace BB_Table { extern const Bitboard ForwardRanksBB[COLOR_NB][RANK_NB]; }


static const Bitboard rank1_n_bb(Color US, const Rank r)
{
	ASSERT_LV2(is_ok(r));
	return BB_Table::ForwardRanksBB[US][(US == BLACK ? r + 1 : 7 - r)];
}


namespace BB_Table { extern const Bitboard EnemyField[COLOR_NB]; }
static const Bitboard enemy_field(Color Us) { return BB_Table::EnemyField[Us]; }




namespace BB_Table {
	extern Bitboard BetweenBB[785];
	extern u16 BetweenIndex[SQ_NB_PLUS1][SQ_NB_PLUS1];
}


static const Bitboard between_bb(Square sq1, Square sq2) { return BB_Table::BetweenBB[BB_Table::BetweenIndex[sq1][sq2]]; }




namespace BB_Table { extern Bitboard LineBB[SQ_NB][4]; }


static const Bitboard line_bb(Square sq1, Square sq2)
{
	static_assert(Effect8::DIRECT_RU == 0 && Effect8::DIRECT_LD == 7 , "");
	auto directions = Effect8::directions_of(sq1, sq2);
	ASSERT_LV3(directions != 0);
	static const int a[8] = { 0 , 1 , 2 , 3 , 3 , 2 , 1 , 0 };
	return BB_Table::LineBB[sq1][a[(int)Effect8::pop_directions(directions)]];
}

#if 0

static bool aligned(Square s1, Square s2, Square s3) {
	return LineBB[s1][s2] & s3;
}
#endif



namespace BB_Table { extern Bitboard CheckCandidateBB[SQ_NB_PLUS1][KING - 1][COLOR_NB]; }




static const Bitboard check_candidate_bb(Color us, PieceType pr, Square sq)
{
	ASSERT_LV3(PAWN<= pr && pr < KING && sq <= SQ_NB && is_ok(us));
	return BB_Table::CheckCandidateBB[sq][pr - 1][us];
}

#if defined(LONG_EFFECT_LIBRARY) 

namespace BB_Table { extern Bitboard CheckCandidateKingBB[SQ_NB_PLUS1]; }


static const Bitboard around24_bb(Square sq)
{
	ASSERT_LV3(sq <= SQ_NB);
	return BB_Table::CheckCandidateKingBB[sq];
}
#endif







template <Color C>
static Bitboard pawn_drop_mask(const Bitboard& pawns) {
	

	const Bitboard left(0x4020100804020100ULL, 0x0000000000020100ULL);

	
	Bitboard t = left - pawns;

#if 0
	
	t = (t & left) >> 8;

	
	
	
	
	
	
	

	return left ^ (left - t);
#endif
	
	
	

	if (C == BLACK)
	{
		
		t = (t & left) >> 7;
		return left ^ (left - t);
	}
	else {
		
		t = (t & left) >> 8;
		return left.andnot(left - t);
	}
}













namespace BB_Table
{
	extern Bitboard KingEffectBB[SQ_NB_PLUS1];
	extern Bitboard GoldEffectBB[SQ_NB_PLUS1][COLOR_NB];
	extern Bitboard SilverEffectBB[SQ_NB_PLUS1][COLOR_NB];
	extern Bitboard KnightEffectBB[SQ_NB_PLUS1][COLOR_NB];
	extern Bitboard PawnEffectBB[SQ_NB_PLUS1][COLOR_NB];

	
	extern Bitboard LanceStepEffectBB[SQ_NB_PLUS1][COLOR_NB];
	extern Bitboard BishopStepEffectBB[SQ_NB_PLUS1];
	extern Bitboard RookStepEffectBB[SQ_NB_PLUS1];
}











template <Color c>
inline Bitboard pawnEffect(Square sq)
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return BB_Table::PawnEffectBB[sq][c];
}


inline Bitboard pawnEffect(Color c, Square sq)
{
	return (c == BLACK) ? pawnEffect<BLACK>(sq) : pawnEffect<WHITE>(sq);
}




template <Color C>
inline Bitboard pawnBbEffect(const Bitboard& bb)
{
	
	ASSERT_LV3(is_ok(C));
	return
		C == BLACK ? (bb >> 1) :
		C == WHITE ? (bb << 1) :
		Bitboard(ZERO);
}


inline Bitboard pawnBbEffect(Color c, const Bitboard& bb)
{
	return (c == BLACK) ? pawnBbEffect<BLACK>(bb) : pawnBbEffect<WHITE>(bb);
}



inline Bitboard knightEffect(const Color c, const Square sq)
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return BB_Table::KnightEffectBB[sq][c];
}


template <Color C>
inline Bitboard knightEffect(const Square sq)
{
	ASSERT_LV3(is_ok(C) && sq <= SQ_NB);
	return BB_Table::KnightEffectBB[sq][C];
}


inline Bitboard silverEffect(const Color c, const Square sq)
{
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return BB_Table::SilverEffectBB[sq][c];
}


template <Color C>
inline Bitboard silverEffect(const Square sq)
{
	ASSERT_LV3(is_ok(C) && sq <= SQ_NB);
	return BB_Table::SilverEffectBB[sq][C];
}


inline Bitboard goldEffect(const Color c, const Square sq) {
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return BB_Table::GoldEffectBB[sq][c];
}


template <Color C>
inline Bitboard goldEffect(const Square sq) {
	ASSERT_LV3(is_ok(C) && sq <= SQ_NB);
	return BB_Table::GoldEffectBB[sq][C];
}



inline Bitboard kingEffect(const Square sq)
{
	ASSERT_LV3(sq <= SQ_NB);
	return BB_Table::KingEffectBB[sq];
}










inline Bitboard lanceStepEffect(Color c, Square sq) {
	ASSERT_LV3(is_ok(c) && sq <= SQ_NB);
	return BB_Table::LanceStepEffectBB[sq][c];
}

template <Color C>
inline Bitboard lanceStepEffect(Square sq) {
	ASSERT_LV3(is_ok(C) && sq <= SQ_NB);
	return BB_Table::LanceStepEffectBB[sq][C];
}


inline Bitboard bishopStepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	return BB_Table::BishopStepEffectBB[sq];
}


inline Bitboard rookStepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	return BB_Table::RookStepEffectBB[sq];
}


inline Bitboard horseStepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	
	return BB_Table::BishopStepEffectBB[sq] | BB_Table::KingEffectBB[sq];
}


inline Bitboard dragonStepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	
	return BB_Table::RookStepEffectBB[sq] | BB_Table::KingEffectBB[sq];
}


inline Bitboard queenStepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	return rookStepEffect(sq) | bishopStepEffect(sq);
}


inline Bitboard cross00StepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	return rookStepEffect(sq) & kingEffect(sq);
}


inline Bitboard cross45StepEffect(Square sq) {
	ASSERT_LV3(sq <= SQ_NB);
	return bishopStepEffect(sq) & kingEffect(sq);
}









template <Color C>
inline Bitboard lanceEffect(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(is_ok(C) && sq <= SQ_NB);

	
#if 0



	if (C == WHITE)
	{
		
		const Bitboard mask(0x3fdfeff7fbfdfeffULL, 0x000000000001feffULL);

		
		Bitboard em = occupied.andnot(mask);

		
		
		
		
		
		
		
		
		
		
		
		
		

		
		
		
		

		Bitboard t = em + pawnEffect<C>(sq);

		
		return t ^ em;

	}
	else {

		
		const Bitboard se = lanceStepEffect<C>(sq);

		
		Bitboard mocc = se & occupied;

		
		
		mocc |= mocc >> 1;
		mocc |= mocc >> 2;
		mocc |= mocc >> 4;
		mocc >>= 1;

		return mocc.andnot(se);
	}
#endif

#if 1
	
	
	if (C == WHITE)
	{
		

		if (Bitboard::part(sq) == 0)
		{
			
			
			
			
			

			u64 mask = lanceStepEffect<WHITE>(sq).template extract64<0>();
			u64 em = occupied.extract64<0>() & mask;
			u64 t = em - 1; 
			return Bitboard((em ^ t) & mask, 0);
		}
		else {
			
			

			u64 mask = lanceStepEffect<WHITE>(sq).template extract64<1>();
			u64 em = occupied.extract64<1>() & mask;
			u64 t = em - 1;
			return Bitboard(0, (em ^ t) & mask);
		}
	}
	else {
		

		if (Bitboard::part(sq) == 0)
		{
			
			u64 se = lanceStepEffect<C>(sq).template extract64<0>();
			u64 mocc = se & occupied.extract64<0>();
			
			mocc = ~uint64_t{0} << MSB64(mocc | 1);
			return Bitboard(mocc & se, 0);
		}
		else {
			
			u64 se = lanceStepEffect<C>(sq).template extract64<1>();
			u64 mocc = se & occupied.extract64<1>();
			mocc = ~uint64_t{0} << MSB64(mocc | 1);
			return Bitboard(0, mocc & se);
		}
	}
#endif
}


inline Bitboard lanceEffect(Color c, Square sq, const Bitboard& occupied)
{
	return (c == BLACK) ? lanceEffect<BLACK>(sq, occupied) : lanceEffect<WHITE>(sq, occupied);
}



inline Bitboard rookFileEffect(Square sq, const Bitboard& occupied)
{
	ASSERT_LV3(sq <= SQ_NB);

	if (Bitboard::part(sq) == 0)
	{
		

		
		u64 mask = lanceStepEffect<WHITE>(sq).template extract64<0>();
		u64 em = occupied.extract64<0>() & mask;
		u64 t = em - 1; 

		
		u64 se = lanceStepEffect<BLACK>(sq).template extract64<0>();
		u64 mocc = se & occupied.extract64<0>();
		mocc = ~uint64_t{0} << MSB64(mocc | 1);

		
		return Bitboard(((em ^ t) & mask) | (mocc & se), 0);
	}
	else {
		
		

		u64 mask = lanceStepEffect<WHITE>(sq).template extract64<1>();
		u64 em = occupied.extract64<1>() & mask;
		u64 t = em - 1;

		u64 se = lanceStepEffect<BLACK>(sq).template extract64<1>();
		u64 mocc = se & occupied.extract64<1>();
		mocc = ~uint64_t{0} << MSB64(mocc | 1);

		return Bitboard(0, ((em ^ t) & mask) | (mocc & se));
	}
}




Bitboard rookRankEffect(Square sq, const Bitboard& occupied);


inline Bitboard rookEffect(const Square sq, const Bitboard& occupied) {
	
	return rookRankEffect(sq, occupied) | rookFileEffect(sq, occupied);
}


Bitboard bishopEffect(const Square sq, const Bitboard& occupied);


inline Bitboard horseEffect(Square sq, const Bitboard& occupied)
{
	return bishopEffect(sq, occupied) | kingEffect(sq);
}


inline Bitboard dragonEffect(Square sq, const Bitboard& occupied)
{
	return rookEffect(sq, occupied) | kingEffect(sq);
}










namespace BB_Table { extern Bitboard QUGIY_STEP_EFFECT[Effect8::DIRECT_NB - 2][SQ_NB_PLUS1]; }


template <Effect8::Direct D>
Bitboard rayEffect(Square sq, const Bitboard& occupied)
{
	
	
	if (D == Effect8::DIRECT_U) return lanceEffect<BLACK>(sq, occupied);
	if (D == Effect8::DIRECT_D) return lanceEffect<WHITE>(sq, occupied);

	ASSERT_LV3(D < Effect8::DIRECT_NB);

	
	const int dd = D > Effect8::DIRECT_D ? D - 2 : D;
	const Bitboard mask = BB_Table::QUGIY_STEP_EFFECT[dd][sq];

	
	constexpr bool reverse =
		   D == Effect8::DIRECT_RU
		|| D == Effect8::DIRECT_R
		|| D == Effect8::DIRECT_RD;

	Bitboard bb = occupied;

	if (reverse)
		bb = bb.byte_reverse();

	
	bb &= mask;
	
	Bitboard bb_minus_one = bb.decrement();
	
	bb = (bb ^ bb_minus_one) & mask;

	if (reverse)
		bb = bb.byte_reverse();

	return bb;
}


Bitboard directEffect(Square sq, Effect8::Direct d, const Bitboard& occupied);







Bitboard effects_from(Piece pc, Square sq);



Bitboard effects_from(Piece pc, Square sq, const Bitboard& occ);











template<Color C>
inline Bitboard pawn_attacks_bb(Bitboard b)
{
	return pawnBbEffect<C>(b);
}

inline Bitboard pawn_attacks_bb(Color c, Square s)
{
	return pawnEffect(c, s);
}









template<Piece PC>
inline Bitboard attacks_bb(Square s) {

	return effects_from(PC, s);
}









template<Piece PC>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {

	return effects_from(PC, s, occupied);
}







#if STOCKFISH
inline Square lsb(Bitboard b) {
#else
inline int8_t lsb(uint64_t b) {
#endif
	assert(b);

#if defined(__GNUC__)  

    return int8_t(__builtin_ctzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  

    unsigned long idx;
    _BitScanForward64(&idx, b);
    return int8_t(idx);

    #else  
    unsigned long idx;

    if (b & 0xffffffff)
    {
        _BitScanForward(&idx, int32_t(b));
        return int8_t(idx);
    }
    else
    {
        _BitScanForward(&idx, int32_t(b >> 32));
        return int8_t(idx + 32);
    }
    #endif
#else  
    #error "Compiler not supported."
#endif
}




#if STOCKFISH
inline Square msb(Bitboard b) {
#else
inline int8_t msb(uint64_t b) {
#endif
    assert(b);

#if defined(__GNUC__)  

    return int8_t(63 ^ __builtin_clzll(b));

#elif defined(_MSC_VER)
    #ifdef _WIN64  

    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return int8_t(idx);

    #else  

    unsigned long idx;

    if (b >> 32)
    {
        _BitScanReverse(&idx, int32_t(b >> 32));
        return int8_t(idx + 32);
    }
    else
    {
        _BitScanReverse(&idx, int32_t(b));
        return int8_t(idx);
    }
    #endif
#else  
    #error "Compiler not supported."
#endif
}








inline Bitboard least_significant_square_bb(Bitboard b) {
	
	
	

	u64 q0 = b.extract64<0>();
	u64 q1 = b.extract64<1>();
	return (q0 != 0) ? Bitboard(q0 & -q0 , 0) : Bitboard(0, q1 & -q1);
}

} 

#endif 
