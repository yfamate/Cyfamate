#include "score.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

#include "usi.h"

namespace YaneuraOu {

Score::Score(Value v ) {
    assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

    if (!is_decisive(v))
    {
        score = InternalUnits{USIEngine::to_cp(v )};
    }
    else
    {
        auto distance = VALUE_MATE - std::abs(v);
        score         = (v > 0) ? Mate{distance} : Mate{-distance};
    }
}

#if !STOCKFISH
Score Score::from_internal_value(Value v) {
    Score score;
    score.score = InternalUnits{ v };
    return score;
}

Value Score::to_value() const {
    if (is<InternalUnits>())
        return USIEngine::cp_to_value(get<InternalUnits>().value);

    int plies = get<Mate>().plies;
    return (plies > 0) ? mate_in(plies) : mated_in(plies);
}
#endif

}
