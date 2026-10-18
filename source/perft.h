#ifndef PERFT_H_INCLUDED
#define PERFT_H_INCLUDED


#include "movegen.h"
#include "position.h"
#include "types.h"
#include "usi.h"
#include "misc.h"

namespace YaneuraOu::Benchmark {




template<bool Root>
uint64_t perft(Position& pos, Depth depth) {

    StateInfo st;

    uint64_t   cnt, nodes = 0;
    const bool leaf = (depth == 2);

    for (const auto& m : MoveList<LEGAL_ALL>(pos))
    {
        if (Root && depth <= 1)
            cnt = 1, nodes++;
        else
        {
            pos.do_move(m, st);
            cnt = leaf ? MoveList<LEGAL_ALL>(pos).size() : perft<false>(pos, depth - 1);
            nodes += cnt;
            pos.undo_move(m);
        }
        if (Root)
#if STOCKFISH
            sync_cout << USIEngine::move(m, pos.is_chess960() ) << ": " << cnt << sync_endl;
#else
            sync_cout << USIEngine::move(m ) << ": " << cnt << sync_endl;
#endif
    }
    return nodes;
}

#if STOCKFISH
inline uint64_t perft(const std::string& fen, Depth depth , bool isChess960) {
#else
inline uint64_t perft(const std::string& fen, Depth depth ) {

	ElapsedTimer time;
    time.reset();
#endif

	Position  p;
    StateInfo st;

#if STOCKFISH
    p.set(fen, isChess960, &st);
    return perft<true>(p, depth);
#else
    p.set(fen, &st);

	auto nodes = perft<true>(p, depth);
    auto elapsed = time.elapsed() + 1; 

	sync_cout << "Elapsed Time = " << elapsed << " [ms]" << sync_endl;
    sync_cout << 1000 * nodes / elapsed << " NPS" << sync_endl;

	return nodes;
#endif

}
}

#endif  
