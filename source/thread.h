#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <atomic>
#include <condition_variable>


#include <mutex>
#include <vector>

#include "memory.h"
#include "numa.h"
#include "position.h"
#include "search.h"
#include "thread_win32_osx.h"

namespace YaneuraOu {
















class OptionalThreadToNumaNodeBinder {
public:
	OptionalThreadToNumaNodeBinder(NumaIndex n) :
		numaConfig(nullptr),
		numaId(n) {}

	OptionalThreadToNumaNodeBinder(const NumaConfig& cfg, NumaIndex n) :
		numaConfig(&cfg),
		numaId(n) {}

	
	NumaReplicatedAccessToken operator()() const {
		if (numaConfig != nullptr)
			
			return numaConfig->bind_current_thread_to_numa_node(numaId);
		else
			return NumaReplicatedAccessToken(numaId);
	}

private:
	const NumaConfig* numaConfig;
	NumaIndex         numaId;
};


















class Thread {
public:

	
	
	
	
	Thread(
		Search::SharedState&,
#if STOCKFISH
		std::unique_ptr<Search::ISearchManager>,
#else
		
		Search::WorkerFactory          factory,
#endif
		size_t                         threadIdx,
		size_t						   numaThreadIdx,
		size_t						   numaTotal,
		OptionalThreadToNumaNodeBinder binder
	);
	virtual ~Thread();

	
	void idle_loop();

	
	
	

	
	
	void start_searching();

	
	
	void clear_worker();

	
	void run_custom_job(std::function<void()> f);

	
	
	void ensure_network_replicated();

	
	
	
	
	

	
	
	
	
	
	

	
	

	void   wait_for_search_finished();

	
	
	size_t id() const { return idx; }

	
	LargePagePtr<Search::Worker> worker;

	
	std::function<void()>           jobFunc;

private:
	
	std::mutex                mutex;

	
	std::condition_variable   cv;

	
	
    size_t idx, idxInNuma, totalNuma ;
    

	
	
	bool                      exit = false, searching = true;  
															   

	
	
	NativeThread              stdThread;

	
	NumaReplicatedAccessToken numaAccessToken;
};











class ThreadPool {
public:
	ThreadPool() {}

	~ThreadPool() {
		
		

		if (threads.size() > 0)
		{
			main_thread()->wait_for_search_finished();

			threads.clear();
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;

	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	
	void   start_thinking(const OptionsMap&, Position&, StateListPtr&, Search::LimitsType);

	
	void   run_on_thread(size_t threadId, std::function<void()> f);

	
	void   wait_on_thread(size_t threadId);

	
	size_t num_threads() const;

	
    
    void clear();

	
    
    void set(const NumaConfig&            numaConfig,
             Search::SharedState sharedState,
             const Search::UpdateContext&,
			 
             size_t                       requested_threads,
             const Search::WorkerFactory& worker_factory
	);
    

#if STOCKFISH
	Search::SearchManager* main_manager();
#else
	
#endif

	
    Thread* main_thread() const { return threads.front().get(); }

	
    
    
    
    uint64_t nodes_searched() const;

#if STOCKFISH
	
	uint64_t               tb_hits() const;

	
    
	Thread*  get_best_thread() const;
#endif

	
    
    void start_searching();

	
    
    void wait_for_search_finished() const;


	std::vector<size_t> get_bound_thread_count_by_numa_node() const;

	
	void ensure_network_replicated();

	
	
	
	
	

	std::atomic_bool stop, abortedSearch
#if STOCKFISH
		, increaseDepth
#endif
		;

	auto cbegin() const noexcept { return threads.cbegin(); }
	auto begin() noexcept { return threads.begin(); }
	auto end() noexcept { return threads.end(); }
	auto cend() const noexcept { return threads.cend(); }
	auto size() const noexcept { return threads.size(); }
	auto empty() const noexcept { return threads.empty(); }

	
	
	
	std::vector<std::unique_ptr<Thread>> threads;

private:
	
	StateListPtr                         setupStates;

	
	std::vector<NumaIndex>               boundThreadToNumaNode;

	
	
	uint64_t accumulate(std::atomic<uint64_t> Search::Worker::* member) const {

		uint64_t sum = 0;
		for (auto&& th : threads)
			sum += (th->worker.get()->*member).load(std::memory_order_relaxed);
		return sum;
	}
};

} 

#endif 
