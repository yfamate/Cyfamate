#include "movepick.h"
#if defined(USE_MOVE_PICKER)

#include <algorithm>

#include <iterator>
#include <utility>

#include "bitboard.h"
#include "position.h"


namespace YaneuraOu {
using namespace Eval; 

namespace {





  





enum Stages: int {

	
	
	

	MAIN_TT,					
	CAPTURE_INIT,				
	GOOD_CAPTURE,				
	QUIET_INIT,					
	GOOD_QUIET,					
	BAD_CAPTURE,				
	BAD_QUIET,                  

	
	
	

	
	
	

	EVASION_TT,					
	EVASION_INIT,				
	EVASION,					

	
	
	

	PROBCUT_TT,					
	PROBCUT_INIT,				
	PROBCUT,					

	
	
	

	QSEARCH_TT,					
	QCAPTURE_INIT,				
	QCAPTURE,					
};















#if defined(__AVX512F__) && defined(__AVX512DQ__)




struct CyfMoveSorter16 {
	static constexpr int MAX_ELEMENTS = 16;
	__m512i sortedValues, sortedMoves;

	explicit CyfMoveSorter16(const ExtMove& first) {
		sortedMoves  = _mm512_set1_epi32(int(first.to_u32()));
		
		sortedValues = _mm512_mask_set1_epi32(_mm512_set1_epi32(first.value), __mmask16(~1),
		                                      std::numeric_limits<int>::min());
	}

	void insert(const ExtMove& m) {
		const __m512i move  = _mm512_set1_epi32(int(m.to_u32()));   
		const __m512i value = _mm512_set1_epi32(m.value);
		
		const __mmask16 expand = _kadd_mask16(_mm512_cmplt_epi32_mask(sortedValues, value), __mmask16(-1));
		sortedValues = _mm512_mask_expand_epi32(value, expand, sortedValues);
		sortedMoves  = _mm512_mask_expand_epi32(move, expand, sortedMoves);
	}

