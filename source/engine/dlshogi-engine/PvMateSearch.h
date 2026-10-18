#ifndef __PV_MATE_SEARCH_H__
#define __PV_MATE_SEARCH_H__

#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)

#include <thread>
#include <set>

#include "../../mate/mate.h"
#include "Node.h"

namespace dlshogi {

	class DlshogiSearcher;

	class PvMateSearcher
	{
	public:
		PvMateSearcher(const int nodes, DlshogiSearcher* dl_searcher);

		PvMateSearcher(PvMateSearcher&& o) noexcept :
			th(o.th), dfpn(std::move(o.dfpn)), dl_searcher(o.dl_searcher), nodes_limit(o.nodes_limit),
			ready_th(o.ready_th), term_th(o.term_th)
		{} 

		int get_nodes_limit() const { return nodes_limit;  }

		void Run();

		void Stop(const bool stop = true);

		void Join();

		void Term();

		~PvMateSearcher() { Term(); }

	private:
		void SearchInner(Position& pos, Node* uct_node, ChildNode* child_node, bool root);



		std::thread* th;

		Mate::Dfpn::MateDfpnSolver dfpn;
		int nodes_limit;

		std::atomic<bool> stop;

		DlshogiSearcher* dl_searcher;





		std::mutex mtx_th;
		std::condition_variable cond_th;

		bool ready_th;

		bool term_th;
	};

} 

#endif 
#endif 

