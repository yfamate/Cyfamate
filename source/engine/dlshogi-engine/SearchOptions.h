#ifndef __SEARCH_OPTIONS_H_INCLUDED__
#define __SEARCH_OPTIONS_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "dlshogi_types.h"

namespace dlshogi {

using namespace YaneuraOu;

struct SearchOptions {

    void add_options(OptionsMap& options);



    float c_fpu_reduction      = 0.27f;
    float c_fpu_reduction_root = 0.00f;


    float         c_init      = 1.44f;
    NodeCountType c_base      = 28288;
    float         c_init_root = 1.16f;
    NodeCountType c_base_root = 25617;



    float draw_value_black = 0.5f;  
    float draw_value_white = 0.5f;  

    float RESIGN_THRESHOLD = 0.0f;

    bool reuse_subtree = true;

    TimePoint pv_interval = 500;

    float eval_coef = 285;

    int max_moves_to_draw = 100000;


    NodeCountType uct_node_limit = 10000000;

    ChildNumType multi_pv = 1;

    bool debug_message = false;

    bool generate_all_legal_moves = false;

	EnteringKingRule enteringKingRule = EKR_27_POINT;

    int leaf_dfpn_nodes_limit = 40;
};

} 

#endif 

#endif 
