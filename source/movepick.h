#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include "config.h"
#if defined(USE_MOVE_PICKER)

#include <array>
#include <cmath>
#include <limits>

#include "history.h"
#include "movegen.h"
#include "types.h"
#include "position.h"

namespace YaneuraOu {




class MovePicker {
   public:
    MovePicker(const MovePicker&)            = delete;
    MovePicker& operator=(const MovePicker&) = delete;

    MovePicker(const Position&              pos_,
               Move                         ttMove_,
               Depth                        depth_,
               const ButterflyHistory*      mh,
               const LowPlyHistory*         lph,
               const CapturePieceToHistory* cph,
               const PieceToHistory**       ch,
               const SharedHistories*       sh,
               int ply_
#if !STOCKFISH
              ,bool generate_all_legal_moves
#endif
    );

    MovePicker(const Position&, Move ttMove_, int threshold_, const CapturePieceToHistory*
#if STOCKFISH
#else
    , bool generate_all_legal_moves
#endif
    );

    Move next_move();

    inline Move next_move_qsearch();

    int cyf_stage() const { return stage; }

    template<class It>
    void trace_root_accept(Move m, It first, It last) {
        if (!CyfRootHint::tracing()) return;
        auto& g = CyfRootHint::state().go;
        if (CyfRootHint::bonus(m.raw())) {
            if (m == ttMove) ++g.accepted_hint_tt;
            else if (pos.capture_stage(m)) ++g.accepted_hint_capture;
            else if (pos.in_check()) ++g.accepted_hint_evasion;
            else ++g.accepted_hint_quiet;
        }
        if (!rootTrace || m == ttMove || (!rootTrace->evasion && pos.capture_stage(m))) return;
        if (!rootTrace->prepared) {
            for (const auto& b : rootTrace->baseline)
                if (b != ttMove && std::any_of(first, last, [&](const auto& rm) { return rm.pv[0].raw() == b.raw(); }))
                    rootTrace->expected.push_back(b.raw());
            rootTrace->prepared = true;
        }
        const size_t i = rootTrace->seen++;
        ++g.trace_compared;
        if (i >= rootTrace->expected.size() || m.raw() != rootTrace->expected[i]) {
            ++g.trace_changed;
            if (!rootTrace->changed) { rootTrace->changed = true; ++g.trace_changed_frames; }
        }
    }

#if defined(CYF_GPU_POOL)
    std::pair<const ExtMove*, const ExtMove*> pool_candidates() const;
#endif

    void skip_quiet_moves();

   private:
    template<typename Pred>
    Move select(Pred);

    template<GenType T>
    ExtMove* score(const MoveList<T>&);

    ExtMove* begin() { return cur; }
    ExtMove* end() { return endCur; }

    const Position& pos;

    const ButterflyHistory*      mainHistory;
    const LowPlyHistory*         lowPlyHistory;
    const CapturePieceToHistory* captureHistory;
    const PieceToHistory**       continuationHistory;
	const SharedHistories*       sharedHistory;

    Move ttMove;

    ExtMove *cur, *endCur, *endBadCaptures, *endCaptures, *endGenerated;

    int stage;

    int threshold;

    Depth depth;

    int ply;

    bool skipQuiets = false;

    ExtMove *lazyQuietCur = nullptr, *lazyQuietEnd = nullptr;
    int      lazyQuietPicks = 0;
    void     finish_lazy_quiets();

    struct RootHintTrace {
        bool evasion = false, prepared = false, changed = false;
        size_t seen = 0;
        std::vector<ExtMove> baseline;
        std::vector<uint16_t> expected;
    };
    std::unique_ptr<RootHintTrace> rootTrace;

    ExtMove moves[MAX_MOVES];

#if STOCKFISH
#else
    bool generate_all_legal_moves;
#endif
};

}  

#endif 
#endif 

