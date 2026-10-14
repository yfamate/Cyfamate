#include "movepick.h"
#if defined(USE_MOVE_PICKER)

#include <algorithm>
//#include <cassert>
#include <iterator>
#include <utility>

#include "bitboard.h"
#include "position.h"


namespace YaneuraOu {
using namespace Eval; // Eval::PieceValue

namespace {



  
// -----------------------
//   指し手オーダリング
// -----------------------

// 指し手を段階的に生成するために現在どの段階にあるかの状態を表す定数
enum Stages: int {

	// -----------------------------------------------------
	//   王手がかっていない通常探索時用の指し手生成
	// -----------------------------------------------------

	MAIN_TT,					// 置換表の指し手を返すフェーズ
	CAPTURE_INIT,				// (CAPTURESの指し手生成)
	GOOD_CAPTURE,				// 捕獲する指し手(CAPTURES_PRO_PLUS)を生成して指し手を一つずつ返す。ただし、SEE値の悪い手(=BAD_CAPTURE)は後回し。
	QUIET_INIT,					// (QUIETの指し手生成)
	GOOD_QUIET,					// CAPTURES_PRO_PLUSで生成しなかった指し手を生成して、一つずつ返す。
	BAD_CAPTURE,				// 捕獲する  悪い指し手(将棋においてそこまで悪い手とは限らない)
	BAD_QUIET,                  // 捕獲しない悪い指し手(これはBAD_CAPTUREのあとに処理する)

	// 将棋ではBAD_CAPTUREをQUIET_の前にやったほうが良いという従来説は以下の実験データにより覆った。
	//  r300, 2585 - 62 - 2993(46.34% R - 25.46)[2016/08/19]
	// b1000, 1051 - 43 - 1256(45.56% R - 30.95)[2016/08/19]

	// -----------------------------------------------------
	//   王手がかっているときの静止探索/通常探索の指し手生成
	// -----------------------------------------------------

	EVASION_TT,					// 置換表の指し手を返すフェーズ
	EVASION_INIT,				// (EVASIONSの指し手を生成)
	EVASION,					// 回避する指し手(EVASIONS)を生成した指し手を一つずつ返す

	// -----------------------------------------------------
	//   通常探索のProbCutの処理のなかから呼び出される用
	// -----------------------------------------------------

	PROBCUT_TT,					// 置換表の指し手を返すフェーズ
	PROBCUT_INIT,				// (PROBCUTの指し手を生成)
	PROBCUT,					// 直前の指し手での駒の価値を上回る駒取りの指し手のみを生成するフェーズ

	// -----------------------------------------------------
	//   静止探索時用の指し手生成
	// -----------------------------------------------------

	QSEARCH_TT,					// 置換表の指し手を返すフェーズ
	QCAPTURE_INIT,				// (QCAPTUREの指し手生成)
	QCAPTURE,					// 捕獲する指し手 + 歩を成る指し手を一手ずつ返す
};

/*
状態遷移の順番は、
	王手がかかっていない時。
		通常探索時 : MAIN_TT    → CAPTURE_INIT  → GOOD_CAPTURE → GOOD_QUIET → BAD_CAPTURE → BAD_QUIET
		静止探索時 : QSEARCH_TT → QCAPTURE_INIT → QCAPTURE
		ProbCut時  : PROBCUT_TT → PROBCUT_INIT  → PROBCUT

		静止探索では、captureする指し手しか生成しないので単にスコア順に並び替えて順番に返せば良い。

	王手がかかっている時。
		通常探索、静止探索共通 : EVASION_TT → EVASION_INIT → EVASION

*/

// -----------------------
//   partial insertion sort
// -----------------------

// Sort moves in descending order up to and including
// a given limit. The order of moves smaller than the limit is left unspecified.
//
// partial_insertion_sort()は指し手を与えられたlimitより、ExtMove::valueが大きいものだけを降順でソートする。
// limitよりも小さい値の指し手の順序については、不定。(sortしたときに末尾のほうに移動する)
// 将棋だと指し手の数が多い(ことがある)ので、数が多いときは途中で打ち切ったほうがいいかも。
// 現状、全体時間の6.5～7.5%程度をこの関数で消費している。
// (長い時間思考させるとこの割合が増えてくる)
#if defined(__AVX512F__) && defined(__AVX512DQ__)
// Cyfamate: Stockfish 04ce2c04 ("Faster movepick sorting on AVX512ICL", no functional change). The first up to 16
// sorted moves live in two 512-bit registers (moves, values); an insertion is one compare plus two masked expands
// instead of the element-by-element backward shift. Only AVX512F/DQ instructions are used, so the deployed
// AVX512VNNI build qualifies. The permutation equals the scalar loop's, including the unsorted tail.
struct CyfMoveSorter16 {
	static constexpr int MAX_ELEMENTS = 16;
	__m512i sortedValues, sortedMoves;

	explicit CyfMoveSorter16(const ExtMove& first) {
		sortedMoves  = _mm512_set1_epi32(int(first.to_u32()));
		// Unused lanes hold INT_MIN so that they sort after every inserted move (callers never insert INT_MIN).
		sortedValues = _mm512_mask_set1_epi32(_mm512_set1_epi32(first.value), __mmask16(~1),
		                                      std::numeric_limits<int>::min());
	}

