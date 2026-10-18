#include <atomic>

#include "engine.h"
#include "thread.h"
#include "perft.h"
#include "usioption.h"
#include "book/book.h"
#include "search.h"

namespace YaneuraOu {



constexpr NumaAutoPolicy DefaultNumaPolicy = BundledL3Policy{32};

Engine::Engine() :
	numaContext(NumaConfig::from_system(DefaultNumaPolicy)),
	states(new std::deque<StateInfo>(1)),
	threads()
{

#if !defined(USE_CLASSIC_EVAL)
	pos.set(StartSFEN, &states->back());
#endif


}

void Engine::usi()
{
#if STOCKFISH
    sync_cout << "id name " << engine_info(true) << "\n" << engine.get_options() << sync_endl;
    sync_cout << "uciok" << sync_endl;
#else
    sync_cout << "id name "
              << engine_info(get_engine_name(), get_engine_author(), get_engine_version(),
                             get_eval_name())
              << get_options() << sync_endl;

    sync_cout << "usiok" << sync_endl;
#endif
}

void Engine::add_base_options() {


    options.add(  
      "NumaPolicy", Option("auto", [this](const Option& o) {
          set_numa_config_from_option(o);
          return numa_config_information_as_string() + "\n"
               + thread_allocation_information_as_string();
      }));

    options.add(  
      "USI_Ponder", Option(false, [this](const Option& o) {
          usi_ponder = o;
          return std::nullopt;
      }));

	options.add(  
      "Stochastic_Ponder", Option(true, [this](const Option& o) {   
          stochastic_ponder = o;
		return std::nullopt;
	}));


    options.add(  
      "DepthLimit", Option(0, 0, int_max));

    options.add(  
      "NodesLimit", Option(0, 0, int64_max));

    options.add(  
      "DebugLogFile", Option("", [](const Option& o) {
          start_logger(o);
          return std::nullopt;
      }));
}

void Engine::add_options() {


    options.add(  
      "Threads", Option(2, 1, MaxThreads, [this](const Option&) {
          resize_threads();
          return thread_allocation_information_as_string();
      }));

    add_base_options();

#if STOCKFISH
    options.add(  
      "Clear Hash", Option([this](const Option&) {
          search_clear();
          return std::nullopt;
      }));
#endif

}

void Engine::set_numa_config_from_option(const std::string& o) {
	if (o == "auto" || o == "system")
	{
		numaContext.set_numa_config(NumaConfig::from_system(DefaultNumaPolicy));
	}
	else if (o == "hardware")
	{
		numaContext.set_numa_config(NumaConfig::from_system(DefaultNumaPolicy, false));
	}
	else if (o == "none")
	{
		numaContext.set_numa_config(NumaConfig{});
	}
	else
	{
		numaContext.set_numa_config(NumaConfig::from_string(o));
	}

	resize_threads();
	threads.ensure_network_replicated();
}


void Engine::wait_for_search_finished() {
#if !STOCKFISH
    if (!threads.size())
        return;
#endif

	threads.main_thread()->wait_for_search_finished();
}

std::optional<PositionSetError> Engine::set_position(const std::string&              sfen,
                                                     const std::vector<std::string>& moves) {


	states = StateListPtr(new std::deque<StateInfo>(1));
	auto err = pos.set(sfen  , &states->back());
	if (err.has_value())
		return err;

#if !STOCKFISH
    std::vector<Move> moves0;
#endif

	for (const auto& move : moves)
	{
		auto m = USIEngine::to_move(pos, move);

		if (m == Move::none())
			return PositionSetError("Illegal move: " + move);

		states->emplace_back();
		if (m == Move::null())
			pos.do_null_move(states->back());
		else
			pos.do_move(m, states->back());

#if !STOCKFISH
		moves0.emplace_back(m);
#endif
	}

#if !STOCKFISH
    game_root_sfen = sfen;
	moves_from_game_root = std::move(moves0);
#endif

	return std::nullopt;
}


#if 0
void Engine::usinewgame()
{
	wait_for_search_finished();

	threads.clear();

}
#endif

void Engine::isready()
{
	resize_threads();

	sync_cout << "readyok" << sync_endl;
}

std::uint64_t Engine::perft(const std::string& fen, Depth depth ) {
	verify_networks();

	return Benchmark::perft(fen, depth );
}


void Engine::go(Search::LimitsType& limits) {
	ASSERT_LV3(limits.perft == 0);

	threads.start_thinking(options, pos, states, limits);
}

void Engine::stop() { threads.stop = true; }

void Engine::search_clear() {
#if STOCKFISH
    wait_for_search_finished();

    tt.clear(threads);
    threads.clear();

    Tablebases::init(options["SyzygyPath"]);  
#else
    wait_for_search_finished();
    isready();
#endif
}

void Engine::set_on_update_no_moves(std::function<void(const Engine::InfoShort&)>&& f) {
    updateContext.onUpdateNoMoves = std::move(f);
}

void Engine::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    updateContext.onUpdateFull = std::move(f);
}

void Engine::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    updateContext.onIter = std::move(f);
}

void Engine::set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) {
    updateContext.onBestmove = std::move(f);
}

void Engine::set_on_verify_networks(std::function<void(std::string_view)>&& f) {
}

#if !STOCKFISH
void Engine::set_on_update_string(std::function<void(std::string_view)>&& f) {
    updateContext.onUpdateString = std::move(f);
}

