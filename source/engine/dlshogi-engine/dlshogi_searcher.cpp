#include "dlshogi_searcher.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include <sstream> 
#include <cstring> 
#include <numeric> 

#include "dlshogi_types.h"
#include "UctSearch.h"
#include "PrintInfo.h"
#include "FukauraOuEngine.h"

#include "../../search.h"
#include "../../thread.h"
#include "../../mate/mate.h"
#include "../../engine.h"

namespace dlshogi {





DlshogiSearcher::DlshogiSearcher(FukauraOuEngine& engine) :
	engine(engine)
{
	gc                   = std::make_unique<NodeGarbageCollector>();
    interruption_checker = std::make_unique<SearchInterruptionChecker>(this);
}

void DlshogiSearcher::add_options(OptionsMap& options) {
	search_options.add_options(options);
    search_limits.time_manager.add_options(options);
}





void DlshogiSearcher::InitGPU(const std::string& model_path, const std::string& model_architecture,
                              std::vector<int> thread_settings, int policy_value_batch_maxsize)
{
	
	
	

	
    size_t total_thread_num = std::accumulate(thread_settings.begin(), thread_settings.end(), 0);

	
	if (total_thread_num == 0)
	{
		sync_cout << "info string Error! : total threads = 0 " << sync_endl;
		return;
	}

	
	
	

	 

	auto worker_factory = [&](Search::SharedState& sharedState,
								const Search::ThreadIds& ids)
	{
            auto p = make_unique_large_page<FukauraOuWorker>(

              
              sharedState, ids,

              
              *this, engine);

			return LargePagePtr<Worker>(p.release());  
        };

	
	const int search_interruption_check_thread_num = 1;

    engine.threads.set(engine.numaContext.get_numa_config(), {engine.options, engine.threads, engine.tt, engine.sharedHists  },
					engine.updateContext, total_thread_num + search_interruption_check_thread_num , worker_factory);

	

	if (thread_settings != last_thread_settings)
    {
        search_groups        = std::make_unique<UctSearcherGroup[]>(thread_settings.size());
        search_groups_size   = thread_settings.size();
        last_thread_settings = thread_settings;
    }

	
    ElapsedTimer time;
        
	for (size_t i = 0; i < search_groups_size ; i++)
		if (thread_settings[i] > 0)
			search_groups[i].Initialize(model_path, model_architecture,
			                            thread_settings[i],int(i), policy_value_batch_maxsize);

	sync_cout << "info string All model files have been loaded. " << time.elapsed() << "ms." << sync_endl;

	
	
	

	
	

	thread_id_to_uct_searcher.clear();

	for (size_t i = 0; i < search_groups_size ; ++i)
		for(int j = 0;j < thread_settings[i];++j)
			thread_id_to_uct_searcher.push_back(search_groups[i].get_uct_searcher(j));

	
	
    

	
	
	

	
	for (auto& uct_searcher : thread_id_to_uct_searcher)
		uct_searcher->InitMateSearcher(search_options);

#if defined(USE_POLICY_BOOK)
	policy_book.read_book();
#endif

}


void DlshogiSearcher::TeminateThreads()
{
	
	search_limits.interruption = true;
	
	

	
    engine.threads.wait_for_search_finished();
}


void DlshogiSearcher::NewGame()
{
}


void DlshogiSearcher::GameOver()
{
#if defined(ENABLE_POLICY_BOOK_LEARN)
	
	auto last_position_cmd = Threads.main()->last_position_cmd_string;
	auto sfen = last_position_cmd.substr(strlen("position "));
	policy_book.append_sfen_to_db_bin(sfen);
#endif

}



void DlshogiSearcher::SetResignThreshold(const int resign_threshold)
{
	search_options.RESIGN_THRESHOLD = (float)resign_threshold / 1000.0f;
}





void DlshogiSearcher::SetDrawValue(const int value_black, const int value_white)
{
	search_options.draw_value_black = (float)value_black / 1000.0f;
	search_options.draw_value_white = (float)value_white / 1000.0f;
}











void DlshogiSearcher::SetPvMateSearch(const int threads,  const int nodes)
{
	
	if (threads == int(pv_mate_searchers.size()) &&
		(threads == 0 || pv_mate_searchers[0].get_nodes_limit() == nodes))
		return; 

	

	pv_mate_searchers.clear(); 
	pv_mate_searchers.reserve(threads);

	for (int i = 0; i < threads; i++)
		pv_mate_searchers.emplace_back(nodes, this);
}


void DlshogiSearcher::InitializeUctSearch() {

    if (!tree)
        tree = std::make_unique<NodeTree>(gc.get());
    
    

    
    
    for (size_t i = 0; i < search_groups_size; ++i)
        search_groups[i].set_dlsearcher(this);
}


void DlshogiSearcher::TerminateUctSearch() {
    for (auto& searcher : pv_mate_searchers)
        searcher.Term();

    

    
    
    
    
    
    
}






void DlshogiSearcher::SetLimits(const Position& pos, const Search::LimitsType& limits) {
    auto& s = search_limits;
    auto& o = search_options;

    
    s.infinite = limits.infinite;

    
    s.nodes_limit = (NodeCountType) limits.nodes;

    
    s.movetime = limits.movetime;

    
#if 0
	
	
	if (limits.nodes) {
		search_limits.node_limit = (NodeCountType)(u64)limits.nodes;
		return;
	}

	const Color color = pos->side_to_move();
	const int divisor = 14 + std::max(0, 30 - pos->game_ply());

	
	search_limits.remaining_time[BLACK] = limits.time[BLACK];
	search_limits.remaining_time[WHITE] = limits.time[WHITE];

	search_limits.time_limit = search_limits.remaining_time[color] / divisor + limits.inc[color];

	
	search_limits.minimum_time = limits.movetime;

	
	search_limits.node_limit = limits.infinite ? std::numeric_limits<NodeCountType>::max() : (NodeCountType)(u64)limits.nodes;

	
	search_limits.extend_time = limits.movetime == 0 && limits.nodes == 0;
#endif

    
    search_limits.time_manager.init(limits, pos.side_to_move(), pos.game_ply(), engine.options,
                                    o.max_moves_to_draw);
}











void DlshogiSearcher::FinalizeUctSearch() {
    TerminateUctSearch();

    search_groups.reset();
    tree.reset();  
    gc.reset();
}











Move DlshogiSearcher::UctSearchGenmove(Position&                pos,
                                       const std::string&       game_root_sfen,
                                       const std::vector<Move>& moves,
                                       Move&                    ponderMove) {

	
    for (auto& searcher : pv_mate_searchers)
        searcher.Stop(false);

    
    ponderMove = Move::none();

    
    
    

    
    
    

    
    search_limits.interruption = false;

    
    tree->ResetToPosition(game_root_sfen, moves);

    
    std::memcpy(&pos_root, &pos, sizeof(Position));

    
    Node* current_root         = tree->GetCurrentHead();
    search_limits.current_root = tree->GetCurrentHead();

    
    

    
    search_limits.nodes_searched = 0;

    
    search_limits.game_ply = pos.game_ply();

    
    
    ExpandRoot(&pos, search_options.generate_all_legal_moves);

    
    const NodeCountType pre_simulated =
      current_root->move_count != NOT_EXPANDED ? current_root->move_count.load() : 0;

	
	bool search_skipped = true;
    Book::ProbeResult probeResult;

    
    
    

    
    
    const ChildNumType child_num = current_root->child_num;
    if (child_num == 0)
    {
        probeResult.bestmove = Move::resign();
        probeResult.bestscore = mated_in(1);
		goto SEARCH_SKIP;
    }

    
    
    

    {
        
        Move move = pos.DeclarationWin();

        if (move)
        {
            
            

			probeResult.bestmove = move;
            probeResult.bestscore = mate_in(1);
            goto SEARCH_SKIP;
        }

        
        
    }

    
    
    

    

    probeResult = book.probe(pos, engine.updateContext);
    if (probeResult.bestmove)
        goto SEARCH_SKIP;

    
    
    

	search_skipped        = false;

    
    if (search_options.debug_message)
        UctPrint::PrintPlayoutLimits(search_limits.time_manager, search_limits.nodes_limit);

    

    rootMateMove = Move::none();

    
    
    current_root->dfpn_checked = false;

    
    for (auto& searcher : pv_mate_searchers)
        searcher.Run();

    
    

	
	engine.threads.start_searching();
	
	

	
	
    parallel_search(pos, 0);

	 

    
    for (auto& searcher : pv_mate_searchers)
        searcher.Stop();

    
    TeminateThreads();

    
    for (auto& searcher : pv_mate_searchers)
        searcher.Join();

SEARCH_SKIP:
    
    
    

    
    if (search_options.debug_message)
    {
        
        const TimePoint finish_time = search_limits.time_manager.elapsed_time();
        

        
        UctPrint::PrintPlayoutInformation(current_root, &search_limits, finish_time, pre_simulated);
    }

	if (search_skipped)
	{
        
        

        Search::InfoFull info;
        info.depth     = 0;
        info.selDepth  = 0;
        info.multiPV   = 1;
        info.score     = probeResult.bestscore;
        TimePoint time = std::max(TimePoint(1), search_limits.time_manager.elapsed_time());
        info.timeMs    = time;
        info.nodes     = 0;
        info.nps       = 0;
        std::string pv = probeResult.bestmove.to_usi_string();
        if (probeResult.pondermove)
            pv += " " + probeResult.pondermove.to_usi_string();
        info.pv       = pv;
        info.hashfull = 0; 
        engine.updateContext.onUpdateFull(info);

		ponderMove = probeResult.pondermove;
        return probeResult.bestmove;
	}

    
    auto best = UctPrint::get_best_move_multipv(current_root, search_limits, search_options, engine.updateContext);

    
    
    

    
    
    
    
    

    
    
    

    
    

    
    
    if (rootMateMove != Move::none())
        return rootMateMove;

    
    if (best.wp < search_options.RESIGN_THRESHOLD)
    {
        ponderMove = Move::none();
        return Move::resign();
    }

    
    ponderMove = best.ponder;

    return best.move;
}



void DlshogiSearcher::ExpandRoot(const Position* pos, bool generate_all) {
    Node* current_head = tree->GetCurrentHead();
    if (current_head->child_num == 0)
    {
        current_head->ExpandNode(pos, generate_all);
    }
}



void DlshogiSearcher::OutputPvCheck() {
    auto& s = search_limits;
    auto& o = search_options;

    
    if (!o.pv_interval)
        return;

    
    const auto elapsed_time = s.time_manager.elapsed_time();
    if (elapsed_time > s.last_pv_print + o.pv_interval)
    {

        
        
        UctPrint::get_best_move_multipv(tree->GetCurrentHead(), search_limits, search_options, engine.updateContext);

        
        
        s.last_pv_print = elapsed_time;
    }
}




void pv_key(Position& pos, Node* node, int ply, Key64 keys[]) {
    if (ply == 0)
        keys[ply] = pos.key();
    else if (node == nullptr || node->child_num == 0)
        keys[ply] = 0;
    else
    {
        if (node->child_nodes.get() == nullptr)
            return;
        

        ChildNumType max_i = 0;
        for (ChildNumType i = 1; i < node->child_num; ++i)
            if (node->child[i].move_count > node->child[max_i].move_count)
                max_i = i;

        StateInfo si;
        Move      m = node->child[max_i].move;
        
        pos.do_move(m, si);
        pv_key(pos, node->child_nodes[max_i].get(), ply - 1, keys);
        pos.undo_move(m);
    }
}



void DlshogiSearcher::InterruptionCheck(const Position& rootPos) {
    auto& s = search_limits;
    auto& o = search_options;

    
    if (s.interruption || engine.threads.stop)
        return;

    
    const auto elapsed_from_ponderhit = s.time_manager.elapsed_time_from_ponderhit();

    
    

    
    
    
    auto interrupt = [&]() {
        if (o.debug_message)
            sync_cout << "info string search search_end = " << s.time_manager.search_end
                      << "[ms], interruption time = " << elapsed_from_ponderhit << "[ms]"
                      << sync_endl;

        s.interruption = true;
    };

    
    

    
    
    if (s.nodes_limit)
    {
        if (s.nodes_searched >= s.nodes_limit)
            interrupt();

        
        return;
    }

    
    
    
    
    
    
    
    
    
    
    
    
    if ((NodeCountType) (s.current_root->move_count + 1) > o.uct_node_limit)
    {
        
        

        interrupt();
        return;
    }

    
    
    if (s.infinite)
        return;

    
    if (search_limits.ponder)
        return;

    

    
    if (rootMateMove != Move::none())
    {
        interrupt();
        return;
    }

    
    
    
    if (s.movetime && elapsed_from_ponderhit > s.movetime)
    {
        
        interrupt();
        return;
    }

    
    if (s.time_manager.search_end)
    {
        if (elapsed_from_ponderhit >= s.time_manager.search_end)
        {
            
            interrupt();
            return;
        }
        else
        {
            
            return;
        }
    }

    
    
    auto mimimum =
      std::max(s.time_manager.minimum() - SearchInterruptionChecker::kCheckIntervalMs, s64(0));
    auto optimum =
      std::max(s.time_manager.optimum() - SearchInterruptionChecker::kCheckIntervalMs, s64(0));
    auto maximum =
      std::max(s.time_manager.maximum() - SearchInterruptionChecker::kCheckIntervalMs, s64(0));

    const Node* current_root = tree->GetCurrentHead();
    const int   child_num    = current_root->child_num;
    if (child_num == 1)
    {
        
        s.time_manager.search_end = mimimum;
        return;
    }

    
    if (elapsed_from_ponderhit < mimimum)
        return;

    
    if (elapsed_from_ponderhit > maximum)
    {
        
        interrupt();
        return;
    }

    const ChildNode* uct_child = current_root->child.get();

    
    for (size_t i = 0; i < current_root->child_num; ++i)
        if (uct_child[i].IsLose())
        {
            
            interrupt();
            return;
            
            
            
        }

    
    
    
    
    double game_ply_factor =
      s.game_ply < 20
        ? 1.5  
      : s.game_ply < 30 ? 3.5
      : s.game_ply < 40 ? 4.0
                        : 3.0;
    
    

    
    maximum = std::min((s64) (optimum * game_ply_factor), maximum);

    
    
    
    
    
    
    
    
    
    
    
    
    
    

    auto elapsed = s.time_manager.elapsed_time();

    
    

    
    s64 rest_maximum_po = std::max(
      (s64) (s.nodes_searched * (maximum - elapsed_from_ponderhit) / (elapsed + 1)), (s64) 0);

    
    

    
    
    

    
    int best_i = 0, second_i = -1, third_i = -1;

    
    for (int i = 1; i < child_num; i++)
    {
        if (uct_child[i].move_count > uct_child[best_i].move_count)
        {
            third_i  = second_i;
            second_i = best_i;
            best_i   = i;
        }
        else if (second_i == -1 || uct_child[i].move_count > uct_child[second_i].move_count)
        {
            third_i  = second_i;
            second_i = i;
        }
        else if (third_i == -1 || uct_child[i].move_count > uct_child[third_i].move_count)
            third_i = i;
    }

    NodeCountType best_searched   = uct_child[best_i].move_count;
    NodeCountType second_searched = uct_child[second_i].move_count;

    
    
    

    const WinType delta          = (WinType) 0.00001f;  
    WinType       best_winrate   = uct_child[best_i].win / (best_searched + delta);
    WinType       second_winrate = uct_child[second_i].win / (second_searched + delta);

    
    for (;;)
    {
        if (rest_maximum_po > 0 )
        {
            WinType eval_diff = best_winrate - second_winrate;
            bool    converged = false;

            
            if (
              second_i != third_i && third_i != -1 && elapsed >= optimum / 8
              
              
            )
            {
                
                

                Node* node1 = current_root->child_nodes[best_i].get();
                Node* node2 = current_root->child_nodes[second_i].get();

                if (node1 != nullptr && node2 != nullptr)
                {
                    
                    Position pos;
                    std::memcpy(&pos, &rootPos, sizeof(Position));
                    StateInfo si;

                    Move m1 = uct_child[best_i].move;
                    Move m2 = uct_child[second_i].move;

                    
                    

                    pos.do_move(m1, si);
                    Key64 k1[4] = {}, k2[4] = {};
                    pv_key(pos, node1, 3, k1);  
                    pos.undo_move(m1);
                    pos.do_move(m2, si);
                    pv_key(pos, node2, 3, k2);  
                    pos.undo_move(m2);

                    
                    
                    
                    if ((k1[0] == k2[0] && k1[0] != 0) || (k1[2] == k2[2] && k1[2] != 0))
                    {
                        

                        
                        NodeCountType third_searched = uct_child[third_i].move_count;
                        WinType third_winrate = uct_child[third_i].win / (third_searched + delta);

                        
                        
                        second_searched = third_searched;
                        second_winrate  = third_winrate;
                        eval_diff       = best_winrate - third_winrate;

                        converged = true;
                    }
                }
            }

            
            
            if (best_winrate >= second_winrate && best_searched >= second_searched * 1.5)
            {
                
                
                WinType ratio = std::max(1.0 - eval_diff * 5, 0.0);

                
                
                if (elapsed >= optimum / 8
                    && best_searched > second_searched + rest_maximum_po * ratio)
                {
                    if (o.debug_message)
                        sync_cout << "info string interrupted by early exit"
                                  << " , best_searched > second_searched + rest_maximum_po * "
                                  << ratio << " "
                                  << " , best_searched = " << best_searched
                                  << " , second_searched = " << second_searched
                                  << " , rest_maximum_po = " << rest_maximum_po
                                  << " , elapsed = " << elapsed << " , eval_diff = " << eval_diff
                                  << " , ratio = " << ratio << " , converged = " << converged
                                  << sync_endl;

                    break;
                }
            }

            
            
            

            
            

            if (best_searched > second_searched + rest_maximum_po)
            {
                if (o.debug_message)
                    sync_cout << "info string interrupted by retirement"
                              << " , best_searched > second_searched + rest_maximum_po"
                              << " , best_searched = " << best_searched
                              << " , second_searched = " << second_searched
                              << " , rest_maximum_po = " << rest_maximum_po << sync_endl;

                break;
            }
        }
        else
        {  

            if (o.debug_message)
                sync_cout << "info string maximum time is over"
                          << " , rest_maximum_po == 0"
                          << " , best_winrate = " << best_winrate
                          << " , second_winrate = " << second_winrate
                          << " , best_searched = " << best_searched
                          << " , second_searched = " << second_searched << sync_endl;

            break;
        }

        
        return;
    }
    
    s.time_manager.search_end = s.time_manager.round_up(elapsed_from_ponderhit);
}





void DlshogiSearcher::parallel_search(const Position& rootPos, size_t thread_id)
{
	
	

	
	

	

	
	

	auto s = thread_id_to_uct_searcher.size();

	
	if (thread_id < s)
		thread_id_to_uct_searcher[thread_id]->ParallelUctSearchStart(rootPos);

	
	else if (thread_id == s)
		interruption_checker->Worker(rootPos);

	else
		ASSERT_LV3(false);
}







void SearchInterruptionChecker::Worker(const Position& rootPos)
{
	

	
	auto stop = [&]() {
		return ds->engine.threads.stop.load() || ds->search_limits.interruption;
	};

	
	ds->ResetLastPvPrint();

	while (!stop() ) {
		std::this_thread::sleep_for(std::chrono::milliseconds(kCheckIntervalMs));

		
		ds->InterruptionCheck(rootPos);

		
		if (stop())
			break;

		
		ds->OutputPvCheck();
	};
}

} 


#endif 
