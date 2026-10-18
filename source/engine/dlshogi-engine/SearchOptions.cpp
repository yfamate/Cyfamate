#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "SearchOptions.h"

#include "../../position.h"
#include "../../usi.h"
#include "../../thread.h"
#include "../../movegen.h" 

#include "../../eval/deep/nn.h"
#include "../../eval/deep/nn_types.h"

namespace dlshogi {

using namespace YaneuraOu;

void SearchOptions::add_options(OptionsMap& options) {

    options.add(  
      "MultiPV", Option(1, 1, MAX_MOVES, [&](const Option& o) {
          multi_pv = ChildNumType(o);
          return std::nullopt;
      }));

    options.add(  
      "PV_Interval", Option(500, 0, int_max, [&](const Option& o) {
          pv_interval = TimePoint(o);
          return std::nullopt;
      }));

    options.add(  
      "MaxMovesToDraw", Option(0, 0, 100000, [&](const Option& o) {
          max_moves_to_draw = int(o);
          if (max_moves_to_draw == 0)
              max_moves_to_draw = 100000;
          return std::nullopt;
      }));

    options.add(  
      "ReuseSubtree", Option(true, [&](const Option& o) {
          reuse_subtree = o;
          return std::nullopt;
      }));


    options.add(  
      "Eval_Coef", Option(285, 1, 10000, [&](const Option& o) {
          eval_coef = float(o);
          return std::nullopt;
      }));

    options.add(  
      "Resign_Threshold", Option(0, 0, 1000, [&](const Option& o) {
          RESIGN_THRESHOLD = int(options["Resign_Threshold"]) / 1000.0f;
          return std::nullopt;
      }));

    options.add(  
      "DebugMessage", Option(false, [&](const Option& o) {
          debug_message = o;
          return std::nullopt;
      }));


    options.add(  
      "UCT_NodeLimit", Option(10000000, 10, 1000000000, [&](const Option& o) {
          uct_node_limit = NodeCountType(o);
          return std::nullopt;
      }));


    options.add(  
      "DrawValueBlack", Option(500, 0, 1000, [&](const Option& o) {
          draw_value_black = int(o) / 1000.0f;
          return std::nullopt;
      }));
    options.add(  
      "DrawValueWhite", Option(500, 0, 1000, [&](const Option& o) {
          draw_value_white = int(o) / 1000.0f;
          return std::nullopt;
      }));



    options.add(  
      "C_fpu_reduction", Option(27, 0, 100, [&](const Option& o) {
          c_fpu_reduction = o / 100.0f;
          return std::nullopt;
      }));
    options.add(  
      "C_fpu_reduction_root", Option(0, 0, 100, [&](const Option& o) {
          c_fpu_reduction_root = o / 100.0f;
          return std::nullopt;
      }));

    options.add(  
      "C_init", Option(144, 0, 500, [&](const Option& o) {
          c_init = o / 100.0f;
          return std::nullopt;
      }));
    options.add(  
      "C_base", Option(28288, 10000, 100000, [&](const Option& o) {
          c_base = NodeCountType(o);
          return std::nullopt;
      }));
    options.add(  
      "C_init_root", Option(116, 0, 500, [&](const Option& o) {
          c_init_root = o / 100.0f;
          return std::nullopt;
      }));
    options.add(  
      "C_base_root", Option(25617, 10000, 100000, [&](const Option& o) {
          c_base_root = NodeCountType(o);
          return std::nullopt;
      }));


    options.add(  
      "Softmax_Temperature", Option(174, 1, 10000, [&](const Option& o) {
          Eval::dlshogi::set_softmax_temperature(o / 100.0f);
          return std::nullopt;
      }));

#if DLSHOGI

    dfpn_min_search_millisecs = options["DfPn_Min_Search_Millisecs"];
#endif

    options.add(  
      "LeafDfpnNodesLimit", Option(40, 0, 10000, [&](const Option& o) {
          leaf_dfpn_nodes_limit = NodeCountType(o);
          return std::nullopt;
      }));

    options.add("PV_Mate_Search_Threads", Option(1, 0, 256));
    options.add("PV_Mate_Search_Nodes", Option(500000, 0, UINT32_MAX));

    options.add(  
      "GenerateAllLegalMoves", Option(false, [&](const Option& o) {
          generate_all_legal_moves = o;
          return std::nullopt;
      }));

    options.add(  
      "EnteringKingRule", Option(EKR_STRINGS, EKR_STRINGS[EKR_27_POINT], [&](const Option& o) {
          enteringKingRule = to_entering_king_rule(o);
          return std::nullopt;
      }));
}

} 

#endif 
