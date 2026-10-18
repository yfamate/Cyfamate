#ifndef __DLSHOGI_SEARCHER_H_INCLUDED__
#define __DLSHOGI_SEARCHER_H_INCLUDED__
#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "../../position.h"
#include "../../book/book.h"
#include "../../book/policybook.h"
#include "../../mate/mate.h"
#include "../../timeman.h"
#include "dlshogi_types.h"
#include "SearchOptions.h"
#include "PvMateSearch.h"


namespace dlshogi {

	struct Node;
	class NodeTree;
	class NodeGarbageCollector;
	class UctSearcher;
	class UctSearcherGroup;
	class SearchInterruptionChecker;
	class DlshogiSearcher;
    class FukauraOuEngine;

	struct SearchLimits
	{

		NodeCountType nodes_limit = 0;


		TimeManagement time_manager;


		TimePoint movetime;

		bool infinite;


		Color root_color;

		Node* current_root;

		
		std::atomic<NodeCountType> nodes_searched;


		TimePoint last_pv_print;


		std::atomic<bool> interruption;

		int game_ply;

		std::atomic_bool ponder;
	};



	class MutexPool
	{
	public:
		static const uint64_t MUTEX_NUM = 65536; 
		static_assert((MUTEX_NUM & (MUTEX_NUM - 1)) == 0);

		std::mutex& get_mutex(const Position* pos)
		{
			return get_mutex(pos->key());
		}

	private:
		std::mutex& get_mutex(const Key key)
		{
			return mutexes[key & (MUTEX_NUM - 1)];
		}

	private:
		std::mutex mutexes[MUTEX_NUM];
	};

	class DlshogiSearcher
	{
	public:
        DlshogiSearcher(FukauraOuEngine& engine);
		~DlshogiSearcher(){ FinalizeUctSearch(); }

		void add_options(OptionsMap& options);

        void InitGPU(const std::string& model_path, const std::string& model_architecture,
                     std::vector<int> thread_settings, int policy_value_batch_maxsize);

		void NewGame();

		void GameOver();

		void SetResignThreshold(const int resign_threshold);

		void SetDrawValue(const int value_black, const int value_white);

		float draw_value(Color side_to_move) const {
			return (search_limits.root_color == BLACK)
				? (side_to_move == BLACK ? search_options.draw_value_black : 1 - search_options.draw_value_black)
				: (side_to_move == WHITE ? search_options.draw_value_white : 1 - search_options.draw_value_white);
		}


		void SetEvalCoef(const int eval_coef);

		void SetPvMateSearch(const int threads,  const int nodes);

		void SetGetnerateAllLegalMoves(bool flag) { search_options.generate_all_legal_moves = flag; }

		void InitializeUctSearch();

		void TerminateUctSearch();

		void SetLimits(const Position& pos, const Search::LimitsType& limits);

		void FinalizeUctSearch();


		Move UctSearchGenmove(Position& pos, const std::string& game_root_sfen , const std::vector<Move>& moves, Move& ponderMove);


		void ResetLastPvPrint() { search_limits.last_pv_print = 0; }

		NodeTree* get_node_tree() const { return tree.get(); }

		void parallel_search(const Position& rootPos , size_t thread_id);


		SearchLimits search_limits;

		SearchOptions search_options;

		std::mutex mutex_expand;

		Book::BookMoveSelector book;

		void InterruptionCheck(const Position& rootPos);

		void OutputPvCheck();

		std::mutex& get_node_mutex(const Position* pos) { return node_mutexes.get_mutex(pos); }

		Position pos_root;

		Move rootMateMove;

#if defined(USE_POLICY_BOOK)
		PolicyBook policy_book;
#endif

        FukauraOuEngine& engine;

	private:

		void ExpandRoot(const Position* pos , bool generate_all);


		void StartThreads();
		void TeminateThreads();

		std::unique_ptr<UctSearcherGroup[]> search_groups;
		size_t search_groups_size = 0;

		std::unique_ptr<NodeTree> tree;
		
		std::unique_ptr<NodeGarbageCollector> gc;

		std::unique_ptr<SearchInterruptionChecker> interruption_checker;

		std::tuple<Move , float , Move > get_and_print_pv();

		typedef std::vector<Move> PV;
		typedef std::vector<PV> PVs;

		std::vector<int> thread_id_to_gpu_id;
		std::vector<UctSearcher*> thread_id_to_uct_searcher;

        std::vector<int>          last_thread_settings;

		MutexPool node_mutexes;

		std::vector<PvMateSearcher> pv_mate_searchers;
	};

	class SearchInterruptionChecker
	{
	public:
		SearchInterruptionChecker(DlshogiSearcher* ds) : ds(ds) {}

		static const int kCheckIntervalMs = 10;

		void Worker(const Position& rootPos);

	private:
		DlshogiSearcher* ds;
	};

} 

#endif 
#endif 