	void write_sorted(ExtMove* moves, std::ptrdiff_t count) const {
		static_assert(sizeof(ExtMove) == 8 && std::is_trivially_copyable_v<ExtMove>);
		auto write = [&](int offset, const __m512i indices) {
			const __m512i        extMoves   = _mm512_permutex2var_epi32(sortedMoves, indices, sortedValues);
			const std::ptrdiff_t storeCount = count - offset;
			if (storeCount > 0)
				_mm512_mask_storeu_epi64(moves + offset, __mmask8((1u << std::min<std::ptrdiff_t>(storeCount, 8)) - 1),
				                         extMoves);
		};
		write(0, _mm512_setr_epi32(0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23));
		write(8, _mm512_setr_epi32(8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31));
	}
};
#endif

void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {

	ExtMove *sortedEnd = begin, *p = begin + 1;

#if defined(__AVX512F__) && defined(__AVX512DQ__)
	if (begin == end)
		return;

	
	
	if (end - begin > 4)
	{
		CyfMoveSorter16 sorter(*begin);
		for (; p < end; ++p)
			if (p->value >= limit)
			{
				
				if (sortedEnd - begin + 1 >= CyfMoveSorter16::MAX_ELEMENTS || p->value == std::numeric_limits<int>::min())
					break;
				sorter.insert(*p);
				*p = *++sortedEnd;
			}
		sorter.write_sorted(begin, sortedEnd - begin + 1);
	}
#endif

	for (; p < end; ++p)
		if (p->value >= limit)
		{
			ExtMove tmp = *p, *q;
			*p          = *++sortedEnd;
			for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
				
				
				
				*q = *(q - 1);
			*q = tmp;
		}
}







ExtMove* partial_insertion_select(ExtMove* begin, ExtMove* end, int limit) {
	if (begin == end)
		return begin;

	ExtMove* sortedEnd = begin;
#if defined(__AVX512F__)
	
	
	
	
	
	static_assert(sizeof(ExtMove) == 8 && std::is_trivially_copyable_v<ExtMove>);
	const __m512i lim = _mm512_set1_epi64(int64_t(limit) * (int64_t(1) << 32));
	for (std::ptrdiff_t off = 1; off < end - begin; off += 64)   
	{
		ExtMove* const base = begin + off;
		const int n   = int(std::min<std::ptrdiff_t>(64, end - base));
		uint64_t  sel = 0;
		int       i   = 0;
		for (; i + 8 <= n; i += 8)
			sel |= uint64_t(_mm512_cmpge_epi64_mask(_mm512_loadu_si512(base + i), lim)) << i;
		if (i < n)
		{
			const __mmask8 k = __mmask8((1u << (n - i)) - 1);
			sel |= uint64_t(_mm512_mask_cmpge_epi64_mask(k, _mm512_maskz_loadu_epi64(k, base + i), lim)) << i;
		}
		while (sel)
		{
			ExtMove* p = base + __builtin_ctzll(sel);
			sel &= sel - 1;
			ExtMove tmp = *p;
			*p          = *++sortedEnd;
			*sortedEnd  = tmp;
		}
	}
#else
	for (ExtMove* p = begin + 1; p < end; ++p)
		if (p->value >= limit)
		{
			ExtMove tmp = *p;
			*p          = *++sortedEnd;
			*sortedEnd  = tmp;
		}
#endif
	return sortedEnd + 1;
}






void stable_sort_moves(ExtMove* first, ExtMove* last) {
	constexpr std::ptrdiff_t Run = 16;
	const std::ptrdiff_t     n   = last - first;
	if (n <= Run)
	{
		partial_insertion_sort(first, last, std::numeric_limits<int>::min());
		return;
	}
	for (std::ptrdiff_t lo = 0; lo < n; lo += Run)   
		partial_insertion_sort(first + lo, first + std::min(lo + Run, n), std::numeric_limits<int>::min());

	ExtMove  buf[MAX_MOVES];   
	ExtMove *src = first, *dst = buf;
	for (std::ptrdiff_t w = Run; w < n; w *= 2)
	{
		for (std::ptrdiff_t lo = 0; lo < n; lo += 2 * w)
		{
			const ExtMove *      a = src + lo, *b = src + std::min(lo + w, n);
			const ExtMove *const aEnd = b, *const bEnd = src + std::min(lo + 2 * w, n);
			ExtMove*             out  = dst + lo;
			while (a < aEnd && b < bEnd)
			{
				const bool takeB = b->value > a->value;
				*out++ = takeB ? *b : *a;
				a += !takeB;
				b += takeB;
			}
			out = std::copy(a, aEnd, out);
			std::copy(b, bEnd, out);
		}
		std::swap(src, dst);
	}
	if (src != first)
		std::copy(src, src + n, first);
}



inline void shift_moves_up(ExtMove* first, ExtMove* last) {
	for (ExtMove* p = last; p != first; --p)
		*p = *(p - 1);
}




#ifndef CYF_BX_PICKS
#define CYF_BX_PICKS 1
#endif
ExtMove* first_max_value(ExtMove* first, ExtMove* last) {
#if defined(__AVX512F__)
	
	
	static_assert(sizeof(ExtMove) == 8 && std::is_trivially_copyable_v<ExtMove>);
	const std::ptrdiff_t n    = last - first;
	const __mmask8       tail = __mmask8((1u << (n & 7)) - 1);
	const __m512i  low  = _mm512_set1_epi32(std::numeric_limits<int>::min());
	__m512i        vmax = low;
	std::ptrdiff_t i    = 0;
	for (; i + 8 <= n; i += 8)
		vmax = _mm512_max_epi32(vmax, _mm512_loadu_si512(first + i));
	if (tail)
		vmax = _mm512_max_epi32(vmax, _mm512_mask_loadu_epi64(low, tail, first + i));
	const __m512i best = _mm512_set1_epi32(_mm512_mask_reduce_max_epi32(__mmask16(0xAAAA), vmax));
	for (i = 0;; i += 8)
	{
		const __m512i   v  = i + 8 <= n ? _mm512_loadu_si512(first + i) : _mm512_mask_loadu_epi64(low, tail, first + i);
		const __mmask16 eq = _mm512_mask_cmpeq_epi32_mask(__mmask16(0xAAAA), v, best);
		if (eq)
			return first + i + (__builtin_ctz(eq) >> 1);
	}
#else
	int best = first->value;
	for (const ExtMove* q = first + 1; q < last; ++q)
		best = std::max(best, q->value);
	ExtMove* q = first;
	while (q->value != best)
		++q;
	return q;
#endif
}






#if defined(__AVX512F__)
template<int NR, int K, int J>
static inline __attribute__((always_inline)) void cyf_bitonic_level(__m512i* v) {
	if constexpr (J >= 8)
	{
		constexpr int RJ = J >> 3;
#pragma clang loop unroll(full)
		for (int r = 0; r < NR; ++r)
		{
			const int p = r ^ RJ;
			if (p < r)
				continue;
			const bool    desc = ((r * 8) & K) == 0;
			const __m512i mx = _mm512_max_epi64(v[r], v[p]), mn = _mm512_min_epi64(v[r], v[p]);
			v[r] = desc ? mx : mn;
			v[p] = desc ? mn : mx;
		}
	}
	else
	{
		constexpr unsigned lowerMask = J == 4 ? 0x0F : J == 2 ? 0x33 : 0x55;
#pragma clang loop unroll(full)
		for (int r = 0; r < NR; ++r)
		{
			const unsigned descMask = K >= 8 ? (((r * 8) & K) == 0 ? 0xFF : 0x00) : (K == 4 ? 0x0F : 0x33);
			const __mmask8 takeMax  = __mmask8(~(lowerMask ^ descMask) & 0xFF);
			__m512i s;
			if constexpr (J == 4)
				s = _mm512_shuffle_i64x2(v[r], v[r], _MM_SHUFFLE(1, 0, 3, 2));
			else if constexpr (J == 2)
				s = _mm512_permutex_epi64(v[r], _MM_SHUFFLE(1, 0, 3, 2));
			else
				s = _mm512_shuffle_epi32(v[r], _MM_PERM_ENUM(_MM_SHUFFLE(1, 0, 3, 2)));
			v[r] = _mm512_mask_blend_epi64(takeMax, _mm512_min_epi64(v[r], s), _mm512_max_epi64(v[r], s));
		}
	}
	if constexpr (J > 1)
		cyf_bitonic_level<NR, K, J / 2>(v);
}

template<int NR, int K = 2>
static inline __attribute__((always_inline)) void cyf_bitonic_desc_i64(__m512i* v) {
	cyf_bitonic_level<NR, K, K / 2>(v);
	if constexpr (K < NR * 8)
		cyf_bitonic_desc_i64<NR, K * 2>(v);
}

template<int NR>
static __attribute__((noinline)) void cyf_vsort_moves_n(ExtMove* first, int n) {
	static_assert(sizeof(ExtMove) == 8 && std::is_trivially_copyable_v<ExtMove>);
	__m512i       v[NR];
	const __m512i hi    = _mm512_set1_epi64(int64_t(0xFFFFFFFF00000000ull));
	const __m512i pad   = _mm512_set1_epi64(std::numeric_limits<int64_t>::min());
	const __m512i lane  = _mm512_setr_epi64(0, 1, 2, 3, 4, 5, 6, 7);
	const __m512i top   = _mm512_set1_epi64(int64_t(0xFFFFFFFFll));
	for (int r = 0; r < NR; ++r)
	{
		const int      rem  = n - r * 8;
		const __mmask8 k    = rem >= 8 ? __mmask8(0xFF) : rem <= 0 ? __mmask8(0) : __mmask8((1u << rem) - 1);
		const __m512i  x    = _mm512_maskz_loadu_epi64(k, first + r * 8);
		const __m512i  idx  = _mm512_add_epi64(lane, _mm512_set1_epi64(r * 8));
		const __m512i  key  = _mm512_or_si512(_mm512_and_si512(x, hi), _mm512_sub_epi64(top, idx));
		v[r]                = _mm512_mask_mov_epi64(pad, k, key);
	}
	cyf_bitonic_desc_i64<NR>(v);
	const int nr = (n + 7) / 8;
	__m512i   out[NR];
	for (int r = 0; r < nr; ++r)
	{
		const int      rem = n - r * 8;
		const __mmask8 k   = rem >= 8 ? __mmask8(0xFF) : __mmask8((1u << rem) - 1);
		const __m512i  idx = _mm512_sub_epi64(top, _mm512_and_si512(v[r], top));
		out[r]             = _mm512_mask_i64gather_epi64(_mm512_setzero_si512(), k, idx, (const long long*) first, 8);
	}
	for (int r = 0; r < nr; ++r)
	{
		const int      rem = n - r * 8;
		const __mmask8 k   = rem >= 8 ? __mmask8(0xFF) : __mmask8((1u << rem) - 1);
		_mm512_mask_storeu_epi64(first + r * 8, k, out[r]);
	}
}


static inline bool cyf_vsort_moves(ExtMove* first, ExtMove* last) {
	const std::ptrdiff_t n = last - first;
	if (n <= 1)
		return true;
	if (n <= 16)
		cyf_vsort_moves_n<2>(first, int(n));
	else if (n <= 32)
		cyf_vsort_moves_n<4>(first, int(n));
	else if (n <= 64)
		cyf_vsort_moves_n<8>(first, int(n));
	else if (n <= 128)
		cyf_vsort_moves_n<16>(first, int(n));
	else if (n <= 256)
		cyf_vsort_moves_n<32>(first, int(n));
	else
		return false;
	return true;
}
#endif

#if defined(__AVX512F__)


static void cyf_stable_sort_moves_v(ExtMove* first, ExtMove* last) {
	constexpr std::ptrdiff_t Run = 64;
	const std::ptrdiff_t     n   = last - first;
	if (n <= Run)
	{
		cyf_vsort_moves(first, last);
		return;
	}
	for (std::ptrdiff_t lo = 0; lo < n; lo += Run)
		cyf_vsort_moves(first + lo, first + std::min(lo + Run, n));

	ExtMove  buf[MAX_MOVES];
	ExtMove *src = first, *dst = buf;
	for (std::ptrdiff_t w = Run; w < n; w *= 2)
	{
		for (std::ptrdiff_t lo = 0; lo < n; lo += 2 * w)
		{
			const ExtMove *      a = src + lo, *b = src + std::min(lo + w, n);
			const ExtMove *const aEnd = b, *const bEnd = src + std::min(lo + 2 * w, n);
			ExtMove*             out  = dst + lo;
			while (a < aEnd && b < bEnd)
			{
				const bool takeB = b->value > a->value;
				*out++ = takeB ? *b : *a;
				a += !takeB;
				b += takeB;
			}
			out = std::copy(a, aEnd, out);
			std::copy(b, bEnd, out);
		}
		std::swap(src, dst);
	}
	if (src != first)
		std::copy(src, src + n, first);
}
#endif

} 












MovePicker::MovePicker(const Position&              p,
                       Move                         ttm,
                       Depth                        d,
                       const ButterflyHistory*      mh,
                       const LowPlyHistory*         lph,
                       const CapturePieceToHistory* cph,
                       const PieceToHistory**       ch,
                       const SharedHistories*       sh,
                       int                          pl
#if !STOCKFISH
                       ,bool generate_all_legal_moves
#endif
                       ) :
    pos(p),
    mainHistory(mh),
    lowPlyHistory(lph),
    captureHistory(cph),
    continuationHistory(ch),
    sharedHistory(sh),
    ttMove(ttm),
    depth(d),
    ply(pl)
#if STOCKFISH
#else
    ,
    generate_all_legal_moves(generate_all_legal_moves)
#endif
{
    
    

#if STOCKFISH
	if (pos.in_check())
        
		stage = EVASION_TT + !(ttm && pos.pseudo_legal(ttm));

    else
        
        
        stage = (depth > 0 ? MAIN_TT : QSEARCH_TT)
              + !(ttm && pos.pseudo_legal(ttm));
#else
	

    if (pos.in_check())
        
        stage = EVASION_TT + !(ttm && pos.pseudo_legal(ttm, generate_all_legal_moves));

    else
        
        
        stage = (depth > 0 ? MAIN_TT : QSEARCH_TT)
              + !(ttm && pos.pseudo_legal(ttm, generate_all_legal_moves));
#endif

	
    
    
    
    
    
    

    
    
    

    
    
    

    

    

    
    
}









MovePicker::MovePicker(const Position& p, Move ttm, int th, const CapturePieceToHistory* cph
#if STOCKFISH
#else
    , bool generate_all_legal_moves
#endif

                       ) :
    pos(p),
    captureHistory(cph),
    ttMove(ttm),
    threshold(Value(th))
#if STOCKFISH
#else
    ,generate_all_legal_moves(generate_all_legal_moves)
#endif

