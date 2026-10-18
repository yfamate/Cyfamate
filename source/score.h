#ifndef SCORE_H_INCLUDED
#define SCORE_H_INCLUDED

#include <variant>
#include <utility>

#include "types.h"

namespace YaneuraOu {


class Score {
   public:
    struct Mate {
        int plies;
    };


    struct InternalUnits {
        int value;
    };

    Score() = default;

    Score(Value v );

#if !STOCKFISH
    static Score from_internal_value(Value v);
    Value        to_value() const;
#endif

    template<typename T>
    bool is() const {
        return std::holds_alternative<T>(score);
    }

	template<typename T>
    T get() const {
        return std::get<T>(score);
    }

    template<typename F>
    decltype(auto) visit(F&& f) const {
        return std::visit(std::forward<F>(f), score);
    }

   private:
    std::variant<Mate,  InternalUnits> score;
};

}

#endif  
