#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "evaluate.h"

#include "numa.h"
#include "position.h"
#include "search.h"

#include "thread.h"
#include "tt.h"
#include "usioption.h"

namespace YaneuraOu {






static int    MaxThreads = std::max(1024, 4 * int(get_hardware_concurrency()));


#if !defined(__EMSCRIPTEN__)

constexpr int MaxHashMB = Is64Bit ? 33554432 : 2048;
#else



constexpr int MaxHashMB = 2048;
#endif


namespace Book { struct BookMoveSelector; }



class IEngine
{
public:
	

	

    using InfoShort  = Search::InfoShort;
	using InfoFull   = Search::InfoFull;
	using InfoIter   = Search::InfoIteration;

#if STOCKFISH
        
        
        IEngine(std::optional<std::string> path = std::nullopt);
#else
        IEngine() {}
#endif

	
    

    IEngine(const IEngine&)            = delete;
    IEngine(IEngine&&)                 = delete;
    IEngine& operator=(const IEngine&) = delete;
    IEngine& operator=(IEngine&&)      = delete;

	
	
    virtual ~IEngine() { wait_for_search_finished(); }

	
#if STOCKFISH
    std::uint64_t perft(const std::string& fen, Depth depth, bool isChess960);
#else
    virtual std::uint64_t perft(const std::string& fen, Depth depth) = 0;
#endif

	
    
    
    virtual void go(Search::LimitsType& limits) = 0;

    
    
    
    virtual void stop() = 0;

	
    
    
	
    virtual void wait_for_search_finished() {};

	
    
    
    
    virtual std::optional<PositionSetError> set_position(const std::string& sfen, const std::vector<std::string>& moves) = 0;

    

	
    virtual void set_numa_config_from_option(const std::string& o) = 0;

	
    
    
    
    
    
    
    
    
    virtual void resize_threads() = 0;

	
    
	virtual void set_tt_size(size_t mb) = 0;

	
	
    
    
    
    virtual void set_ponderhit(bool b) = 0;

	
	
	
	virtual void search_clear() = 0;

	
    
    

    virtual void set_on_update_no_moves(std::function<void(const InfoShort&)>&&) {}
    virtual void set_on_update_full(std::function<void(const InfoFull&)>&&) {}
    virtual void set_on_iter(std::function<void(const InfoIter&)>&&) {}
    virtual void set_on_bestmove(std::function<void(std::string_view, std::string_view)>&&) {}
    virtual void set_on_verify_networks(std::function<void(std::string_view)>&&) {}

#if !STOCKFISH
    virtual void set_on_update_string(std::function<void(std::string_view)>&&) {}

	
    virtual std::function<void(std::string_view, std::string_view)> get_on_bestmove() {
        return [](auto, auto) {};
    }
#endif

    
	

	
	
    virtual void verify_networks() const {}

	
#if STOCKFISH
    
    void load_networks();

	

    void load_big_network(const std::string& file);
    void load_small_network(const std::string& file);
#endif
    
    
#if STOCKFISH
    void         save_network(const std::pair<std::optional<std::string>, std::string> files[2]);
#else
    
    virtual void save_network(const std::string& path) = 0;
#endif

    
	

    
    virtual void trace_eval() const = 0;

    
    virtual Value evaluate() const = 0;

	

    virtual const OptionsMap& get_options() const = 0;
    virtual OptionsMap&       get_options()       = 0;

	
	
	
    virtual int get_hashfull(int maxAge = 0) const = 0;

	
    virtual std::string sfen() const = 0;

	
    virtual void flip() = 0;

    
    virtual std::string visualize() const = 0;

	

	virtual std::vector<std::pair<size_t, size_t>> get_bound_thread_count_by_numa_node() const {
        return std::vector<std::pair<size_t, size_t>>();
    }
	virtual std::string get_numa_config_as_string() const { return ""; }
    virtual std::string numa_config_information_as_string() const { return ""; }
    virtual std::string thread_allocation_information_as_string() const { return ""; }
    virtual std::string thread_binding_information_as_string() const { return ""; }

#if !STOCKFISH
    
    
    
    virtual bool qsearch_psv(const std::string& inputPath,
                             const std::string& outputPath,
                             size_t             workerCount,
                             std::string&       message) {
        message = "qsearch_psv is not supported by this engine.";
        return false;
    }

    
    virtual void cyf_resource_sync(bool game_end) {}
#endif

#if STOCKFISH
   private:
    const std::string binaryDirectory;
#endif

	
	
	
	
	
	virtual void add_options() = 0;

	

#if STOCKFISH
	

    OptionsMap                                         options;
    ThreadPool                                         threads;
    TranspositionTable                                 tt;
    LazyNumaReplicatedSystemWide<Eval::NNUE::Networks> networks;