std::function<void(std::string_view, std::string_view)> Engine::get_on_bestmove() {
    return updateContext.onBestmove;
}
#endif

void Engine::resize_threads() {

	wait_for_search_finished();   


#if STOCKFISH
	threads.set(numaContext.get_numa_config(), { options, threads, tt, networks }, updateContext);
#else


	if (!options.count("Threads"))
        return;

	auto worker_factory = [&](Search::SharedState& sharedState, const Search::ThreadIds& ids)
		{ return make_unique_large_page<Search::Worker>(sharedState, ids); };

    threads.set(numaContext.get_numa_config(),
                {options, threads, tt, sharedHists  }, 
				updateContext,
                options["Threads"],
				worker_factory);
#endif


#if STOCKFISH
	set_tt_size(options["Hash"]);
#endif


	threads.ensure_network_replicated();
}

void Engine::set_tt_size(size_t mb) {
#if STOCKFISH
	wait_for_search_finished();
    tt.resize(mb, threads);
#endif
}

void Engine::set_ponderhit(bool b) {
#if STOCKFISH
	threads.main_manager()->ponder = b;
#endif
}






void Engine::trace_eval() const {
#if STOCKFISH
	StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;

	p.set(pos.fen(), options["UCI_Chess960"], &trace_states->back());

    verify_networks();
	sync_cout << "\n" << Eval::trace(p, *networks) << sync_endl;
#endif
}

#if !STOCKFISH
Value Engine::evaluate() const { return VALUE_NONE; }
#endif

const OptionsMap& Engine::get_options() const { return options; }
OptionsMap&       Engine::get_options() { return options; }

#if STOCKFISH
std::string Engine::fen() const { return pos.fen(); }
#else
std::string Engine::sfen() const { return pos.sfen(); }
#endif

void Engine::flip() { pos.flip(); }

std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

#if STOCKFISH
int Engine::get_hashfull(int maxAge) const { return tt.hashfull(maxAge); }
#else
int Engine::get_hashfull(int maxAge) const { return 0; }
#endif

std::vector<std::pair<size_t, size_t>> Engine::get_bound_thread_count_by_numa_node() const {
	auto                                   counts = threads.get_bound_thread_count_by_numa_node();
	const NumaConfig& cfg = numaContext.get_numa_config();
	std::vector<std::pair<size_t, size_t>> ratios;
	NumaIndex                              n = 0;
	for (; n < counts.size(); ++n)
		ratios.emplace_back(counts[n], cfg.num_cpus_in_numa_node(n));
	if (!counts.empty())
		for (; n < cfg.num_numa_nodes(); ++n)
			ratios.emplace_back(0, cfg.num_cpus_in_numa_node(n));
	return ratios;
}

std::string Engine::get_numa_config_as_string() const {
	return numaContext.get_numa_config().to_string();
}

std::string Engine::numa_config_information_as_string() const {
	auto cfgStr = get_numa_config_as_string();
	return "Available processors: " + cfgStr;
}

std::string Engine::thread_binding_information_as_string() const {
	auto              boundThreadsByNode = get_bound_thread_count_by_numa_node();
	std::stringstream ss;
	if (boundThreadsByNode.empty())
		return ss.str();

	bool isFirst = true;

	for (auto&& [current, total] : boundThreadsByNode)
	{
		if (!isFirst)
			ss << ":";
		ss << current << "/" << total;
		isFirst = false;
	}

	return ss.str();
}

std::string Engine::thread_allocation_information_as_string() const {
	std::stringstream ss;

	size_t threadsSize = threads.size();
	ss << "Using " << threadsSize << (threadsSize > 1 ? " threads" : " thread");

	auto boundThreadsByNodeStr = thread_binding_information_as_string();
	if (boundThreadsByNodeStr.empty())
		return ss.str();

	ss << " with NUMA node thread binding: ";
	ss << boundThreadsByNodeStr;

	return ss.str();
}


void Engine::run_heavy_job(std::function<void()> job) {



    std::atomic_bool thread_started{ false };

    std::atomic_bool thread_end{ false };

    auto th = std::thread([&] {
        thread_started.store(true, std::memory_order_release);

        int count = 0;
        while (!thread_end.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (++count >= 50 )
            {
                count = 0;
                sync_cout << sync_endl;  

            }
        }
    });
    SCOPE_EXIT({
        thread_end.store(true, std::memory_order_release);
        th.join();
    });

    while (!thread_started.load(std::memory_order_acquire))
        Tools::sleep(100);


    job();
}


using EngineEntry = std::tuple<std::function<void()>, std::string, int>;

static std::vector<EngineEntry>& engineFuncs() {
	static std::vector<EngineEntry> funcs;
	return funcs;
}

EngineFuncRegister::EngineFuncRegister(std::function<void()> f, const std::string& engine_name, int priority)
{
	engineFuncs().push_back({ f , engine_name, priority });
}

void run_engine_entry()
{
	auto& v = engineFuncs();
	EngineEntry* m = nullptr;
	for (auto& entry : v)
	{
		if (!m || std::get<2>(*m) < std::get<2>(entry))
		{
			m = &entry;
		}
	}

	if (m == nullptr) {
		sync_cout << "Error: no engine entry point." << sync_endl;
		Tools::exit();
	}
	else {
		std::get<0>(*m)(); 
	}
}


} 
