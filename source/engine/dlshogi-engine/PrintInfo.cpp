#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)

#include "PrintInfo.h"
#include "FukauraOuEngine.h"
#include "Node.h"
#include "UctSearch.h"

#include "../../usi.h"
#include "../../engine.h"

#include <iomanip>		
#include <sstream>		

namespace dlshogi::UctPrint {

using namespace YaneuraOu::Search;






bool is_superior_to(const ChildNode* lhs, const ChildNode* rhs) {
    
    

    
    auto win_type_of = [](const ChildNode* c) { return c->IsLose() ? 2 : c->IsWin() ? 0 : 1; };

    int t1 = win_type_of(lhs);
    int t2 = win_type_of(rhs);

    
    
    

    
    
    const NodeCountType lm = lhs->move_count + 1;
    const NodeCountType rm = rhs->move_count + 1;

    return t1 != t2 ? t1 > t2 :  
             lm != rm ? lm > rm
                      :                  
             lhs->nnrate > rhs->nnrate;  
}


struct BestMove {
    
    Move move;

    
    WinType wp;

    
    Node* node;

    
    NodeCountType move_count;

    BestMove() :
        move(Move::none()),
        wp(0),
        node(nullptr),
        move_count(0) {}
    BestMove(Move move_, WinType wp_, Node* node_, NodeCountType move_count) :
        move(move_),
        wp(wp_),
        node(node_),
        move_count(move_count) {}
};

BestMovePonder::BestMovePonder() :
    move(Move::none()),
    wp(0),
    ponder(Move::none()) {}





int select_best_child(Node* node) {
    int child_num = node->child_num;
    if (child_num == 0)
        return -1;

    
    ChildNode* child      = node->child.get();
    int        best_child = 0;

    for (int i = 1; i < child_num; ++i)
        if (is_superior_to(&child[i], &child[best_child]))
            best_child = i;

    return best_child;
}



std::vector<BestMove> select_best_moves(const Node* node, ChildNumType multiPv) {
    std::vector<BestMove> bests;

    
    if (node->child_num == 0)
        return bests;

    

    const ChildNode* child = node->child.get();

    
    std::vector<std::pair<ChildNumType, const ChildNode*>> list;
    list.reserve(node->child_num);
    for (ChildNumType i = 0; i < node->child_num; ++i)
        list.emplace_back(i, &child[i]);

    
    multiPv = std::min(multiPv, (ChildNumType) list.size());
    std::partial_sort(list.begin(), list.begin() + multiPv, list.end(),
                      [](auto& lhs, auto& rhs) { return is_superior_to(lhs.second, rhs.second); });

    
    for (ChildNumType i = 0; i < multiPv; ++i)
    {
        ChildNumType index     = list[i].first;
        const auto&  child     = list[i].second;
        auto         next_node = node->child_nodes ? node->child_nodes[index].get() : nullptr;

        
        float wp = child->move_count ? (float) (child->win / child->move_count)
                                     :  0.5f;
        bests.push_back(BestMove(child->move, wp, next_node, child->move_count));
    }
    return bests;
}


void get_pv(Node* node, std::vector<Move>& moves) {
    while (node)
    {
        
        if ((NodeCountType) (node->move_count + 1) <= 1)
            break;

        auto best_child = select_best_child(node);
        if (best_child == -1)
            break;

        moves.push_back(node->child[best_child].move);
        if (!node->child)
            break;

        node = node->child_nodes ? node->child_nodes[best_child].get() : nullptr;
    }
}


BestMovePonder get_best_move_multipv(const Node*          rootNode,
                                     const SearchLimits&  po_info,
                                     const SearchOptions& options,
                                     UpdateContext&       updates) {
    ChildNumType multiPv = options.multi_pv;

    
    auto finish_time = std::max((TimePoint) 1, po_info.time_manager.elapsed_time());

    InfoFull info;
    info.nps      = (po_info.nodes_searched * 1000LL / (u64) finish_time);
    info.timeMs   = finish_time;
    info.hashfull = po_info.current_root->move_count * 1000LL / options.uct_node_limit;
    info.nodes    = po_info.nodes_searched;

#if 0
	if (rootNode->mate_ply > 0)
	{
		
		const ChildNode* uct_child = rootNode->child.get();
		Move move = Move::none();
		int ply = rootNode->mate_ply;
		
		if (ply == 0)
			ply = INT_MAX;

		for (size_t i = 0; i < rootNode->child_num; ++i)
			if (uct_child[i].IsLose())
			{
				
				
				if (rootNode->child_nodes.get() && rootNode->child_nodes[i])
				{
					int mated_ply = rootNode->child_nodes[i]->mate_ply;
					if (mated_ply)
					{
						int mate_ply = 1 - mated_ply; 
						if (mate_ply < ply)
						{
							ply = mate_ply;
							move = uct_child[i].getMove();
						}
					}
				}

				
				
				if (!move)
					move = uct_child[i].getMove();
			}

		
		nps << " pv " << to_usi_string(move);
		if (!silent)
			sync_cout << "info score mate " << ply << nps.str() << sync_endl;
			
		return BestMovePonder(move, 1.0, Move::none());
	}
#endif

    

    auto bests = select_best_moves(rootNode, multiPv);
    if (bests.size() == 0)
        return BestMovePonder();

    Move ponder = Move::none();
    for (ChildNumType i = 0; i < (ChildNumType) bests.size(); ++i)
    {
        auto best = bests[i];

        std::vector<Move> moves = {best.move};
        get_pv(best.node, moves);

		
		int cp = Eval::dlshogi::value_to_cp((float) best.wp, options.eval_coef);

		info.multiPV = (i + 1);

		
		if (info.multiPV > 1)
			info.nodes   = best.move_count;

		info.depth = int(moves.size());
		info.score = Value(cp);

		
        info.selDepth = info.depth;

		

		
		std::string pv;
		if (moves.size())
		{
			for (auto m : moves)
				pv += to_usi_string(m) + ' ';
			if (!pv.empty())
				pv.pop_back();

			info.pv = std::move(pv);
		}

		
		updates.onUpdateFull(info);

        if (i == 0 && moves.size() >= 2)
            ponder = moves[1];
    }

    return BestMovePonder(bests[0].move, bests[0].wp, ponder);
}




void PrintPlayoutInformation(const Node*         root,
                             const SearchLimits* po_info,
                             const TimePoint     finish_time,
                             const NodeCountType pre_simulated) {
    double finish_time_sec = finish_time / 1000.0;

    
    auto root_move_count = root->move_count == NOT_EXPANDED ? 0 : root->move_count.load();
    sync_cout << "All Playouts       :  " << std::setw(7) << root_move_count << sync_endl;

    
    sync_cout << "Pre Simulated      :  " << std::setw(7) << pre_simulated << sync_endl;

    
    sync_cout << "Thinking Time      :  " << std::setw(7) << finish_time_sec << " sec" << sync_endl;

    
    
    
    if (root_move_count)
    {
        double winning_percentage = (double) root->win / root->move_count;
        sync_cout << "Winning Percentage :  " << std::setw(7) << (winning_percentage * 100) << "%"
                  << sync_endl;
    }

    
    if (finish_time_sec != 0.0)
        sync_cout << "Playout Speed      :  " << std::setw(7)
                  << (int) (po_info->nodes_searched / finish_time_sec) << " PO/sec " << sync_endl;
}


void PrintPlayoutLimits(const TimeManagement& time_manager, const int playout_limit) {
    sync_cout << "Minimum Time  : " << time_manager.minimum() << "[ms]" << sync_endl;
    sync_cout << "Optimum Time  : " << time_manager.optimum() << "[ms]" << sync_endl;
    sync_cout << "Maximum Time  : " << time_manager.maximum() << "[ms]" << sync_endl;
    sync_cout << "Playout Limit : " << playout_limit << " PO" << sync_endl;
}


void PrintReuseCount(const int count) {
    sync_cout << "Reuse : " << count << " Playouts" << sync_endl;
}

}  


#endif 