    Search::SearchManager::UpdateContext  updateContext;
    std::function<void(std::string_view)> onVerifyNetworks;
    std::map<NumaIndex, SharedHistories>  sharedHists;
#else
	
	virtual ThreadPool& get_threads() = 0;
	virtual const ThreadPool& get_threads() const = 0;
#endif

	
	
	
	virtual Position& get_position() = 0;

	

	
	
    virtual void usi() = 0;

	
    
	virtual void isready() = 0;

	
	
	
	virtual void usinewgame() = 0;

	
	virtual void user(std::istringstream& is) = 0;

	

	virtual std::string get_engine_name() const    = 0;
    virtual std::string get_engine_author() const  = 0;
    virtual std::string get_engine_version() const = 0;
    virtual std::string get_eval_name() const      = 0;
};




class Engine: public IEngine {
   public:
    Engine();

    virtual std::uint64_t perft(const std::string& fen,
                                Depth              depth ) override;

    virtual void go(Search::LimitsType& limits) override;
    virtual void stop() override;

    virtual void wait_for_search_finished() override;
    virtual std::optional<PositionSetError> set_position(const std::string&              sfen,
                                                         const std::vector<std::string>& moves) override;

    virtual void set_numa_config_from_option(const std::string& o) override;
    virtual void resize_threads() override;
    virtual void set_tt_size(size_t mb) override;
    virtual void set_ponderhit(bool b) override;
    virtual void search_clear() override;

    virtual void set_on_update_no_moves(std::function<void(const InfoShort&)>&&) override final;
    virtual void set_on_update_full(std::function<void(const InfoFull&)>&&) override final;
    virtual void set_on_iter(std::function<void(const InfoIter&)>&&) override final;
    virtual void
    set_on_bestmove(std::function<void(std::string_view, std::string_view)>&&) override final;
    virtual void set_on_verify_networks(std::function<void(std::string_view)>&&) override;
    virtual void set_on_update_string(std::function<void(std::string_view)>&&) override final;
    virtual std::function<void(std::string_view, std::string_view)>
    get_on_bestmove() override final;

    virtual void verify_networks() const override {}
    virtual void save_network(const std::string& path) override {}

    virtual void              trace_eval() const override;
    virtual Value             evaluate() const override;
    virtual const OptionsMap& get_options() const override;
    virtual OptionsMap&       get_options() override;

	virtual int get_hashfull(int maxAge = 0) const override;

    virtual std::string sfen() const override;
    virtual void        flip() override;
    virtual std::string visualize() const override;
	virtual std::vector<std::pair<size_t, size_t>> get_bound_thread_count_by_numa_node() const override;
    virtual std::string get_numa_config_as_string() const override;
    virtual std::string numa_config_information_as_string() const override;
    virtual std::string thread_allocation_information_as_string() const override;
    virtual std::string thread_binding_information_as_string() const override;

#if !STOCKFISH
    
    
    virtual bool qsearch_psv(const std::string& inputPath,
                             const std::string& outputPath,
                             size_t             workerCount,
                             std::string&       message) override {
        message = "qsearch_psv is not supported by this engine.";
        return false;
    }

    virtual void cyf_resource_sync(bool game_end) override {}
#endif

    virtual void              add_options() override;
    virtual ThreadPool&       get_threads() override { return threads; }
    virtual const ThreadPool& get_threads() const override { return threads; }
    virtual Position&         get_position() override { return pos; }

    virtual void usi() override;
    virtual void isready() override;
    virtual void usinewgame() override {};
    virtual void user(std::istringstream& is) override {};

    virtual std::string get_engine_name() const override { return "YaneuraOu"; }
    virtual std::string get_engine_author() const override { return "yaneurao"; }
    virtual std::string get_engine_version() const override { return ENGINE_VERSION; }
    virtual std::string get_eval_name() const override { return EVAL_TYPE_NAME; }

	
	
	void add_base_options();

	
    bool usi_ponder        = false;

	
	bool stochastic_ponder = false;

#if STOCKFISH    
   protected:
#endif   
    

    
    

    
    NumaReplicationContext numaContext;

    
    
    Position pos;

    
    StateListPtr states;

    
    OptionsMap options;

    
    ThreadPool threads;

	
	
	
    TranspositionTable tt;

    
    

    
    Search::UpdateContext updateContext;

    
    std::function<void(std::string_view)> onVerifyNetworks;

    std::map<NumaIndex, SharedHistories>  sharedHists;

    

    
    
    
    
    
    void run_heavy_job(std::function<void()> job);

	

	
	std::string game_root_sfen;

	
	std::vector<Move> moves_from_game_root;
};




class EngineWrapper: public IEngine {
   public:
    
