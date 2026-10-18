#ifndef HISTORY_H_INCLUDED
#define HISTORY_H_INCLUDED

#include <array>
#include <atomic>

#include <cmath>


#include <limits>
#include <type_traits>  



#include "memory.h"
#include "misc.h"
#include "position.h"

namespace YaneuraOu {






constexpr int PAWN_HISTORY_BASE_SIZE   = 8192;  
constexpr int UINT_16_HISTORY_SIZE     = std::numeric_limits<uint16_t>::max() + 1;

constexpr int CORRHIST_BASE_SIZE       = UINT_16_HISTORY_SIZE;
constexpr int CORRECTION_HISTORY_LIMIT = 1024;
constexpr int LOW_PLY_HISTORY_SIZE     = 5;

static_assert((PAWN_HISTORY_BASE_SIZE & (PAWN_HISTORY_BASE_SIZE - 1)) == 0,
              "PAWN_HISTORY_BASE_SIZE has to be a power of 2");

static_assert((CORRHIST_BASE_SIZE & (CORRHIST_BASE_SIZE - 1)) == 0,
              "CORRHIST_BASE_SIZE has to be a power of 2");



namespace CyfHandCorr { constexpr int w = 8000; constexpr int u = 128; }
namespace CyfMajorCorr { constexpr int w = 6000; constexpr int u = 128; }   
inline uint16_t pawn_correction_history_index(const Position& pos) { return uint16_t(pos.pawn_key()); }



inline uint16_t minor_piece_index(const Position& pos) { return uint16_t(pos.minor_piece_key()); }

template<Color c>
inline uint16_t non_pawn_index(const Position& pos) {
    return uint16_t(pos.non_pawn_key(c));
}















template<typename T, int D, bool Atomic = false>
struct StatsEntry {

	static_assert(std::is_arithmetic_v<T>, "Not an arithmetic type");

   private:
	
    std::conditional_t<Atomic, std::atomic<T>, T> entry;


   public:
    void operator=(const T& v) {
        if constexpr (Atomic)
            entry.store(v, std::memory_order_relaxed);
        else
            entry = v;
    }

    operator T() const {
        if constexpr (Atomic)
            return entry.load(std::memory_order_relaxed);
        else
            return entry;
    }

	
	
	void operator<<(int bonus) {

		
		

		int clampedBonus = std::clamp(bonus, -D, D);

        T   val          = *this;
        *this            = val + clampedBonus - val * std::abs(clampedBonus) / D;

        assert(std::abs(T(*this)) <= D);

		
		

		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		

		ASSERT_LV3(std::abs(entry) <= D);
	}
};

enum StatsType {
	NoCaptures,
	Captures
};

template<typename T, int D, std::size_t... Sizes>
using Stats = MultiArray<StatsEntry<T, D>, Sizes...>;

template<typename T, int D, std::size_t... Sizes>
using AtomicStats = MultiArray<StatsEntry<T, D, true>, Sizes...>;










template<typename T, int SizeMultiplier>
struct DynStats {
	
    explicit DynStats(size_t s) {
		
        size = s * SizeMultiplier;

		
        data = make_unique_large_page<T[]>(size);
    }

    
	
	
    void clear_range(int value, size_t threadIdx, size_t numaTotal) {
        size_t start = uint64_t(threadIdx) * size / numaTotal;
        assert(start < size);

		
        size_t end = threadIdx + 1 == numaTotal ? size : uint64_t(threadIdx + 1) * size / numaTotal;

        while (start < end)
            data[start++].fill(value);
    }

	
	size_t get_size() const { return size; }

	
	T&     operator[](size_t index) {
        assert(index < size);
        return data.get()[index];
    }
    const T& operator[](size_t index) const {
        assert(index < size);
        return data.get()[index];
    }

   private:
	
