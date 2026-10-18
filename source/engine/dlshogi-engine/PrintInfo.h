#ifndef __PRINT_INFO_H_INCLUDED__
#define __PRINT_INFO_H_INCLUDED__

#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)
#include "../../position.h"
#include "Node.h"
#include "UctSearch.h"

namespace dlshogi
{
	struct SearchLimits;
	struct SearchOptions;
}

namespace dlshogi::UctPrint {

void PrintPlayoutInformation(const Node*         root,
                             const SearchLimits* po_info,
                             const TimePoint     finish_time,
                             const NodeCountType pre_simulated);

void PrintPlayoutLimits(const TimeManagement& time_manager, const int playout_limit);

void PrintReuseCount(const int count);


struct BestMovePonder {
    Move move;

    Move ponder;

    WinType wp;

    BestMovePonder();
    BestMovePonder(Move move_, WinType wp_, Move ponder_) :
        move(move_),
        wp(wp_),
        ponder(ponder_) {}
};

BestMovePonder get_best_move_multipv(const Node*                       rootNode,
                                     const SearchLimits&               po_info,
                                     const SearchOptions&              options,
                                     YaneuraOu::Search::UpdateContext& context);

}  

#endif 

#endif 