    {

    
    ASSERT_LV3(!pos.in_check());

    
    
    

    stage = PROBCUT_TT
      + !(
        ttm
#if STOCKFISH        
        && pos.capture_stage(ttm)
        && pos.pseudo_legal(ttm) 
#else
        && pos.capture(ttm)
        
        
        
        && pos.pseudo_legal(ttm, generate_all_legal_moves) 
#endif
	    );
    
    
}











template<GenType Type>
ExtMove* MovePicker::score(const MoveList<Type>& ml) {
    if constexpr (Type == QUIETS || Type == QUIETS_ALL || Type == EVASIONS || Type == EVASIONS_ALL) {
        if (ply == 0 && CyfRootHint::tracing()) {
            rootTrace = std::make_unique<RootHintTrace>();
            rootTrace->evasion = Type == EVASIONS || Type == EVASIONS_ALL;
            ++CyfRootHint::state().go.trace_frames;
        }
    }

#if STOCKFISH
	static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");
#else
    static_assert(Type == CAPTURES || Type == CAPTURES_ALL || Type == QUIETS || Type == QUIETS_ALL
                    || Type == EVASIONS || Type == EVASIONS_ALL,
                  "Wrong type");
#endif

	Color us = pos.side_to_move();

	

#if STOCKFISH
    [[maybe_unused]] Bitboard threatByLesser[KING + 1];
	if constexpr (Type == QUIETS)
#else
    if constexpr (Type == QUIETS || Type == QUIETS_ALL)
#endif
	{
#if STOCKFISH
        threatByLesser[PAWN]   = 0;
        threatByLesser[KNIGHT] = threatByLesser[BISHOP] = pos.attacks_by<PAWN>(~us);
        threatByLesser[ROOK] =
          pos.attacks_by<KNIGHT>(~us) | pos.attacks_by<BISHOP>(~us) | threatByLesser[KNIGHT];
        threatByLesser[QUEEN] = pos.attacks_by<ROOK>(~us) | threatByLesser[ROOK];
        threatByLesser[KING]  = 0;

#else

#if 0
		
		

		
		
		threatenedByPawn = (~us == BLACK) ? pos.attacks_by<BLACK, PAWN>() : pos.attacks_by<WHITE, PAWN>();

		
		threatened =  (pos.pieces(us,PAWN).andnot(pos.pieces(us))                 & threatenedByPawn );
#endif
		

#endif
	}

    ExtMove* it = cur;
    
    [[maybe_unused]] decltype(&sharedHistory->pawn_entry(pos)) pawnHist = nullptr;
    if constexpr (Type == QUIETS || Type == QUIETS_ALL)
        pawnHist = &sharedHistory->pawn_entry(pos);

    
    
    
    
    [[maybe_unused]] const PieceToHistory *ch0 = nullptr, *ch1 = nullptr, *ch2 = nullptr, *ch3 = nullptr, *ch5 = nullptr;
    [[maybe_unused]] std::remove_reference_t<decltype((*mainHistory)[us])>*  mainHist = nullptr;
    [[maybe_unused]] std::remove_reference_t<decltype((*lowPlyHistory)[0])>* lowPly   = nullptr;  
    [[maybe_unused]] const StateInfo* const st      = pos.state();
    [[maybe_unused]] const Bitboard*        checkSq = nullptr;   
    [[maybe_unused]] const int              plyHere = (Type == QUIETS || Type == QUIETS_ALL) ? ply : 0;   
    RootHintTrace* const                    trace   = rootTrace.get();
    if constexpr (Type == QUIETS || Type == QUIETS_ALL)
    {
        mainHist = &(*mainHistory)[us];
        ch0      = continuationHistory[0];
        ch1      = continuationHistory[1];
        ch2      = continuationHistory[2];
        ch3      = continuationHistory[3];
        ch5      = continuationHistory[5];
        if (plyHere < LOW_PLY_HISTORY_SIZE)
            lowPly = &(*lowPlyHistory)[plyHere];
        
        checkSq = pos.check_squares_all();

        
        
        
        
        
        
        
        
        
        static_assert(std::remove_pointer_t<decltype(pawnHist)>::index(1, 0) == PieceToHistory::index(1, 0)
                      && std::remove_pointer_t<decltype(pawnHist)>::index(0, 1) == PieceToHistory::index(0, 1));
        const int wPawn = cyfMpWPawn, w5 = cyfMpW5;   
        auto scoreQuiets = [&](auto lowPlyDiv) -> ExtMove* {
            constexpr int Div = decltype(lowPlyDiv)::value;   
            const Move* const mlBegin = ml.begin();
            const int         n       = int(ml.size());
            uint16_t          checkIdx[MAX_MOVES];
            int               nCheck = 0;
#ifndef CYF_BX_PREFETCH
#define CYF_BX_PREFETCH 2   
#endif
#if CYF_BX_PREFETCH > 0
            
            
            
            for (int i = 0; i < n; ++i)
            {
                const Move        mm = mlBegin[i];
                const std::size_t kk = PieceToHistory::index(pos.moved_piece(mm), mm.to_sq());
                _mm_prefetch((const char*) &pawnHist->flat(kk), _MM_HINT_T0);
#if CYF_BX_PREFETCH > 1
                _mm_prefetch((const char*) &ch0->flat(kk), _MM_HINT_T0);
                _mm_prefetch((const char*) &ch1->flat(kk), _MM_HINT_T0);
                _mm_prefetch((const char*) &ch2->flat(kk), _MM_HINT_T0);
                _mm_prefetch((const char*) &ch3->flat(kk), _MM_HINT_T0);
                _mm_prefetch((const char*) &ch5->flat(kk), _MM_HINT_T0);
#endif
            }
#endif
            for (int i = 0; i < n; ++i)
            {
                ExtMove& m = it[i];
                m          = mlBegin[i];

                const Square    to = m.to_sq();
                const Piece     pc = pos.moved_piece(m);
                const PieceType pt = type_of(pc);

                
                const std::size_t k = PieceToHistory::index(pc, to);
                int v  =  1 * (*mainHist)[m.raw()];
                v     +=  (wPawn * int(pawnHist->flat(k))) >> 3;   
                v     +=      ch0->flat(k);
                v     +=      ch1->flat(k);
                v     +=      ch2->flat(k);
                v     +=      ch3->flat(k);
                v     +=  (w5 * int(ch5->flat(k))) >> 3;           
                if constexpr (Div != 0)
                    v += 10 * (*lowPly)[m.raw()] / Div;
                m.value = v;

                if (checkSq[pt] & to)   
                    checkIdx[nCheck++] = uint16_t(i);
            }
            for (int k = 0; k < nCheck; ++k)
            {
                ExtMove& m = it[checkIdx[k]];
                m.value += pos.see_ge(m, -63) * 26355;
            }
            if (cyfMpPawnThreat)
            {
                const Bitboard pawnAtt = pawnBbEffect(~us, pos.pieces(~us, PAWN));   
                for (int k = 0; k < n; ++k)
                    if (pawnAtt & it[k].to_sq())
                        it[k].value -= cyfMpPawnThreat;
            }
            return it + n;
        };
        static_assert(LOW_PLY_HISTORY_SIZE == 5, "one case per ply below LOW_PLY_HISTORY_SIZE");
        if (!trace && plyHere > 0)
            switch (plyHere)
            {
            case 1 : return scoreQuiets(std::integral_constant<int, 2>{});
            case 2 : return scoreQuiets(std::integral_constant<int, 3>{});
            case 3 : return scoreQuiets(std::integral_constant<int, 4>{});
            case 4 : return scoreQuiets(std::integral_constant<int, 5>{});
            default: return scoreQuiets(std::integral_constant<int, 0>{});   
            }
    }

    for (auto move : ml)
    {
        ExtMove& m = *it++;
        m          = move;

		const Square    from          = m.from_sq();
		const Square    to            = m.to_sq();
		const Piece     pc            = pos.moved_piece(m);
		const PieceType pt            = type_of(pc);
		const Piece     capturedPiece = pos.piece_on(to);

		if constexpr (Type == CAPTURES)
		{
			
			
			
			
			
			
			

			
			
			
			
			

			
			m.value = (*captureHistory)[pc][to][type_of(capturedPiece)]
						+ int(Eval::PieceValue[capturedPiece]) * cyfMpCapPV / 64;   


			
			
			
			
			
			
			
		}
#if STOCKFISH
		else if constexpr (Type == QUIETS)
#else
        else if constexpr (Type == QUIETS || Type == QUIETS_ALL)
#endif
		{
			
			
			
			

			
			
			
			
			
			int v  =  1 * (*mainHist)[m.raw()];
			v     +=  (cyfMpWPawn * int((*pawnHist)[pc][to])) >> 3;   
			v     +=      (*ch0)[pc][to];
			v     +=      (*ch1)[pc][to];
			v     +=      (*ch2)[pc][to];
			v     +=      (*ch3)[pc][to];
			v     +=  (cyfMpW5 * int((*ch5)[pc][to])) >> 3;           

			
			v += ((checkSq[pt] & to) && pos.see_ge(m, -63)) * 26355;


			

#if STOCKFISH
			
			

			
			

			
			

            int v = 20 * (bool(threatByLesser[pt] & from) - bool(threatByLesser[pt] & to));
            m.value += PieceValue[pt] * v;

			
#endif

			
			
			if (lowPly)   
                v += 10 * (*lowPly)[m.raw()] / (1 + plyHere);

			
			if (plyHere == 0)
				v += CyfRootHint::scoring_bonus(m.raw());

			m.value = v;

			
		}
		else 
		{
			
			if (pos.capture_stage(m))
				m.value = PieceValue[capturedPiece] + (1 << 28);
			

			else
				
				m.value = (*mainHistory)[us][m.raw()] + (*continuationHistory[0])[pc][to]
				        + (ply == 0 ? CyfRootHint::scoring_bonus(m.raw()) : 0);   
		}

        if (trace) {
            ExtMove base = m;
            if (!trace->evasion || !pos.capture_stage(m)) base.value -= CyfRootHint::bonus(m.raw());
            trace->baseline.push_back(base);
        }
	}
    if (rootTrace) {
        auto& v = rootTrace->baseline;
        if (!v.empty()) partial_insertion_sort(v.data(), v.data() + v.size(),
            rootTrace->evasion ? std::numeric_limits<int>::min() : -5551 * depth);
        if (!rootTrace->evasion) std::stable_partition(v.begin(), v.end(), [](const ExtMove& m) { return m.value > -5907; });
    }
    return it;
}










template<typename Pred>
Move MovePicker::select(Pred filter) {

	for (; cur < endCur; ++cur)
		
		if (*cur != ttMove && filter())
			return *cur++;

	return Move::none();
}










#if defined(CYF_GPU_POOL)
std::pair<const ExtMove*, const ExtMove*> MovePicker::pool_candidates() const {
    switch (stage) {
    case GOOD_QUIET: case BAD_QUIET:
        
        
        const_cast<MovePicker*>(this)->finish_lazy_quiets();
        return {cur, endCur};
    case GOOD_CAPTURE: case BAD_CAPTURE: case EVASION:
        return {cur, endCur};
    default: return {nullptr, nullptr}; 
    }
}
#endif


inline char cyf_stage_code(int s) {
	switch (s) {
	case CAPTURE_INIT: return 'T';
	case GOOD_CAPTURE: return 'G';
	case GOOD_QUIET:   return 'Q';
	case BAD_CAPTURE:  return 'B';
	case BAD_QUIET:    return 'L';
	case EVASION_INIT: return 't';
	case EVASION:      return 'E';
	default:           return '?';
	}
}
Move MovePicker::next_move() {

	
	
	constexpr int goodQuietThreshold = -14000;



top:
    switch (stage)
    {

    
    case MAIN_TT :
    case EVASION_TT :
    case QSEARCH_TT :
    case PROBCUT_TT :
        ++stage;
        pos.fill_check_squares();   
        return ttMove;

    
    case CAPTURE_INIT :
    case PROBCUT_INIT :
    case QCAPTURE_INIT : {

#if STOCKFISH
        MoveList<CAPTURES> ml(pos);

        cur = endBadCaptures = moves;

        
        endCur = endCaptures = score<CAPTURES>(ml);
#else
        if (generate_all_legal_moves)
        {
            MoveList<CAPTURES_ALL> ml(pos);
            cur = endBadCaptures = moves;
            endCur = endCaptures = score<CAPTURES_ALL>(ml);
        }
        else
        {
            MoveList<CAPTURES> ml(pos);
            cur = endBadCaptures = moves;
            endCur = endCaptures = score<CAPTURES>(ml);
        }
#endif

        
        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());

        
        
        if (cur != endCur)
            pos.fill_check_squares();

        ++stage;
        goto top;
    }

	
	
	case GOOD_CAPTURE:
		if (select([&]() {
				
				
				if (pos.see_ge(*cur, -(cur->value - ((cyfMpCapSplit && cyfMpCapPV)
				                                      ? int(Eval::PieceValue[pos.piece_on(cur->to_sq())]) * cyfMpCapPV / 64 : 0)) / 15))
					return true;


				std::swap(*endBadCaptures++, *cur);
				
				return false;
			}))
			return *(cur - 1);

		++stage;
		[[fallthrough]];

	
	case QUIET_INIT:

		if (!skipQuiets)
		{
			

#if STOCKFISH
            MoveList<QUIETS> ml(pos);

            endCur = endGenerated = score<QUIETS>(ml);
#else
			if (generate_all_legal_moves)
			{
                MoveList<QUIETS_ALL> ml(pos);
                endCur = endGenerated = score<QUIETS_ALL>(ml);
			}
			else
			{
                MoveList<QUIETS> ml(pos);
                
                endCur = endGenerated = score<QUIETS>(ml);
			}
            
			
#endif						

			


			
			
			
			
			lazyQuietCur = cur;
			lazyQuietEnd = partial_insertion_select(cur, endCur, -5551 * depth);
		}

		++stage;
		[[fallthrough]];

		
		
		
	case GOOD_QUIET:
        if (!skipQuiets)
        {
            
            
            
            
            
            
            while (cur < endCur)
            {
                if (cur == lazyQuietCur && cur != lazyQuietEnd)
                {
                    if (lazyQuietPicks++ < CYF_BX_PICKS)
                    {
                        ExtMove* const best = first_max_value(cur, lazyQuietEnd);
                        const ExtMove  m    = *best;
                        shift_moves_up(cur, best);   
                        *cur = m;
                        ++lazyQuietCur;

                        
                        
                        if (m.value <= goodQuietThreshold)
                        {
                            cur = lazyQuietEnd;
                            continue;
                        }
                    }
                    else
                        finish_lazy_quiets();
                }
                if (*cur != ttMove && cur->value > goodQuietThreshold)
                    return *cur++;
                ++cur;
            }
        }

		
		
		

		cur    = moves;
        endCur = endBadCaptures;

		++stage;
		[[fallthrough]];

		
	case BAD_CAPTURE:
		if (select([]() { return true; }))
			return *(cur - 1);

		
		

        cur    = endCaptures;
		endCur = endGenerated;

		++stage;
		[[fallthrough]];

	case BAD_QUIET:
		if (!skipQuiets)
        {
            
            
            
            
            
            for (; cur < endCur; ++cur)
            {
                if (cur == lazyQuietCur && cur != lazyQuietEnd)
                {
                    if (lazyQuietPicks++ < CYF_BX_PICKS)
                    {
                        ExtMove* const best = first_max_value(cur, lazyQuietEnd);
                        const ExtMove  m    = *best;
                        shift_moves_up(cur, best);   
                        *cur = m;
                        ++lazyQuietCur;
                    }
                    else
                        finish_lazy_quiets();
                }
                if (*cur != ttMove && cur->value <= goodQuietThreshold)
                    return *cur++;
            }
            return Move::none();
        }

		return Move::none();

	
    case EVASION_INIT : {
#if STOCKFISH
        MoveList<EVASIONS> ml(pos);

        cur    = moves;
        endCur = endGenerated = score<EVASIONS>(ml);
#else
		if (generate_all_legal_moves)
		{
            MoveList<EVASIONS_ALL> ml(pos);
            cur    = moves;
            endCur = endGenerated = score<EVASIONS_ALL>(ml);
		}
		else
		{
            MoveList<EVASIONS> ml(pos);
            cur    = moves;
			
            endCur = endGenerated = score<EVASIONS>(ml);
		}
#endif

        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());

        pos.fill_check_squares();   

        ++stage;
        [[fallthrough]];
    }
		
	case EVASION:
		
	case QCAPTURE:
		
		return select([]() { return true; });

		
	case PROBCUT:
		return select([&]() { return pos.see_ge(*cur, threshold); });
		

	default:
		UNREACHABLE;
		return Move::none();
	}

	ASSERT(false);
	return Move::none(); 
}






inline Move MovePicker::next_move_qsearch() {
    if (stage == QCAPTURE || stage == EVASION)
    {
        for (; cur < endCur; ++cur)
            if (*cur != ttMove)
                return *cur++;
        return Move::none();
    }
    return next_move();
}

void MovePicker::skip_quiet_moves() { skipQuiets = true; }




void MovePicker::finish_lazy_quiets() {
    if (lazyQuietCur != lazyQuietEnd)
    {
#if defined(__AVX512F__) && (!defined(CYF_BX_VSORT) || CYF_BX_VSORT)
        cyf_stable_sort_moves_v(lazyQuietCur, lazyQuietEnd);   
#else
        stable_sort_moves(lazyQuietCur, lazyQuietEnd);
#endif
        
        lazyQuietCur = lazyQuietEnd;
    }
}

} 

#endif 
