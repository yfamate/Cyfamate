#include "../../config.h"

#if defined(YANEURAOU_MATE_ENGINE)

#include <sstream>

#include "../../usi.h"
#include "../../search.h"
#include "../../thread.h"
#include "../../misc.h"
#include "../../mate/mate.h"

using namespace std;
namespace YaneuraOu {

using namespace Mate::Dfpn;

namespace {

MateDfpnSolver solver(DfpnSolverType::None);


vector<string> solver_list = {"32bitNodeSolver", "64bitNodeSolver"};
}

namespace Search {

class YaneuraOuMateWorker: public Worker {
   public:
    YaneuraOuMateWorker(Search::SharedState& sharedState,
					 const Search::ThreadIds& ids) :
        
        Worker(sharedState, ids) {}

    
    
    virtual void clear() override {}
	
    
    
    
    virtual void start_searching() override {
        
        
        atomic<Move> move = Move::resign();

        
        u64 nodes_limit = options["NodesLimit"];

		
        int depth_limit = int(options["DepthLimit"]);
        if (depth_limit == 0)
			
            solver.set_max_game_ply(0);
        else
			
			solver.set_max_game_ply(rootPos.game_ply() + depth_limit - 1);

        
        auto thread = std::thread([&]() { move = solver.mate_dfpn(rootPos, nodes_limit); });

        ElapsedTimer time;
        time.reset();                                    
        TimePoint lastPvOutput = 0;                      
        TimePoint pvInterval   = options["PvInterval"];  

        
        auto print_pv = [&]() {
            auto elapsed        = time.elapsed();
            u64  nodes_searched = solver.get_nodes_searched();

            
            u64 nps = nodes_searched * 1000 / elapsed;

            sync_cout << "info time " << elapsed << " nodes " << nodes_searched << " nps " << nps
                      << " hashfull " << solver.hashfull() << " pv"
                      << USIEngine::move(solver.get_current_pv()) << sync_endl;
        };


        
        
        auto time_up = [&]() { return limits.mate && time.elapsed() >= limits.mate; };

        
        while (!threads.stop && !time_up() && move.load() == Move::resign())
        {
            Tools::sleep(100);

            auto elapsed = time.elapsed();
            if (pvInterval && elapsed > lastPvOutput + pvInterval)
            {
                print_pv();
                lastPvOutput = time.elapsed();
            }
        }

        thread.join();

        
        print_pv();

        if (time_up())
        {
            sync_cout << "checkmate timeout" << sync_endl;
        }
        else if (move.load() == Move::none())
        {
            if (solver.is_out_of_memory())
                sync_cout << "info string Out Of Memory." << sync_endl;
            else if (solver.get_nodes_searched() >= nodes_limit)
                sync_cout << "info string Exceeded NodesLimit." << sync_endl;

            sync_cout << "checkmate none" << sync_endl;  
        }
        else if (move.load() == Move::null())
        {
            
            sync_cout << "checkmate nomate" << sync_endl;
        }
        else
        {
            auto pv = solver.get_pv();
            sync_cout << "checkmate" << USIEngine::move(pv) << sync_endl;
        }
    }
};

class YaneuraOuMateEngine: public Engine {
    
    virtual std::string get_engine_name() const override { return "YaneuraOuMateEngine"; }

    
    virtual void isready() override {
        
        auto solver_type = (string) options["SolverType"];
        if (solver_type == solver_list[0])
            solver.ChangeSolverType(Mate::Dfpn::DfpnSolverType::Node32bit);
        else if (solver_type == solver_list[1])
            solver.ChangeSolverType(Mate::Dfpn::DfpnSolverType::Node64bit);
        else
            solver.ChangeSolverType(Mate::Dfpn::DfpnSolverType::None);

        u64 mem = options["USI_Hash"];
        sync_cout << "info string DfPn memory allocation , USI_Hash = " << mem << " [MB]"
                  << sync_endl;
        solver.alloc(mem);

        
        Engine::isready();
    }

    
    virtual void add_options() override {
        
        Engine::add_options();

        
        options.add(  
          "USI_Hash", Option(1024, 1, MaxHashMB, [this](const Option& o) {
              
              
              return std::nullopt;
          }));

        
        
        options.add("PvInterval", Option(1000, 0, 100000));

        options.add("SolverType", Option(solver_list, solver_list[0]));

		
		
		
        
		

        
        
		
    }

    
    virtual void user(std::istringstream& is) override {
        sync_cout << "UserEngine::user_cmd" << sync_endl;
    }

    
    virtual void resize_threads() override {
        

        
        threads.wait_for_search_finished();

        

        
        
		auto worker_factory = [&](Search::SharedState& sharedState,
								  const Search::ThreadIds& ids)
		{

			auto p = make_unique_large_page<Search::YaneuraOuMateWorker>(
				
                sharedState, ids
			);

			return LargePagePtr<Worker>(p.release());  
		};

        threads.set(numaContext.get_numa_config(), {options, threads, tt, sharedHists  },
					updateContext, options["Threads"], worker_factory);

        

        
        threads.ensure_network_replicated();
    }
};
} 

namespace Eval {

void add_options(OptionsMap& options, ThreadPool& threads) {}

} 
} 

using namespace YaneuraOu;

namespace {

	
	void engine_main()
	{
		
		auto engine = std::make_unique<Search::YaneuraOuMateEngine>();

		
		auto usi = std::make_unique<USIEngine>();
		usi->set_engine(*engine);  
		usi->enqueue_startup_commands(CommandLine::g);

		
		usi->loop();
	}

	
	static EngineFuncRegister r(engine_main, "YaneuraOuMateEngine", 0);

} 

#endif 
