#ifndef __PV_MATE_SEARCH_CPP__
#define __PV_MATE_SEARCH_CPP__

#include "PvMateSearch.h"
#if defined(YANEURAOU_ENGINE_DEEP)

#include <cstring> 

#include "dlshogi_searcher.h"
#include "UctSearch.h"

namespace dlshogi {

	
	
	

	PvMateSearcher::PvMateSearcher(const int nodes, DlshogiSearcher* dl_searcher ) :
		ready_th(true),
		term_th(false),
		th(nullptr),
		dfpn(Mate::Dfpn::DfpnSolverType::Node48bitOrdering),
		dl_searcher(dl_searcher)
		
	{
		
		dfpn.alloc_by_nodes_limit((size_t)(nodes * 8));
		nodes_limit = nodes;
		
		dfpn.set_max_game_ply(dl_searcher->search_options.max_moves_to_draw);
	}

	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	void PvMateSearcher::SearchInner(Position& pos, Node* uct_node, ChildNode* child_node, bool root)
	{
		
		if (stop) return;

		
		Node::mtx_dfpn.lock();

		
		if (!(uct_node->dfpn_proven_unsolvable || uct_node->dfpn_checked))
		{
			

			
			uct_node->dfpn_checked = true;
			Node::mtx_dfpn.unlock();

			
			Move mate_move = dfpn.mate_dfpn(pos, nodes_limit);
			if (mate_move.is_ok()) {
				

				
				if (root)
				{
					
					int mate_ply = dfpn.get_mate_ply();
					sync_cout << "info string found the root mate by df-pn , move = " << to_usi_string(mate_move) << " ,ply = " << mate_ply << sync_endl;

					
					

					
					std::stringstream pv;
					for (auto move : dfpn.get_pv())
						pv << ' ' << to_usi_string(move);
					sync_cout << "info score mate " << mate_ply << " pv" << pv.str() << sync_endl;

					
					
					auto* child = uct_node->child.get();

					for (size_t i = 0; i < uct_node->child_num; ++i)
						if (child[i].getMove() == mate_move)
							
						{
							child[i].SetLose();
							break;
						}

					
					
					dl_searcher->rootMateMove = mate_move;

				} else {

					
					
					
					child_node->SetWin();
					
					

#if 0
					
					int mate_ply = dfpn.get_mate_ply();
					int gamePly = pos.game_ply();
					sync_cout << "info string found the pv mate by df-pn , move = " << to_usi_string(move)
						<< " ,ply = " << mate_ply << " , gamePly = " << gamePly << sync_endl;

					
					uct_node->mate_ply = mate_ply;

					
					std::stringstream pv;
					for (auto move : dfpn.get_pv())
						pv << ' ' << to_usi_string(move);
					sync_cout << "info string score mate " << mate_ply << " pv" << pv.str() << sync_endl;
#endif
				}
			}
			else if (stop) {
				
				std::lock_guard<std::mutex> lock(Node::mtx_dfpn);
				uct_node->dfpn_checked = false;
			}
			
		}
		else {
			Node::mtx_dfpn.unlock();

			
			

			
			if (!uct_node->IsEvaled() || !uct_node->child) {
				std::this_thread::yield();
				return;
			}

			
			
			const auto next_index = select_max_child_node(uct_node);

			auto uct_child = uct_node->child.get();

			
			if (uct_child[next_index].IsWin() || uct_child[next_index].IsLose()) {
				std::this_thread::yield();
				return;
			}

			
			if (!uct_node->child_nodes)
				return;

			auto uct_next = uct_node->child_nodes[next_index].get();
			if (!uct_next)
				return;

			if (uct_node->child_nodes && uct_node->child_nodes[next_index])
			{
				
				StateInfo st;
				Move m = uct_node->child[next_index].move;
				pos.do_move(m, st);

				
				SearchInner(pos, uct_next, &uct_node->child[next_index], false);
			}
			else
				std::this_thread::yield();
		}
	}

#if 0
	
	
	void PvMateSearcher::SearchInnerBFS(Position& pos, Node* uct_node, ChildNode* child_node, bool root, int depth)
	{
		
		if (stop) return;

		
		Node::mtx_dfpn.lock();

		
		if (!(uct_node->dfpn_proven_unsolvable || uct_node->dfpn_checked))
		{
			

			
			uct_node->dfpn_checked = true;
			Node::mtx_dfpn.unlock();

			
			Move mate_move = dfpn.mate_dfpn(pos, nodes_limit);
			if (is_ok(mate_move)) {
				

				
				if (root)
				{
					
					int mate_ply = dfpn.get_mate_ply();
					sync_cout << "info string found the root mate by df-pn , move = " << to_usi_string(mate_move) << " ,ply = " << mate_ply << sync_endl;

					
					

					
					std::stringstream pv;
					for (auto move : dfpn.get_pv())
						pv << ' ' << to_usi_string(move);
					sync_cout << "info score mate " << mate_ply << " pv" << pv.str() << sync_endl;

					
					
					auto* child = uct_node->child.get();
					for (size_t i = 0; i < uct_node->child_num; ++i)
						if (child[i].getMove() == mate_move)
							
						{
							child[i].SetLose();
							break;
						}

				}
				else {

					
					
					
					child_node->SetWin();
					
					

				}
			}
			else if (stop) {
				
				std::lock_guard<std::mutex> lock(Node::mtx_dfpn);
				uct_node->dfpn_checked = false;
			}
			
			return;
		}
		else {
			Node::mtx_dfpn.unlock();

			

			
			if (depth < 1)
				return;

			
			

			
			if (!uct_node->IsEvaled() || !uct_node->child) {
				return;
			}

			
			if (!uct_node->child_nodes)
				return;

			
			
			int indices[MAX_MOVES];
			int N = 4; 
			select_nth_child_node(uct_node, N, indices);

			for (int i = 0; i < N; ++i)
			{
				int next_index = indices[i];
				
				if (next_index == -1)
					break;

				auto uct_next = uct_node->child_nodes[next_index].get();
				
				if (!uct_next)
					continue;

				
				StateInfo st;
				Move m = uct_node->child[next_index].getMove();
				sync_cout << to_usi_string(m) << sync_endl;
				pos.do_move(m, st);

				
				SearchInnerBFS(pos, uct_next, &uct_node->child[next_index], false, depth - 1);

				pos.undo_move(m);
			}
		}
	}
#endif