    size_t            size;

	
    LargePagePtr<T[]> data;
};













using ButterflyHistory = Stats<std::int16_t, 7183, COLOR_NB, UINT_16_HISTORY_SIZE>;










using LowPlyHistory = Stats<std::int16_t, 7183, LOW_PLY_HISTORY_SIZE, UINT_16_HISTORY_SIZE>;




using CapturePieceToHistory = Stats<std::int16_t, 10692, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>;





using PieceToHistory = FlatMultiArray2<StatsEntry<std::int16_t, 30000>, PIECE_NB, SQUARE_NB>;









using ContinuationHistory = MultiArray<PieceToHistory, PIECE_NB, SQUARE_NB>;





using PawnHistory =
  DynStats<FlatMultiArray2<StatsEntry<std::int16_t, 8192, true>, PIECE_NB, SQUARE_NB>, PAWN_HISTORY_BASE_SIZE>;   









enum CorrHistType {
    Pawn,          
    Minor,         
    NonPawn,       
    PieceTo,       
    Continuation,  
};




template<typename T, int D>
struct CorrectionBundle {
    StatsEntry<T, D, true> pawn;
    StatsEntry<T, D, true> minor;
    StatsEntry<T, D, true> nonPawnWhite;
    StatsEntry<T, D, true> nonPawnBlack;
    StatsEntry<T, D, true> hand;   
    StatsEntry<T, D, true> major;   

	
    void operator=(T val) {
        pawn         = val;
        minor        = val;
        nonPawnWhite = val;
        nonPawnBlack = val;
        hand         = val;
        major        = val;   
    }
};

namespace Detail {

template<CorrHistType>
struct CorrHistTypedef {
    using type =
      DynStats<Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, COLOR_NB>, CORRHIST_BASE_SIZE>;
};

template<>
struct CorrHistTypedef<PieceTo> {
    using type = Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, PIECE_NB, SQUARE_NB>;
};

template<>
struct CorrHistTypedef<Continuation> {
    using type = MultiArray<CorrHistTypedef<PieceTo>::type, PIECE_NB, SQUARE_NB>;
};

template<>
struct CorrHistTypedef<NonPawn> {
    using type = DynStats<Stats<std::int16_t, CORRECTION_HISTORY_LIMIT, COLOR_NB, COLOR_NB>,
                          CORRHIST_BASE_SIZE>;
};

}

using UnifiedCorrectionHistory =
  DynStats<MultiArray<CorrectionBundle<std::int16_t, CORRECTION_HISTORY_LIMIT>, COLOR_NB>,
           CORRHIST_BASE_SIZE>;


template<CorrHistType T>
using CorrectionHistory = typename Detail::CorrHistTypedef<T>::type;

using TTMoveHistory = StatsEntry<std::int16_t, 8192>;













struct SharedHistories {
    SharedHistories(size_t threadCount) :
        correctionHistory(threadCount),
        pawnHistory(threadCount) {
        assert((threadCount & (threadCount - 1)) == 0 && threadCount != 0);
        sizeMinus1         = correctionHistory.get_size() - 1;
        pawnHistSizeMinus1 = pawnHistory.get_size() - 1;
    }

    size_t get_size() const { return sizeMinus1 + 1; }

    static std::size_t cyf_ph_key(const Position& pos) {   
        if (cyfPhKMode != 2) return std::size_t(pos.pawn_key());
        const std::size_t kp = std::size_t(pos.square<KING>(BLACK)) * 81 + std::size_t(pos.square<KING>(WHITE));
        return std::size_t(pos.pawn_key()) ^ std::size_t((kp * 0x9E3779B97F4A7C15ull) >> 20);
    }
    auto& pawn_entry(const Position& pos) {
        return pawnHistory[cyf_ph_key(pos) & pawnHistSizeMinus1];
    }
    const auto& pawn_entry(const Position& pos) const {
        return pawnHistory[cyf_ph_key(pos) & pawnHistSizeMinus1];
    }

    auto& pawn_correction_entry(const Position& pos) {
        return correctionHistory[pos.pawn_key() & sizeMinus1];
    }
    const auto& pawn_correction_entry(const Position& pos) const {
        return correctionHistory[pos.pawn_key() & sizeMinus1];
    }

    auto& minor_piece_correction_entry(const Position& pos) {
        return correctionHistory[pos.minor_piece_key() & sizeMinus1];
    }
    const auto& minor_piece_correction_entry(const Position& pos) const {
        return correctionHistory[pos.minor_piece_key() & sizeMinus1];
    }

    template<Color c>
    auto& nonpawn_correction_entry(const Position& pos) {
        return correctionHistory[pos.non_pawn_key(c) & sizeMinus1];
    }
    template<Color c>
    const auto& nonpawn_correction_entry(const Position& pos) const {
        return correctionHistory[pos.non_pawn_key(c) & sizeMinus1];
    }

    auto& major_correction_entry(const Position& pos) {   
        return correctionHistory[pos.state()->majorPieceKey & sizeMinus1];
    }
    auto& hand_correction_entry(const Position& pos) {   
        return correctionHistory[pos.state()->hand_key & sizeMinus1];
    }
    const auto& major_correction_entry(const Position& pos) const {
        return correctionHistory[pos.state()->majorPieceKey & sizeMinus1];
    }
    const auto& hand_correction_entry(const Position& pos) const {
        return correctionHistory[pos.state()->hand_key & sizeMinus1];
    }

    UnifiedCorrectionHistory correctionHistory;
    PawnHistory              pawnHistory;

   private:
    size_t sizeMinus1, pawnHistSizeMinus1;
};


} 

#endif 