	void insert(const ExtMove& m) {
		const __m512i move  = _mm512_set1_epi32(int(m.to_u32()));   // raw() is only the 16-bit part
		const __m512i value = _mm512_set1_epi32(m.value);
		// All lanes except the insertion point: the first lane whose value is smaller than m.value.
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

	// [NPS-R 2026-10-08] One or two moves (half of the capture lists): the scalar loop below does at most one
	// compare, cheaper than setting up and writing back the sorter. Both produce the same stable sort.
	if (end - begin > 4)
	{
		CyfMoveSorter16 sorter(*begin);
		for (; p < end; ++p)
			if (p->value >= limit)
			{
				// Full sorter, or a value equal to the empty-lane marker: the scalar loop takes over from here.
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
				// ■ 備考
				// ここ、ExtMove同士の operator<()を呼び出している。
				// これはExtMove::valueを比較するように定義されている。
				*q = *(q - 1);
			*q = tmp;
		}
}

// [NPS 2026-10-08] The selection step of partial_insertion_sort(begin, end, limit) without the sorting:
// returns e such that [begin, e) holds exactly the moves partial_insertion_sort() would sort (begin[0] and every
// later move with value >= limit) in their original order, while every move in [e, end) ends up exactly where
// partial_insertion_sort() leaves it (the insertion there only ever writes inside the sorted prefix).
// partial_insertion_sort() is a stable descending sort of that prefix, so stable-sorting [begin, e) afterwards
// (or selecting from it the first maximum, repeatedly) reproduces its result bit for bit.
ExtMove* partial_insertion_select(ExtMove* begin, ExtMove* end, int limit) {
	if (begin == end)
		return begin;

	ExtMove* sortedEnd = begin;
#if defined(__AVX512F__)
	// [NPS 2026-10-08] The test at p only ever reads the original move (every swap writes at or before p), so the
	// tests are evaluated branch-free, 8 per compare and 64 per bit mask, and only the selected moves are visited,
	// in increasing order: the scalar loop below without its data-dependent (badly predicted) branch.
	// A move as a signed 64-bit lane is value * 2^32 + (32-bit move), so lane >= limit * 2^32 <=> value >= limit.
	// Masked loads never touch memory at or past end.
	static_assert(sizeof(ExtMove) == 8 && std::is_trivially_copyable_v<ExtMove>);
	const __m512i lim = _mm512_set1_epi64(int64_t(limit) * (int64_t(1) << 32));
	for (std::ptrdiff_t off = 1; off < end - begin; off += 64)   // an offset, so no pointer past the buffer is formed
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

// [NPS 2026-10-08] Stable descending sort of [first, last) by value: exactly the result of
// partial_insertion_sort(first, last, INT_MIN), because both are stable sorts (ties keep their order) and a stable
// sort's result is unique. That insertion sort costs O(n^2) once more than CyfMoveSorter16::MAX_ELEMENTS moves are
// sorted, so longer ranges are sorted by it in runs of 16 that are then merged pairwise (bottom-up); a merge takes the
// left (earlier) run's move on ties, so it is stable as well.
void stable_sort_moves(ExtMove* first, ExtMove* last) {
	constexpr std::ptrdiff_t Run = 16;
	const std::ptrdiff_t     n   = last - first;
	if (n <= Run)
	{
		partial_insertion_sort(first, last, std::numeric_limits<int>::min());
		return;
	}
	for (std::ptrdiff_t lo = 0; lo < n; lo += Run)   // an offset, so no pointer past the buffer is formed
		partial_insertion_sort(first + lo, first + std::min(lo + Run, n), std::numeric_limits<int>::min());

	ExtMove  buf[MAX_MOVES];   // n <= MAX_MOVES: the range lies in a move buffer
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

// [NPS-R 2026-10-08] std::move_backward(first, last, last + 1) as an inline loop instead of a memmove call:
// [first, last) moves up by one element, last first, so every element is read before it is overwritten.
inline void shift_moves_up(ExtMove* first, ExtMove* last) {
	for (ExtMove* p = last; p != first; --p)
		*p = *(p - 1);
}

// [NPS 2026-10-08] The first move with the largest value in [first, last) (first < last): the next move of a
// stable descending sort. Pass 1 finds the largest value, pass 2 its first occurrence.
// [movepicker-20261014 BX1] how many stable max selections before the rest is sorted (any value gives the same order).
#ifndef CYF_BX_PICKS
#define CYF_BX_PICKS 1
#endif
ExtMove* first_max_value(ExtMove* first, ExtMove* last) {
#if defined(__AVX512F__)
	// 8 moves per register; the values are the odd 32-bit lanes (same layout as CyfMoveSorter16::write_sorted()).
	// Masked loads never touch memory past last.
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

// [movepicker-20261014 BX1] Branch-free AVX-512 stable descending sort of up to 256 ExtMoves (value desc, ties in
// original order): a bitonic network on 64-bit keys value * 2^32 + (2^32 - 1 - index). The keys are distinct (the
// index part), so the network's result is the unique stable order, i.e. exactly what partial_insertion_sort(.., INT_MIN)
// / stable_sort_moves() produce. The moves are then gathered back by index. Padding lanes hold INT64_MIN, which is
// below every real key (the index part of a real key is >= 2^32 - 256 > 0), so they sort to the end and are not stored.
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

// returns false when n is too large (caller falls back)
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
// [movepicker-20261014 BX1] stable_sort_moves() with runs of 64 sorted by the network above (instead of runs of 16 by
// insertion), then the same stable bottom-up merge (left run wins ties). Same unique stable result.
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

} // end of namespace

// 指し手オーダリング器

// Constructors of the MovePicker class. As arguments, we pass information
// to decide which class of moves to emit, to help sorting the (presumably)
// good moves first, and how important move ordering is at the current node.

// MovePicker constructor for the main search and for the quiescence search

// MovePickerクラスのコンストラクタ。引数として、どの種類の手を生成するかを決定するための情報、
// どの手を優先的に（おそらく良い手を）ソートするか、そして現在のノードで手順の順序がどれほど重要かを渡します。

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
    // 次の指し手生成の段階
    // 王手がかかっているなら王手回避のフェーズへ。さもなくばQSEARCHのフェーズへ。

#if STOCKFISH
	if (pos.in_check())
        // 王手がかかっているなら回避手
		stage = EVASION_TT + !(ttm && pos.pseudo_legal(ttm));

    else
        // 王手がかかっていないなら通常探索用/静止探索の指し手生成
        // ⇨ 通常探索から呼び出されたのか、静止探索から呼び出されたのかについてはdepth > 0 によって判定できる。
        stage = (depth > 0 ? MAIN_TT : QSEARCH_TT)
              + !(ttm && pos.pseudo_legal(ttm));
#else
	// 🌈 やねうら王では、pos.pseudo_legal()にgenerate_all_legal_movesを渡してやる必要がある。

    if (pos.in_check())
        // 王手がかかっているなら回避手
        stage = EVASION_TT + !(ttm && pos.pseudo_legal(ttm, generate_all_legal_moves));

    else
        // 王手がかかっていないなら通常探索用/静止探索の指し手生成
        // ⇨ 通常探索から呼び出されたのか、静止探索から呼び出されたのかについてはdepth > 0 によって判定できる。
        stage = (depth > 0 ? MAIN_TT : QSEARCH_TT)
              + !(ttm && pos.pseudo_legal(ttm, generate_all_legal_moves));
#endif

	// ⇨ Stockfish 16のコード、ttm(置換表の指し手)は無条件でこのMovePickerが返す1番目の指し手としているが、これだと
    //    TTの指し手だけで千日手になってしまうことがある。これは、将棋ではわりと起こりうる。
    //    対策としては、qsearchで千日手チェックをしたり、SEEが悪いならskipするなど。
    //  ※　ここでStockfish 14のころのように置換表の指し手に条件をつけるのは良くなさげ。(V7.74l3 と V7.74mとの比較)
    //  →　ただし、その場合、qsearch()で千日手チェックが必要になる。
    //    qsearchでの千日手チェックのコストが馬鹿にならないので、
    //    ⇓このコードを有効にして、qsearch()での千日手チェックをやめた方が得。

    // recaptureの制約なくす。(ただし、やねうら王ではttmは何らかの制約を課す)
    // →　この制約入れないと、TTの指し手だけで16手超えで循環されてしまうとqsearch()で
    //    is_repetition()入れたところで永久ループになる。

    // 置換表の指し手を優遇するコード。
    // depth > -5なら、TT優先。depth <= -5でもcaptureである制約。
    // 単にcapture()にするより、この制約にしたほうが良さげ。(V775a7 vs V775a8)

    // ↓これ参考に変更したほうがよさげ？

    /*
	stage = (pos.in_check() ? EVASION_TT : QSEARCH_TT) +
		!(ttm
			&& (pos.in_check() || depth > -5 || pos.capture(ttm))
			&& pos.pseudo_legal(ttm));
*/

    // 置換表の指し手があるならそれを最初に試す。ただしpseudo_legalでなければならない。
    // 置換表の指し手がないなら、次のstageから開始する。
}

// MovePicker constructor for ProbCut: we generate captures with Static Exchange
// Evaluation (SEE) greater than or equal to the given threshold.

// 通常探索のProbCutのためのMovePickerコンストラクタ
// : 与えられた閾値以上の静的交換評価（SEE）を持つキャプチャを生成します。
// th = 枝刈りのしきい値
// ⇨ SEEの値がth以上となるcaptureの指し手(歩の成りは含む)だけを生成する。

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

    // ProbCutから呼び出されているので王手はかかっていないはず。
    ASSERT_LV3(!pos.in_check());

    // ProbCutにおいて、SEEが与えられたthresholdの値以上の指し手のみ生成する。
    // (置換表の指し手も、この条件を満たさなければならない)
    // 置換表の指し手がないなら、次のstageから開始する。

    stage = PROBCUT_TT
      + !(
        ttm
#if STOCKFISH        
        && pos.capture_stage(ttm)
        && pos.pseudo_legal(ttm) 
#else
        && pos.capture(ttm)
        // 注意 : ⇑ ProbCutの指し手生成(PROBCUT_INIT)で、
        // 歩の成りも生成するなら、ここはcapture_or_pawn_promotion()、しないならcapture()にすること。
        // ただし、TTの指し手は優遇した方が良い可能性もある。
        && pos.pseudo_legal(ttm, generate_all_legal_moves) 
#endif
	    );
    // ⇨ qsearch()のTTと同様、置換表の指し手に関してはsee_geの条件、
    // つけないほうがいい可能性があるが、やってみたら良くなかった。(V774v2 vs V774v3)
}

// Assigns a numerical value to each move in a list, used for sorting.
// Captures are ordered by Most Valuable Victim (MVV), preferring captures
// with a good history. Quiets moves are ordered using the history tables.

// 各手に数値を割り当ててリストをソートします。
// キャプチャは最も価値のある駒（MVV）に基づいて順序付けられ、
// 良好な履歴を持つキャプチャが優先されます。
// 静かな手は履歴テーブルを使用して順序付けられます。

// QUIETS、EVASIONS、CAPTURESの指し手のオーダリングのためのスコアリング。似た処理なので一本化。
template<GenType Type>
ExtMove* MovePicker::score(const MoveList<Type>& ml) {

#if STOCKFISH
	static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");
#else
    static_assert(Type == CAPTURES || Type == CAPTURES_ALL || Type == QUIETS || Type == QUIETS_ALL
                    || Type == EVASIONS || Type == EVASIONS_ALL,
                  "Wrong type");
#endif

	Color us = pos.side_to_move();

	// 自分より価値の安い駒で当たりになっているか

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
		// 🌈　Stockfishのコードを忠実に実装すると将棋ではたくさんの利きを計算しなくてはならないので
		//     非常に計算コストが高くなる。ここでは歩による当たりになっている駒だけ考える。

		// 歩による脅威だけ。
		// squares threatened by pawns
		threatenedByPawn = (~us == BLACK) ? pos.attacks_by<BLACK, PAWN>() : pos.attacks_by<WHITE, PAWN>();

		// 歩以外の自駒で、相手の歩の利きにある駒
		threatened =  (pos.pieces(us,PAWN).andnot(pos.pieces(us))                 & threatenedByPawn );
#endif
		// →　やってみたが強くならないのでコメントアウトする。[2022/04/26]

#endif
	}

    ExtMove* it = cur;
    // Cyfamate (cf. Stockfish PR #7159): the pawn-history row depends only on the position; resolve it once.
    [[maybe_unused]] decltype(&sharedHistory->pawn_entry(pos)) pawnHist = nullptr;
    if constexpr (Type == QUIETS || Type == QUIETS_ALL)
        pawnHist = &sharedHistory->pawn_entry(pos);

    // [NPS 2026-10-08] Loop invariants in locals. Through members the compiler reloaded all of them (and the
    // continuation-history pointers through this->continuationHistory) for every move, because the see_ge() and
    // scoring_bonus() calls in the loop are opaque. None of them changes while the moves are scored: pos is const
    // and see_ge() is a const query (pos.state() fixed), and rootTrace is only (re)set above.
    [[maybe_unused]] const PieceToHistory *ch0 = nullptr, *ch1 = nullptr, *ch2 = nullptr, *ch3 = nullptr, *ch5 = nullptr;
    [[maybe_unused]] std::remove_reference_t<decltype((*mainHistory)[us])>*  mainHist = nullptr;
    [[maybe_unused]] std::remove_reference_t<decltype((*lowPlyHistory)[0])>* lowPly   = nullptr;  // null: ply >= LOW_PLY_HISTORY_SIZE
    [[maybe_unused]] const StateInfo* const st      = pos.state();
    [[maybe_unused]] const Bitboard*        checkSq = nullptr;   // pos.check_squares_all() for quiet pickers
    [[maybe_unused]] const int              plyHere = (Type == QUIETS || Type == QUIETS_ALL) ? ply : 0;   // ply is not set for ProbCut pickers
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
        // [NPS 2026-10-08] checkSquares is filled on first use; the accessor fills it and returns the array.
        checkSq = pos.check_squares_all();

        // [NPS 2026-10-08] Every ply but the root (no root-hint term, no trace): the loop below with the rare
        // see_ge() call of the check bonus taken out of it, so the history loads of consecutive moves overlap and no
        // call clobbers registers, and with the lowPly divisor 1 + ply as a constant (no idiv).
        // Pass 1 stores the seven history terms (same order as below) plus, for ply < LOW_PLY_HISTORY_SIZE, the
        // lowPly term 10 * h / (1 + ply), and records the moves that land on a check square of their piece type;
        // pass 2 adds ((checkSquares[pt] & to) && see_ge(m, -63)) * 26355 to exactly those moves. The terms are the
        // ones of the loop below; only the lowPly term and the check bonus are added in the other order, which
        // cannot change an int sum here (every term is bounded by 10 * 2^15, so no partial sum overflows).
        // see_ge() is a const query of pos, so calling it after the history loads changes nothing.
        static_assert(std::remove_pointer_t<decltype(pawnHist)>::index(1, 0) == PieceToHistory::index(1, 0)
                      && std::remove_pointer_t<decltype(pawnHist)>::index(0, 1) == PieceToHistory::index(0, 1));
        const int wPawn = cyfMpWPawn, w5 = cyfMpW5;   // [MP] loaded once per picker
        auto scoreQuiets = [&](auto lowPlyDiv) -> ExtMove* {
            constexpr int Div = decltype(lowPlyDiv)::value;   // 1 + ply, or 0: no lowPly term
            const Move* const mlBegin = ml.begin();
            const int         n       = int(ml.size());
            uint16_t          checkIdx[MAX_MOVES];
            int               nCheck = 0;
#ifndef CYF_BX_PREFETCH
#define CYF_BX_PREFETCH 2   // [movepicker-20261014 BX2] default: pawn history + the five continuation histories
#endif
#if CYF_BX_PREFETCH > 0
            // [movepicker-20261014 BX2] Touch the history entries of all moves first (prefetch only, no state change):
            // the pawn-history row is selected by the pawn hash and usually cold, so the scoring loop otherwise
            // stalls on its loads with only a few moves in flight.
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

                // [NPS-R 2026-10-08] The six [pc][to] tables share one flat index (see FlatMultiArray2)
                const std::size_t k = PieceToHistory::index(pc, to);
                int v  =  1 * (*mainHist)[m.raw()];
                v     +=  (wPawn * int(pawnHist->flat(k))) >> 3;   // [MP] == 2 * h at the default 16
                v     +=      ch0->flat(k);
                v     +=      ch1->flat(k);
                v     +=      ch2->flat(k);
                v     +=      ch3->flat(k);
                v     +=  (w5 * int(ch5->flat(k))) >> 3;           // [MP] == h at the default 8
                if constexpr (Div != 0)
                    v += 10 * (*lowPly)[m.raw()] / Div;
                m.value = v;

                if (checkSq[pt] & to)   // rare (a few % of the quiets): a well predicted branch
                    checkIdx[nCheck++] = uint16_t(i);
            }
            for (int k = 0; k < nCheck; ++k)
            {
                ExtMove& m = it[checkIdx[k]];
                m.value += pos.see_ge(m, -63) * 26355;
            }
            return it + n;
        };
        static_assert(LOW_PLY_HISTORY_SIZE == 5, "one case per ply below LOW_PLY_HISTORY_SIZE");
        if (plyHere > 0)
            switch (plyHere)
            {
            case 1 : return scoreQuiets(std::integral_constant<int, 2>{});
            case 2 : return scoreQuiets(std::integral_constant<int, 3>{});
            case 3 : return scoreQuiets(std::integral_constant<int, 4>{});
            case 4 : return scoreQuiets(std::integral_constant<int, 5>{});
            default: return scoreQuiets(std::integral_constant<int, 0>{});   // lowPly == nullptr
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
			// Position::see()を用いると遅い。単に取る駒の価値順に調べたほうがパフォーマンス的にもいい。
			// 歩が成る指し手もあるのでこれはある程度優先されないといけない。
			// CAPTURE系である以上、打つ指し手は除外されている。
			// CAPTURES_PRO_PLUSで生成しているので歩の成る指し手が混じる。
			
			// MVV-LVAだが、将棋ではLVAあんまり関係なさげだが(複数の駒である1つの駒が取れるケースがチェスより少ない)、
			// Stockfish 9に倣いMVV + captureHistoryで処理する。

			// ここに来るCAPTURESに歩の成りを含めているので、捕獲する駒(pos.piece_on(to_sq(m)))がNO_PIECEで
			// ある可能性については考慮しておく必要がある。
			// → Eval::CapturePieceValuePlusPromote()を用いて計算。
			// → しかしこのあとsee_ge()の引数に使うのだが、see_ge()ではpromotionの価値を考慮してないので、
			//    ここでpromotionの価値まで足し込んでしまうとそこと整合性がとれなくなるのか…。

			
			m.value = (*captureHistory)[pc][to][type_of(capturedPiece)]
						+ int(Eval::PieceValue[capturedPiece]) * cyfMpCapPV / 64;   // [MP] 0 at the default


			// →　係数を掛けてるのは、
			// このあと、GOOD_CAPTURE で、
			//	return pos.see_ge(*cur, Value(-cur->value))
			// のようにしてあって、要するにsee_ge()の時のスケール(PieceValue)に変換するため。
			// 
			// Stockfishとは駒点が異なるので、この部分の係数を調整する必要がある。
			//
		}
#if STOCKFISH
		else if constexpr (Type == QUIETS)
#else
        else if constexpr (Type == QUIETS || Type == QUIETS_ALL)
#endif
		{
			// 駒を取らない指し手をオーダリングする。
			// ここ、歩以外の成りも含まれているのだが…。
			// →　指し手オーダリングは、quietな指し手の間での優劣を付けたいわけで、
			//    駒を成るような指し手はどうせevaluate()で大きな値がつくからそっちを先に探索することになる。

			
			// Cyfamate (cf. Stockfish PR #7115): sum in a local. The shared pawn history is atomic and an atomic load
			// may alias m.value, so the compiler otherwise stores m.value after every term.
			// [NPS 2026-10-08] The same terms, added in the same order, through the hoisted locals; m.value is
			// stored once at the end (nothing reads it in between: see_ge() and scoring_bonus() take the move only).
			int v  =  1 * (*mainHist)[m.raw()];
			v     +=  (cyfMpWPawn * int((*pawnHist)[pc][to])) >> 3;   // [MP]
			v     +=      (*ch0)[pc][to];
			v     +=      (*ch1)[pc][to];
			v     +=      (*ch2)[pc][to];
			v     +=      (*ch3)[pc][to];
			v     +=  (cyfMpW5 * int((*ch5)[pc][to])) >> 3;           // [MP]

			// bonus for checks (checkSq[pt] == pos.check_squares(pt))
			v += ((checkSq[pt] & to) && pos.see_ge(m, -63)) * 26355;


			// これ、効果があるのか検証したほうが良さげ。

#if STOCKFISH
			// penalty for moving to a square threatened by a lesser piece
			// or bonus for escaping an attack by a lesser piece.

			// 格下の駒に脅かされているマスに移動する際のペナルティ  
			// または格下の駒による攻撃から逃れる際のボーナス

			//  📓 移動元の駒が安い駒で当たりになっている場合、
			//      移動させることでそれを回避できるなら価値を上げておく。

            int v = 20 * (bool(threatByLesser[pt] & from) - bool(threatByLesser[pt] & to));
            m.value += PieceValue[pt] * v;

			// → Stockfishのコードそのままは書けない。
#endif

			// lowPlyHistoryも加算
			
			if (lowPly)   // ply < LOW_PLY_HISTORY_SIZE
                v += 10 * (*lowPly)[m.raw()] / (1 + plyHere);


			m.value = v;

			
		}
		else // Type == EVASIONS || EVASIONS_ALL
		{
			// 王手回避の指し手をスコアリングする。
			if (pos.capture_stage(m))
				m.value = PieceValue[capturedPiece] + (1 << 28);
			/* 📓 捕獲する指し手に関しては簡易SEE + MVV/LVA
				  
				  被害が小さいように、LVA(価値の低い駒)を動かして取ることを
				  優先されたほうが良いので駒に価値の低い順に番号をつける。
				  そのためのテーブル。

				  💡 LVA = Least Valuable Aggressor。cf.MVV-LVA

				ここ、moved_piece_before()を用いるのが正しい。
				そうしておかないと、同じto,fromである角成りと角成らずの2つの指し手がある時、
				moved_piece_after()だと、角成りの方は、取られた時の損失が多いとみなされてしまい、
				オーダリング上、後回しになってしまう。

				⇑これは、captureの指し手のスコアがそうでない指し手のスコアより
				常に大きくなるようにするための下駄履き。
				
				　captureの指し手の方がそうでない指し手より稀なので、この下駄履きは、captureの時にしておく。
			*/

			else
				// それ以外の指し手に関してはhistoryの値の順番
				m.value = (*mainHistory)[us][m.raw()] + (*continuationHistory[0])[pc][to];
		}

	}
    return it;
}

// Returns the next move satisfying a predicate function.
// This never returns the TT move, as it was emitted before.

// 条件を満たす次の手を返します。
// この関数は、トランスポジションテーブル（TT）の手は既に出力されているため、決して返しません。

// ※　この関数の返し値は同時にthis->moveにも格納されるので活用すると良い。filterのなかでも
//   この変数にアクセスできるので、指し手によってfilterするかどうかを選べる。

template<typename Pred>
Move MovePicker::select(Pred filter) {

	for (; cur < endCur; ++cur)
		// filter()のなかで*curにアクセスして判定するのでfilter()は引数を取らない。
		if (*cur != ttMove && filter())
			return *cur++;

	return Move::none();
}

// This is the most important method of the MovePicker class. We emit one
// new pseudo-legal move on every call until there are no more moves left,
// picking the move with the highest score from a list of generated moves.

// これはMovePickerクラスで最も重要なメソッドです。呼び出すたびに、新しい擬似合法手を1つ生成し、
// すべての手が尽きるまで続けます。生成された手のリストから、最も高いスコアを持つ手を選びます。

// ※ 置換表の指し手(ttMove)を返したあとは、それを取り除いた指し手を返す。
// skipQuiets : これがtrueだとQUIETな指し手は返さない。
Move MovePicker::next_move() {

	// 💡 good Quietの閾値
	
	constexpr int goodQuietThreshold = -14000;



top:
    switch (stage)
    {

    // 置換表の指し手を返すフェーズ
    case MAIN_TT :
    case EVASION_TT :
    case QSEARCH_TT :
    case PROBCUT_TT :
        ++stage;
        pos.fill_check_squares();   // [NPS 2026-10-09] the caller tests ttMove with gives_check_ready()
        return ttMove;

    // 置換表の指し手を返したあとのフェーズ
    case CAPTURE_INIT :
    case PROBCUT_INIT :
    case QCAPTURE_INIT : {

#if STOCKFISH
        MoveList<CAPTURES> ml(pos);

        cur = endBadCaptures = moves;

        // 駒を捕獲する指し手に対してオーダリングのためのスコアをつける
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

        // captureの指し手はそんなに数多くないので全数ソートで問題ないし、全数ソートした方が良い。
        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());

        // [NPS 2026-10-09] These moves are tested with gives_check_ready(); fill only when there are some
        //   (many qsearch nodes have no capture, and lazy filling skipped those states).
        if (cur != endCur)
            pos.fill_check_squares();

        ++stage;
        goto top;
    }

	// 置換表の指し手を返したあとのフェーズ
	// (killer moveの前のフェーズなのでkiller除去は不要)
	case GOOD_CAPTURE:
		if (select([&]() {
				// moveは駒打ちではないからsee()の内部での駒打ちは判定不要だが…。
				
				if (pos.see_ge(*cur, -(cur->value - ((cyfMpCapSplit && cyfMpCapPV)
				                                      ? int(Eval::PieceValue[pos.piece_on(cur->to_sq())]) * cyfMpCapPV / 64 : 0)) / 15))
					return true;


				std::swap(*endBadCaptures++, *cur);
				// 損をする捕獲する指し手はあとのほうで試行されるようにendBadCapturesに移動させる
				return false;
			}))
			return *(cur - 1);

		++stage;
		[[fallthrough]];

	// 駒を捕獲しない指し手を生成してオーダリング
	case QUIET_INIT:

		if (!skipQuiets)
		{
			/*
			moves          : バッファの先頭
			endBadCaptures : movesから(endBadCaptures - 1) までに bad capturesの指し手が格納されている。

				■ quiet(captures)の指し手を生成した直後。

				|--- 指し手生成用のバッファ ---------------------------------------|
				|    quiet(captures)                        |  未使用のバッファ    |
				|------------------------------------------------------------------|
				↑                                          ↑
				moves = endBadCaptures                    endMoves


				■ quiet(captures)の指し手を生成して、curポインタをインクリメントしながら読み進めているとき。

				|--- 指し手生成用のバッファ ---------------------------------------|
				|   badCaptures |  quiet(captures)           |  未使用のバッファ   |
				|------------------------------------------------------------------|
				↑             ↑              ↑            ↑
				moves      endBadCaptures      cur         endMoves


				■ quiet(captures)の指し手をpartial_sortで並び替え後にある程度スコアがいい指し手を返したとき

				残りの悪いquietの指し手の先頭をbeginBadQuietsにして、curはmovesに移動。これで、badCaptureを処理する。
				そのあと、beginBadQuietsを処理する。


				|--- 指し手生成用のバッファ ---------------------------------------|
				|   badCaptures |  quiet(captures)           |  未使用のバッファ   |
				|------------------------------------------------------------------|
				↑             ↑              ↑            ↑
				moves      endBadCaptures    beginBadQuiets endMoves
				 ↑cur

			*/

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
                // 駒を捕獲しない指し手に対してオーダリングのためのスコアをつける
                endCur = endGenerated = score<QUIETS>(ml);
			}
            // ⚠ ここ⇑、CAPTURE_INITで生成した指し手に歩の成りの指し手が
			//     含まれているなら、それを除外しなければならない。
#endif						

			/*
				📓

					指し手を部分的にソートする。depthに線形に依存する閾値で。
					(depthが低いときに真面目に全要素ソートするのは無駄だから)
			
					将棋では平均合法手は100手程度。(以前は80手程度だったが、
					AI同士の対局では終局までの平均手数が伸びたので相対的に
					終盤が多くなり、終盤は手駒を持っていることが多いから、
					そのため平均合法手が増えた。)

					また、合法手の最大は、593手。
			 
					それに対して、チェスの平均合法手は40手、合法手の最大は、218手と言われている。
			
					insertion sortの計算量は、O(n^2) で、将棋ではわりと悩ましいところ。
					sortする個数が64以上などはquick sortに切り替えるなどした方がいい可能性もある。
			*/


			
			// [NPS 2026-10-08] Was partial_insertion_sort(cur, endCur, -5551 * depth). Most pickers return only a
			// few quiets (cutoff or skipQuiets), so the O(k^2) ordering of the k selected moves is done lazily in
			// GOOD_QUIET / BAD_QUIET / pool_candidates(); every observable order is unchanged.
			lazyQuietCur = cur;
			lazyQuietEnd = partial_insertion_select(cur, endCur, -5551 * depth);
		}