	void PvMateSearcher::Run()
	{
		
		if (th == nullptr) {
			th = new std::thread([&]() {
				while (!term_th) {
					
					while (!stop) {
						
						
						Position pos_copy;
						auto& pos_root = dl_searcher->pos_root;
						std::memcpy(&pos_copy, &pos_root, sizeof(Position));
						
						

						
						auto* tree = dl_searcher->get_node_tree();

						SearchInner(pos_copy, tree->GetCurrentHead(), nullptr, true);
#if 0
						
						
						if ((thread_id & 1) == 0)
							SearchInner(pos_copy, tree->GetCurrentHead(), nullptr, true);
						else
						{
							
							SearchInnerBFS(pos_copy, tree->GetCurrentHead(), nullptr, true, 6);
							std::this_thread::yield();
						}
#endif
					}

					std::unique_lock<std::mutex> lk(mtx_th);
					ready_th = false;
					cond_th.notify_all();

					
					cond_th.wait(lk, [this] { return ready_th || term_th; });
				}
				});
		}
		else {
			
			
			std::unique_lock<std::mutex> lk(mtx_th);
			ready_th = true;
			cond_th.notify_all();
		}
	}

	void PvMateSearcher::Stop(const bool stop)
	{
		dfpn.dfpn_stop(stop);
		this->stop = stop;
	}

	void PvMateSearcher::Join()
	{
		std::unique_lock<std::mutex> lk(mtx_th);
		if (ready_th && !term_th)
			cond_th.wait(lk, [this] { return !ready_th || term_th; });
	}

	
	void PvMateSearcher::Term()
	{
		{
			std::unique_lock<std::mutex> lk(mtx_th);
			term_th = true;
			ready_th = false;
			cond_th.notify_all();
		}
		if (th)
		{
			th->join();
			delete th;
			th = nullptr; 
		}
	}

} 

#endif 
#endif 
