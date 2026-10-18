#include <algorithm> 
#include <cmath>     
#include <map>
#include <memory>
#include <unordered_map>

#include "history.h"
#include "thread.h"
#include "usi.h"
#include "tt.h"
#include "movegen.h"

namespace YaneuraOu {








Thread::Thread(Search::SharedState& sharedState,
               
               Search::WorkerFactory          worker_factory,
               size_t                         n,               
               size_t                         numaN,           
               size_t                         totalNumaCount,  
               OptionalThreadToNumaNodeBinder binder) :
    idx(n),
    idxInNuma(numaN),
    totalNuma(totalNumaCount),
    
    stdThread(&Thread::idle_loop, this) {

#if !defined(__EMSCRIPTEN__)

#if STOCKFISH
	run_custom_job([this, &binder, &sharedState , n]() {
#else
	run_custom_job([this, &binder, &sharedState , n , worker_factory]() {
#endif
		
        
        

        
        

        this->numaAccessToken = binder();
#if STOCKFISH
        this->worker = make_unique_large_page<Search::Worker>(
          sharedState, std::move(sm), n, idxInNuma, totalNuma, this->numaAccessToken);
#else
		
		this->worker = std::move(worker_factory(sharedState,
												{n, idxInNuma, totalNuma, this->numaAccessToken}));
#endif
    });

    
    wait_for_search_finished();

#else
    
    
    
    
    
    
    
    
    
    
    
#endif
}








Thread::~Thread() {

    
    ASSERT_LV3(!searching);

    
    exit = true;
    start_searching();
    stdThread.join();
}




void Thread::start_searching() {
    assert(worker != nullptr);
    run_custom_job([this]() { worker->start_searching(); });
}




void Thread::clear_worker() {
    assert(worker != nullptr);
    run_custom_job([this]() { worker->clear(); });
}




void Thread::wait_for_search_finished() {

    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&] { return !searching; });
}




void Thread::run_custom_job(std::function<void()> f) {
    {
        std::unique_lock<std::mutex> lk(mutex);
        cv.wait(lk, [&] { return !searching; });
        jobFunc   = std::move(f);
        searching = true;
    }
    cv.notify_one();
}

void Thread::ensure_network_replicated() { worker->ensure_network_replicated(); }







void Thread::idle_loop() {
    while (true)
    {
        std::unique_lock<std::mutex> lk(mutex);
        searching = false;
        cv.notify_one();  
                          

        cv.wait(lk, [&] { return searching; });

        if (exit)
            return;

        std::function<void()> job = std::move(jobFunc);
        jobFunc                   = nullptr;

        lk.unlock();

        if (job)
            job();
    }
}




uint64_t ThreadPool::nodes_searched() const { return accumulate(&Search::Worker::nodes); }


static size_t next_power_of_two(uint64_t count) { return count > 1 ? (2ULL << msb(count - 1)) : 1; }

bool cyf_privhist_active();









void ThreadPool::set(const NumaConfig&   numaConfig,
                     Search::SharedState sharedState,
                     const Search::UpdateContext& updateContext,
                     
                     size_t                       requested_threads,
                     const Search::WorkerFactory& worker_factory
) {

    

    
    if (threads.size() > 0)  
    {
        main_thread()->wait_for_search_finished();

        
        threads.clear();

        boundThreadToNumaNode.clear();
    }

#if STOCKFISH
    const size_t requested = sharedState.options["Threads"];
#else
    const size_t requested = requested_threads;
    
#endif

    if (requested > 0)  
    {
        
        
        
        
        
        

        
        
        
        
        
        
        
        

        
        
        
        
        
        

        

        const std::string numaPolicy(sharedState.options["NumaPolicy"]);
        const bool        doBindThreads = [&]() {
            if (numaPolicy == "none")
                return false;

            if (numaPolicy == "auto")
                return numaConfig.suggests_binding_threads(requested);

            
            

            return true;
        }();

        std::map<NumaIndex, size_t> counts;

        boundThreadToNumaNode = doBindThreads
                                ? numaConfig.distribute_threads_among_numa_nodes(requested)
                                : std::vector<NumaIndex>{};

        if (boundThreadToNumaNode.empty())
            counts[0] = requested;  
        else
        {
            for (size_t i = 0; i < boundThreadToNumaNode.size(); ++i)
                counts[boundThreadToNumaNode[i]]++;
        }

        sharedState.sharedHistories.clear();
        for (auto pair : counts)
        {
            NumaIndex numaIndex = pair.first;
            uint64_t  count     = pair.second;
            auto      f         = [&]() {
                sharedState.sharedHistories.try_emplace(numaIndex, cyf_privhist_active() ? size_t(1) : next_power_of_two(count));   
            };
            if (doBindThreads)
                numaConfig.execute_on_numa_node(numaIndex, f);
            else
                f();
        }

        auto threadsPerNode = counts;
        counts.clear();

        while (threads.size() < requested)
        {
            const size_t    threadId      = threads.size();
            const NumaIndex numaId        = doBindThreads ? boundThreadToNumaNode[threadId] : 0;
            auto create_thread = [&]() {
#if STOCKFISH
                auto manager = threadId == 0
                               ? std::unique_ptr<Search::ISearchManager>(
                                   std::make_unique<Search::SearchManager>(updateContext))
                               : std::make_unique<Search::NullSearchManager>();
                
                
                
                
#endif

                
                
                
                

                
                
                
                
                

                auto binder = doBindThreads ? OptionalThreadToNumaNodeBinder(numaConfig, numaId)
                                            : OptionalThreadToNumaNodeBinder(numaId);
#if STOCKFISH
                threads.emplace_back(std::make_unique<Thread>(sharedState, std::move(manager),
                                                              threadId, counts[numaId]++,
                                                              threadsPerNode[numaId], binder));
#else
                threads.emplace_back(std::make_unique<Thread>(sharedState, worker_factory, threadId,
                                                              counts[numaId]++,
                                                              threadsPerNode[numaId], binder));
#endif
            };

            
            if (doBindThreads)
                numaConfig.execute_on_numa_node(numaId, create_thread);
            else
                create_thread();
        }

        
        
        
        clear();

        
        main_thread()->wait_for_search_finished();
    }
}









void ThreadPool::clear() {
	if (threads.size() == 0)
		return;

	for (auto&& th : threads)
		th->clear_worker();

	for (auto&& th : threads)
		th->wait_for_search_finished();

	
	
#if STOCKFISH
	
	main_manager()->bestPreviousAverageScore = VALUE_INFINITE;
	main_manager()->previousTimeReduction = 0.85;

	main_manager()->callsCnt = 0;
	main_manager()->bestPreviousScore = VALUE_INFINITE;
	main_manager()->originalTimeAdjust = -1;
	main_manager()->tm.clear();
#endif
}

void ThreadPool::run_on_thread(size_t threadId, std::function<void()> f) {
	assert(threads.size() > threadId);
	threads[threadId]->run_custom_job(std::move(f));
}

void ThreadPool::wait_on_thread(size_t threadId) {
	assert(threads.size() > threadId);
	threads[threadId]->wait_for_search_finished();
}

size_t ThreadPool::num_threads() const { return threads.size(); }








void ThreadPool::start_thinking(const OptionsMap&  options,
                                Position&          pos,
                                StateListPtr&      states,
                                Search::LimitsType limits) {

    main_thread()->wait_for_search_finished();
#if defined(CYFAMATE_NNUE_5GP)
    
#endif

    
    
    
	
#if STOCKFISH
    main_manager()->stopOnPonderhit = stop = abortedSearch = false;
    main_manager()->ponder                                 = limits.ponderMode;
    increaseDepth                                          = true;
#else
    stop = abortedSearch = false;
#endif

    Search::RootMoves rootMoves;
#if STOCKFISH
    const auto legalmoves = MoveList<LEGAL_ALL>(pos);

    for (const auto& usiMove : limits.searchmoves)
    {
        auto move = USIEngine::to_move(pos, usiMove);

        if (std::find(legalmoves.begin(), legalmoves.end(), move) != legalmoves.end())
            rootMoves.emplace_back(move);
    }

    
    
    if (rootMoves.empty())
        for (const auto& m : legalmoves)
            rootMoves.emplace_back(m);

#else

    
    bool generate_all_legal_moves =
      options.count("GenerateAllLegalMoves") && options["GenerateAllLegalMoves"];

    
    

    auto setup_rootMoves = [&](auto& moveList) {
        for (const auto& usiMove : limits.searchmoves)
        {
            auto move = USIEngine::to_move(pos, usiMove);

			
            if (std::find(moveList.begin(), moveList.end(), move) != moveList.end())
                rootMoves.emplace_back(move);
        }

        if (rootMoves.empty())
            for (const auto& m : moveList)
                rootMoves.emplace_back(m);
    };

    if (generate_all_legal_moves) {
        auto legalmoves = MoveList<LEGAL_ALL>(pos);
        setup_rootMoves(legalmoves);
    } else {
        auto legalmoves = MoveList<LEGAL>(pos);
        setup_rootMoves(legalmoves);
    }

#endif

    
    

    
    

    
    

    assert(states.get() || setupStates.get());

    if (states.get())
        setupStates = std::move(states);  
#if !STOCKFISH
    Position::set_repetition_history(pos);
#endif

    
    
    
    
    

    
    
    
    

    for (auto&& th : threads)
    {
        th->run_custom_job([&]() {
#if STOCKFISH
            th->worker->limits = limits;
            th->worker->nodes = th->worker->tbHits = th->worker->bestMoveChanges = 0;
            th->worker->nmpMinPly                                                = 0;
            th->worker->rootDepth = th->worker->completedDepth = 0;
            th->worker->rootMoves                              = rootMoves;
            th->worker->rootPos.set(pos.fen(), pos.is_chess960(), &th->worker->rootState);
            th->worker->rootState = setupStates->back();
            th->worker->tbConfig  = tbConfig;
#else

            th->worker->limits = limits;
            th->worker->nodes  = 0;
#endif

			

#if STOCKFISH
            th->worker->nmpMinPly = 0;
			th->worker->bestMoveChanges = 0;
            th->worker->rootDepth = th->worker->completedDepth = 0;

            
            
#endif

            th->worker->rootMoves = rootMoves;
            th->worker->rootPos.set(pos.sfen(), &th->worker->rootState);
            th->worker->rootState = setupStates->back();
            th->worker->rootPos.bind_repetition_history();
#if defined(CYFAMATE_NNUE_5GP)
            Cyfamate::gp_init_parts(th->worker->rootPos, &th->worker->rootState);   
#endif

#if !STOCKFISH
			
			
            th->worker->pre_start_searching();
#endif
		});
    }

    for (auto&& th : threads)
        th->wait_for_search_finished();

    main_thread()->start_searching();
}












void ThreadPool::start_searching() {

	for (auto&& th : threads)
		if (th != threads.front())
			th->start_searching();
}






void ThreadPool::wait_for_search_finished() const {

	for (auto&& th : threads)
		if (th != threads.front())
			th->wait_for_search_finished();
}

std::vector<size_t> ThreadPool::get_bound_thread_count_by_numa_node() const {
	std::vector<size_t> counts;

	if (!boundThreadToNumaNode.empty())
	{
		NumaIndex highestNumaNode = 0;
		for (NumaIndex n : boundThreadToNumaNode)
			if (n > highestNumaNode)
				highestNumaNode = n;

		counts.resize(highestNumaNode + 1, 0);

		for (NumaIndex n : boundThreadToNumaNode)
			counts[n] += 1;
	}

	return counts;
}

void ThreadPool::ensure_network_replicated() {
	for (auto&& th : threads)
		th->ensure_network_replicated();
}

} 