    void set_engine(IEngine& _engine) { engine = &_engine; }

    
    

    virtual std::uint64_t perft(const std::string& fen,
                                Depth              depth ) override {
        return engine->perft(fen, depth);
    }

    virtual void go(Search::LimitsType& limits) override { engine->go(limits); }
    virtual void stop() override { engine->stop(); }

    virtual void wait_for_search_finished() override { engine->wait_for_search_finished(); }
    virtual std::optional<PositionSetError> set_position(const std::string&              sfen,
                                                         const std::vector<std::string>& moves) override {
        return engine->set_position(sfen, moves);
    }

    virtual void set_numa_config_from_option(const std::string& o) override { engine->set_numa_config_from_option(o); }
    virtual void resize_threads() override { engine->resize_threads(); }
    virtual void set_tt_size(size_t mb) override { engine->set_tt_size(mb); }
    virtual void set_ponderhit(bool b) override { engine->set_ponderhit(b); }
    virtual void search_clear() override { engine->search_clear(); }

    virtual void set_on_update_no_moves(std::function<void(const InfoShort&)>&& f) override final {
        engine->set_on_update_no_moves(std::move(f));
    }
    virtual void set_on_update_full(std::function<void(const InfoFull&)>&& f) override final {
        engine->set_on_update_full(std::move(f));
    }
    virtual void set_on_iter(std::function<void(const InfoIter&)>&& f) override final {
        engine->set_on_iter(std::move(f));
    }
    virtual void
    set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) override final {
        engine->set_on_bestmove(std::move(f));
    }
    virtual void set_on_verify_networks(std::function<void(std::string_view)>&& f) override {
        engine->set_on_verify_networks(std::move(f));
    }
    virtual void set_on_update_string(std::function<void(std::string_view)>&& f) override final {
        engine->set_on_update_string(std::move(f));
    }
    virtual std::function<void(std::string_view, std::string_view)> get_on_bestmove() override {
        return engine->get_on_bestmove();
    }

    virtual void verify_networks() const override { engine->verify_networks(); }
    virtual void save_network(const std::string& path) override { engine->save_network(path); }

    virtual void trace_eval() const override { engine->trace_eval(); }
    virtual Value evaluate() const override { return engine->evaluate(); }

    virtual const OptionsMap& get_options() const override { return engine->get_options(); }
    virtual OptionsMap&       get_options() override { return engine->get_options(); }

    virtual int get_hashfull(int maxAge = 0) const override { return engine->get_hashfull(maxAge); }

    virtual std::string sfen() const override { return engine->sfen(); }
    virtual void        flip() override { return engine->flip(); }
    virtual std::string visualize() const override { return engine->visualize(); }

    virtual std::vector<std::pair<size_t, size_t>>
    get_bound_thread_count_by_numa_node() const override {
        return engine->get_bound_thread_count_by_numa_node();
    }
    virtual std::string get_numa_config_as_string() const override {
        return engine->get_numa_config_as_string();
    }
    virtual std::string numa_config_information_as_string() const override {
        return engine->numa_config_information_as_string();
    }
    virtual std::string thread_allocation_information_as_string() const override {
        return engine->thread_allocation_information_as_string();
    }
    virtual std::string thread_binding_information_as_string() const override {
        return engine->thread_binding_information_as_string();
    }

#if !STOCKFISH
    virtual bool qsearch_psv(const std::string& inputPath,
                             const std::string& outputPath,
                             size_t             workerCount,
                             std::string&       message) override {
        return engine->qsearch_psv(inputPath, outputPath, workerCount, message);
    }

    virtual void cyf_resource_sync(bool game_end) override { engine->cyf_resource_sync(game_end); }
#endif

    virtual void              add_options() override { return engine->add_options(); }
    virtual ThreadPool&       get_threads() override { return engine->get_threads(); }
    virtual const ThreadPool& get_threads() const override { return engine->get_threads(); }
    virtual Position&         get_position() override { return engine->get_position(); }

    virtual void usi() override { engine->usi(); }
    virtual void isready() override { engine->isready(); }
    virtual void usinewgame() override { engine->usinewgame(); }
    virtual void user(std::istringstream& is) override { engine->user(is); }

    virtual std::string get_engine_name() const override { return engine->get_engine_name(); }
    virtual std::string get_engine_author() const override { return engine->get_engine_author(); }
    virtual std::string get_engine_version() const override { return engine->get_engine_version(); }
    virtual std::string get_eval_name() const override { return engine->get_eval_name(); }

   private:
    IEngine* engine;
};






struct EngineFuncRegister {
	EngineFuncRegister(std::function<void()> f, const std::string& engine_name, int priority);
};

} 

#endif 