		++stage;
		[[fallthrough]];

		// 駒を捕獲しない指し手を返す。
		// (置換表の指し手とkillerの指し手は返したあとなのでこれらの指し手は除外する必要がある)
		// ※　これ、指し手の数が多い場合、AVXを使って一気に削除しておいたほうが良いのでは..
	case GOOD_QUIET:
        if (!skipQuiets)
        {
            // [NPS 2026-10-08] select([&]() { return cur->value > goodQuietThreshold; }) with lazy ordering:
            // before position lazyQuietCur is examined, the move partial_insertion_sort() would have put there is
            // moved there. That is the first maximum of [lazyQuietCur, lazyQuietEnd) (a stable sort's next
            // element); the rotation keeps the rest in its original order. After a few picks the rest is ordered
            // by the same partial_insertion_sort() call on the remainder (a stable sort's suffix).
            // A pick costs O(r) for the r unordered moves, the sort O(r^2): pick while that is clearly cheaper.
            while (cur < endCur)
            {
                if (cur == lazyQuietCur && cur != lazyQuietEnd)
                {
                    if (lazyQuietPicks++ < CYF_BX_PICKS)
                    {
                        ExtMove* const best = first_max_value(cur, lazyQuietEnd);
                        const ExtMove  m    = *best;
                        shift_moves_up(cur, best);   // [NPS-R] == std::move_backward(cur, best, best + 1)
                        *cur = m;
                        ++lazyQuietCur;

                        // Every move left in [cur, lazyQuietEnd) fails the filter: skip them unordered,
                        // BAD_QUIET (or pool_candidates()) orders them before they are used.
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

		// Prepare the pointers to loop over the bad captures
		// bad capturesの指し手を返すためにポインタを準備する。
		// 📝　bad capturesの先頭を指すようにする。これは指し手生成バッファの先頭からの領域を再利用している。

		cur    = moves;
        endCur = endBadCaptures;

		++stage;
		[[fallthrough]];

		// see()が負の指し手を返す。
	case BAD_CAPTURE:
		if (select([]() { return true; }))
			return *(cur - 1);

		// Prepare the pointers to loop over the bad quiets
		// 悪いquietの手をループするためのポインタを準備します

        cur    = endCaptures;
		endCur = endGenerated;

		++stage;
		[[fallthrough]];

	case BAD_QUIET:
		if (!skipQuiets)
        {
            // [NPS-R 2026-10-08] select([&]() { return cur->value <= goodQuietThreshold; }) after
            // finish_lazy_quiets(), with the not yet ordered rest of the lazy quiet prefix ordered on demand exactly
            // as in GOOD_QUIET: before position lazyQuietCur is examined, the first maximum of
            // [lazyQuietCur, lazyQuietEnd) is moved there (the next move of the stable sort), the others keep their
            // order. A picker that is left before its bad quiets run out never sorts the rest.
            for (; cur < endCur; ++cur)
            {
                if (cur == lazyQuietCur && cur != lazyQuietEnd)
                {
                    if (lazyQuietPicks++ < CYF_BX_PICKS)
                    {
                        ExtMove* const best = first_max_value(cur, lazyQuietEnd);
                        const ExtMove  m    = *best;
                        shift_moves_up(cur, best);   // [NPS-R] == std::move_backward(cur, best, best + 1)
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

	// 王手回避手の生成
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
			// 王手を回避する指し手に対してオーダリングのためのスコアをつける
            endCur = endGenerated = score<EVASIONS>(ml);
		}
#endif

        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());

        pos.fill_check_squares();   // [NPS 2026-10-09] the evasions are tested with gives_check_ready()

        ++stage;
        [[fallthrough]];
    }
		// 王手回避の指し手を返す
	case EVASION:
		// 静止探索用の指し手を返す処理
	case QCAPTURE:
		// そんなに数は多くないはずだから、オーダリングがベストのスコアのものを選択する
		return select([]() { return true; });

		// PROBCUTの指し手を返す
	case PROBCUT:
		return select([&]() { return pos.see_ge(*cur, threshold); });
		// threadshold以上のSEE値で、ベストのものを一つずつ返す

	default:
		UNREACHABLE;
		return Move::none();
	}

	ASSERT(false);
	return Move::none(); // Silence warning
}

// [NPS-R 2026-10-08] In qsearch almost every next_move() call is in stage QCAPTURE or EVASION, which only run
// select([]() { return true; }). That loop is done here, inline in qsearch, so these calls skip the prologue and
// epilogue of the large stage switch. Every other stage, and the first call that generates and sorts the moves,
// goes to next_move() as before: the same moves in the same order. (Inlining this test into the main search's
// next_move() calls as well cost more instructions than it saved, so search() keeps calling next_move().)
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

// [NPS 2026-10-08] Orders the rest of the lazy quiet prefix. [lazyQuietCur, lazyQuietEnd) holds the moves that are
// still unordered, in their original relative order, and every earlier position already holds its final move, so
// this yields exactly the layout of the original partial_insertion_sort(.., -5551 * depth).
void MovePicker::finish_lazy_quiets() {
    if (lazyQuietCur != lazyQuietEnd)
    {
#if defined(__AVX512F__) && (!defined(CYF_BX_VSORT) || CYF_BX_VSORT)
        cyf_stable_sort_moves_v(lazyQuietCur, lazyQuietEnd);   // [BX1] same unique stable order, AVX-512 network
#else
        stable_sort_moves(lazyQuietCur, lazyQuietEnd);
#endif
        // partial_insertion_sort(.., INT_MIN), in O(r log r)
        lazyQuietCur = lazyQuietEnd;
    }
}

} // namespace YaneuraOu

#endif // defined(USE_MOVE_PICKER)
