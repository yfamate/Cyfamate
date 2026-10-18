#include <cstdlib>
#include <sstream>
#include <queue>

#include "types.h"
#include "usi.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "misc.h"
#include "testcmd/unit_test.h"
#include "benchmark.h"
#include "engine.h"
#include "movegen.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#endif


namespace YaneuraOu {


constexpr auto BenchmarkCommand = "speedtest";

#if !STOCKFISH


namespace Test {
void test_cmd(IEngine& engine, std::istringstream& is);
}


#if defined(ENABLE_MAKEBOOK_CMD)
namespace Book {
void makebook(IEngine& engine, std::istringstream& is);
}
#endif

#endif



template<typename... Ts>
struct overload : Ts... {
	using Ts::operator()...;
};

template<typename... Ts>
overload(Ts...) -> overload<Ts...>;


void USIEngine::print_info_string(std::string_view str) {
	sync_cout_start();
	for (auto& line : split(str, "\n"))
	{
		if (!is_whitespace(line))
		{
			std::cout << "info string " << line << '\n';
		}
	}
	sync_cout_end();
}

#if STOCKFISH
UCIEngine::UCIEngine(int argc, char** argv) :
    engine(argv[0]),
    
    cli(argc, argv) {
    

    engine.get_options().add_info_listener([](const std::optional<std::string>& str) {
        if (str.has_value())
            print_info_string(*str);
    });

    
    

    
    
    
    
}
#else

void USIEngine::set_engine(IEngine& _engine) {
    engine.set_engine(_engine);

    
    
    engine.get_options().read_engine_option_profile("engine_option_profile.txt");

    
    
    engine.add_options();

    
    
    

#if defined(USE_CLASSIC_EVAL)
    Eval::add_options(engine.get_options(), engine.get_threads());
#endif

	
    engine.get_options().add_info_listener([](const std::optional<std::string>& str) {
        if (str.has_value())
            print_info_string(*str);
    });

    
    
    
    init_search_update_listeners();
}

#endif


void USIEngine::init_search_update_listeners() {
    engine.set_on_iter([](const auto& i) { on_iter(i); });
    engine.set_on_update_no_moves([](const auto& i) { on_update_no_moves(i); });
    engine.set_on_update_full(
      [this](const auto& i) { on_update_full(i ); });
    engine.set_on_bestmove([](const auto& bm, const auto& p) { on_bestmove(bm, p); });
    engine.set_on_update_string([](const auto& i) { on_update_string(i); });
    engine.set_on_verify_networks([](const auto& s) { print_info_string(s); });
}


void USIEngine::loop()
{
    
    

#if !defined(__EMSCRIPTEN__)

	
	while (true)
	{
		
		std::string cmd = std_input.input();

		
		if (usi_cmdexec(cmd))
			break;
	}

	
	engine.wait_for_search_finished();

#else
	
	
#endif
}



bool USIEngine::usi_cmdexec(const std::string& cmd) {

#if STOCKFISH
    string token, cmd;
    for (int i = 1; i < cli.argc; ++i)
        cmd += std::string(cli.argv[i]) + " ";

    do
    {

        if (cli.argc == 1 && !getline(std::cin, cmd))  
                                                       
            cmd = "quit";

        token.clear();  
                        
#else
    std::string token;
#endif

    std::istringstream is(cmd);
    is >> std::skipws >> token;

#if STOCKFISH
    if (token == "quit" || token == "stop")
#else
    if (token == "quit" || token == "stop" || token == "gameover")
    
#endif
        
        { Cyfamate::zyf_cancelled().store(true); engine.stop();
#if !STOCKFISH
          sp_ponder_pending = false;
          open_infinite = open_ponder = false;
#endif
          
          if (token == "quit") { bool printed = false; Cyfamate::auto_service().shutdown(&printed); } }

    
    
    
    

    
    
    
    

    
    else if (token == "cyfhint")
        CyfRootHint::receive(is);

    else if (token == "ponderhit")
#if STOCKFISH
        engine.set_ponderhit(false);
#else
    {
        
        if (engine.get_options().count("Stochastic_Ponder")
            && engine.get_options()["Stochastic_Ponder"])
        {
          
          if (!sp_ponder_pending)
            sync_cout << "info string ponderhit ignored: no pending go ponder" << sync_endl;
          else {
            sp_ponder_pending = false;
            const auto _sp_t0 = std::chrono::steady_clock::now();   
            {   const int64_t _gp = Cyfamate::sp_goponder_ms().load(), _now = (int64_t)now();
                Cyfamate::sp_credit_ms().store(_gp > 0 && _now > _gp ? (_now - _gp) * Cyfamate::options().sp_credit / 100 : 0);
                if (Cyfamate::options().sp_credit > 0) sync_cout << "info string sp hit gp=" << _gp << " now=" << _now << " credit_ms=" << Cyfamate::sp_credit_ms().load() << sync_endl; }
			
            auto on_bestmove = engine.get_on_bestmove();
            engine.set_on_bestmove([](auto,auto) {});
            Cyfamate::bestmove_muted() = true;
	        engine.stop();
            engine.wait_for_search_finished();
            Cyfamate::bestmove_muted() = false;
            engine.set_on_bestmove(std::move(on_bestmove));

			

            std::istringstream iss1(last_position_cmd_string);
            iss1 >> token; 
            position(iss1);

            
            
            
            const int64_t _sp_wait = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _sp_t0).count();
            std::string _sp_go = Cyfamate::sp_go_with_ponderhit_clock(last_go_cmd_string, is);
            {
                bool _black = true;  
                std::istringstream ps(last_position_cmd_string); std::string t; std::vector<std::string> tk;
                while (ps >> t) tk.push_back(t);
                size_t mi = 0; for (; mi < tk.size(); ++mi) if (tk[mi] == "moves") break;
                size_t nm = mi < tk.size() ? tk.size() - mi - 1 : 0;
                if (tk.size() > 1 && tk[1] == "sfen" && tk.size() > 3) _black = (tk[3] == "b");
                if (nm % 2) _black = !_black;
                const std::string key = _black ? "btime" : "wtime";
                std::istringstream gs(_sp_go); std::vector<std::string> g; while (gs >> t) g.push_back(t);
                std::string out;
                for (size_t i = 0; i < g.size(); ++i) {
                    if (g[i] == key && i + 1 < g.size()) {
                        long long v = 0; try { v = std::stoll(g[i + 1]); } catch (...) { v = -1; }
                        if (v >= 0) { out += (out.empty() ? "" : " ") + g[i] + " " + std::to_string(std::max<long long>(0, v - _sp_wait)); ++i; continue; }
                    }
                    out += (out.empty() ? "" : " ") + g[i];
                }
                if (_sp_wait > 50) sync_cout << "info string sp ponderhit wait_ms=" << _sp_wait << " charged to " << key << sync_endl;
                _sp_go = out;
            }
            std::istringstream iss_go(_sp_go);
            std::string replay_go;
            iss_go >> token;
            while (iss_go >> token)
                if (token != "ponder") replay_go += (replay_go.empty() ? "" : " ") + token;
            std::istringstream iss2(replay_go);
            Cyfamate::replaying_go() = true;    
            go(iss2);
            Cyfamate::replaying_go() = false;
          }
		}
		else {
            
            const int64_t _ph = Cyfamate::zyf_ponderhit_ms().load();
            Cyfamate::zyf_action_deadline().store(_ph > 0 ? CyfZyf::now_us() + _ph * 1000 : 0);
	        engine.set_ponderhit(false);
            open_ponder = false;
        }
    }
#endif

    
    else if (token == "usi")
#if STOCKFISH
    {
        sync_cout << "id name " << engine_info(true) << "\n"
                    << engine.get_options() << sync_endl;

        sync_cout << "uciok" << sync_endl;
    }
#else
        engine.usi();
#endif

    
    else if (token == "setoption")
#if !STOCKFISH
    {
        
        
        if (open_infinite || open_ponder)
            sync_cout << "info string setoption ignored: search in progress" << sync_endl;
        else
            setoption(is);
    }
#else
        setoption(is);
#endif

    else if (token == "go")
    {
#if !STOCKFISH
        
        if (open_infinite || open_ponder)
        {
            sync_cout << "info string go ignored: search in progress" << sync_endl;
            return false;   
        }
#endif
        
        

#if STOCKFISH
        print_info_string(engine.numa_config_information_as_string());
        print_info_string(engine.thread_allocation_information_as_string());
        
#else
		
        last_go_cmd_string = cmd;
        Cyfamate::sp_goponder_ms().store(cmd.find(" ponder") != std::string::npos ? (int64_t)now() : 0);
#endif
        go(is);
    }

    else if (token == "position")
#if STOCKFISH
        position(is);
#else
    {
		
        last_position_cmd_string = cmd;
        position(is);
    }
#endif

#if STOCKFISH
    else if (token == "ucinewgame")
        engine.search_clear();
#else
    else if (token == "usinewgame"){
        engine.cyf_resource_sync(true);   
        engine.usinewgame();
        Cyfamate::auto_service().invalidate();   
    }
#endif

    
    else if (token == "isready")
#if STOCKFISH
        sync_cout << "readyok" << sync_endl;
#else
    {
        if (open_infinite || open_ponder)
            sync_cout << "readyok" << sync_endl;   
        else
            isready();
    }
#endif

    
    

    
    

    
    else if (token == "flip")
        engine.flip();

    
    else if (token == "bench")
        bench(is);

    else if (token == BenchmarkCommand)
        benchmark(is);

    
    else if (token == "d")
        sync_cout << engine.visualize() << sync_endl;

    
    else if (token == "eval")
        engine.trace_eval();

    
    else if (token == "compiler")
        sync_cout << compiler_info() << sync_endl;

    
    
    else if (token == "export_net")
    {
#if STOCKFISH
        std::pair<std::optional<std::string>, std::string> files[2];

        if (is >> std::skipws >> files[0].second)
            files[0].first = files[0].second;

        if (is >> std::skipws >> files[1].second)
            files[1].first = files[1].second;
#else
    std::string file;
    is >> std::skipws >> file;
    engine.save_network(file);
#endif
    }

#if STOCKFISH
    else if (token == "--help" || token == "help" || token == "--license" || token == "license")
        sync_cout << "\nStockfish is a powerful chess engine for playing and analyzing."
                        "\nIt is released as free software licensed under the GNU GPLv3 License."
                        "\nStockfish is normally used with a graphical user interface (GUI) and implements"
                        "\nthe Universal Chess Interface (UCI) protocol to communicate with a GUI, an API, etc."
                        "\nFor any further information, visit https://github.com/official-stockfish/Stockfish#readme"
                        "\nor read the corresponding README.md and LICENSE files distributed along with this program.\n"
                    << sync_endl;

    else if (!token.empty() && token[0] != '#')
        sync_cout << "Unknown command: '" << cmd << "'. Type help for more information." << sync_endl;
#else

	
    
    

    
    else if (token == "f")
        enqueue_command_from_file(is);

	
	else if (token == "e")
        sync_cout << "eval = " << engine.evaluate() << sync_endl;
#if defined(CYF_BLEND)
    
    else if (token == "cyfblend")
        Cyfamate::blend_debug(engine.get_position());
    
    else if (token == "cyfdlcache")
        Cyfamate::blend_dl_debug(is);
#endif

    
    else if (token == "moves")
        moves();

    
    else if (token == "getoption")
        getoption(is);

    
    else if (token == "unittest")
        unittest(is);

    
    else if (token == "config")
        sync_cout << config_info() << sync_endl;

    
    else if (token == "qsearch_psv")
        qsearch_psv(is);

#if defined(ENABLE_MAKEBOOK_CMD)
	
	else if (token == "makebook")
        Book::makebook(engine, is);
#endif

    
    else if (token == "matsuri")
    {
        std::istringstream iss("l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w GR5pnsg 1");

		
        position(iss);

        sync_cout << "The matsuri position has been set." << sync_endl;
    }

    
    
    
    else if (token == "log")
        start_logger("io_log.txt");

#if defined(ENABLE_TEST_CMD)
    
    else if (token == "test")
        Test::test_cmd(engine, is);
#endif

    
    else if (token == "user")
        engine.user(is);


    
    else
    {
        

        if (!token.empty())
        {
            std::string value;
            is >> value;
            if (value == "=") 
                is >> value;

			
			
			
			

			auto ss = engine.get_options().set_option_if_exists(token, value);
            sync_cout << ss << sync_endl;
        }
    }
#endif


#if STOCKFISH
    } while (token != "quit" && cli.argc == 1);  
#else
    
    return token == "quit";
#endif
}



Search::LimitsType USIEngine::parse_limits(std::istream& is) {

#if STOCKFISH
#else
    const auto& options = engine.get_options();
#endif

	Search::LimitsType limits;
	std::string        token;

	
	
	

    limits.startTime = now();  
    if (Cyfamate::replaying_go()) {   
        const int64_t _c = Cyfamate::sp_credit_ms().exchange(0);
        if (_c > 0) { limits.startTime -= (TimePoint)_c; sync_cout << "info string sp credit_ms=" << _c << sync_endl; }
    }
							   

    
    

    limits.depth = options.count("DepthLimit") ? (int) options["DepthLimit"] : 0;
    limits.nodes = options.count("NodesLimit") ? (u64) options["NodesLimit"] : 0;

	while (is >> token)

		

		if (token == "searchmoves")  
			                         
		{
			
			while (is >> token)
			{
#if STOCKFISH
				limits.searchmoves.push_back(to_lower(token));
#else
				
				
				limits.searchmoves.push_back(token);
#endif
			}
		}

		
        else if (token == "wtime")
			is >> limits.time[WHITE];
		else if (token == "btime")
			is >> limits.time[BLACK];

		
        else if (token == "winc")
			is >> limits.inc[WHITE];
		else if (token == "binc")
			is >> limits.inc[BLACK];

#if STOCKFISH
		
		
		else if (token == "movestogo")
			is >> limits.movestogo;
#else
        
        
        else if (token == "byoyomi")
        {
            TimePoint t = 0;
            is >> t;

            
            

            

            
            limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = t;
        }

        
        
		
		else if (token == "rtime")
            is >> limits.rtime;

#endif
        
        else if (token == "depth")
			is >> limits.depth;

		
		else if (token == "nodes")
			is >> limits.nodes;

		
		else if (token == "movetime")
			is >> limits.movetime;

        
		
        
#if STOCKFISH
		else if (token == "mate")
			is >> limits.mate;
#else
        else if (token == "mate")
        {
            token.clear();
            is >> token;
			
			
            if (token == "infinite" || token == "")
                limits.mate = INT32_MAX;
            else
                
				
                limits.mate = stoi(token);
        }
#endif

		
		else if (token == "perft")
			is >> limits.perft;

		
		else if (token == "infinite")
			limits.infinite = 1;

		
        else if (token == "ponder")
			limits.ponderMode = true;

	return limits;
}











void USIEngine::go(std::istringstream& is)
{
    engine.wait_for_search_finished();
    Cyfamate::zyf_cancelled().store(false);
    Search::LimitsType limits = parse_limits(is);
#if !STOCKFISH
    open_infinite = !limits.perft && limits.infinite;
    open_ponder = !limits.perft && limits.ponderMode;
    sp_ponder_pending = limits.ponderMode && engine.get_options().count("Stochastic_Ponder")
                        && bool(engine.get_options()["Stochastic_Ponder"]);
#endif
#if !STOCKFISH
    limits.cyfGoGen = Cyfamate::replaying_go() ? Cyfamate::go_received().load()
                                               : ++Cyfamate::go_received();   
#endif

	if (limits.perft)
		perft(limits);
	else
#if STOCKFISH
		engine.go(limits);
#else
	{
		
		
		
		
		
		auto sp_has_move = [](const std::string& s) {
			const auto m = s.find(" moves ");
			return m != std::string::npos && s.find_first_not_of(' ', m + 7) != std::string::npos;
		};
		if (limits.ponderMode && engine.get_options().count("Stochastic_Ponder")
			&& engine.get_options()["Stochastic_Ponder"] && sp_has_move(last_position_cmd_string))
		{
            auto s = last_position_cmd_string;
			

			
			std::size_t last_space  = s.find_last_of(' ');
			std::istringstream iss(s.substr(0,last_space));
			
            std::string             token;
            iss >> token;

            position(iss);
		}

		
		
		
		if (Cyfamate::parent_info_enabled())
		{
			engine.wait_for_search_finished();
			sync_cout << "info string cyfgo v=1 gen=" << limits.cyfGoGen << sync_endl;
		}
		engine.go(limits);
	}
#endif
}


void USIEngine::bench(std::istream& args) {

    
    
    Cyfamate::bench_guard() = true;
    Cyfamate::mate_table().disable(); Cyfamate::mate_worker().stop_join();   
    struct BenchGuardOff { ~BenchGuardOff(){ Cyfamate::bench_guard() = false; } } _cyf_bench_guard;

    std::string token;
    uint64_t    num, nodes = 0, cnt = 1;
    uint64_t    nodesSearched = 0;
    const auto& options       = engine.get_options();

    engine.set_on_update_full([&](const auto& i) {
        nodesSearched = i.nodes;
#if STOCKFISH
        on_update_full(i, options["UCI_ShowWDL"]);
#else
        on_update_full(i);
#endif
    });

#if !STOCKFISH
	
    isready();
#endif

    std::vector<std::string> list = Benchmark::setup_bench(engine.sfen(), args);

    num = count_if(list.begin(), list.end(), [](const std::string& s) { return s.find("go ") == 0 || s.find("eval") == 0; });

    TimePoint elapsed = now();

    for (const auto& cmd : list)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go" || token == "eval")
        {
            std::cerr << "\nPosition: " << cnt++ << '/' << num << " (" << engine.sfen() << ")" << std::endl;
            if (token == "go")
            {
                Search::LimitsType limits = parse_limits(is);

#if !STOCKFISH
                
                limits.disablePvInterval = true;
#endif

                if (limits.perft)
                    nodesSearched = perft(limits);
                else
                {
                    engine.go(limits);
                    engine.wait_for_search_finished();
                }

                nodes += nodesSearched;
                nodesSearched = 0;
            }
            else
                engine.trace_eval();
        }
        else if (token == "setoption")
            setoption(is);
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
						
						
        {
			engine.search_clear();  
                                    

			elapsed = now();
        }
    }

    elapsed = now() - elapsed + 1;  
                                    

    dbg_print();

    std::cerr << "\n==========================="    
              << "\nTotal time (ms) : " << elapsed  
              << "\nNodes searched  : " << nodes    
              << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;

    
    

#if STOCKFISH
    engine.set_on_update_full([&](const auto& i) { on_update_full(i, options["UCI_ShowWDL"]); });
#else
    engine.set_on_update_full([&](const auto& i) { on_update_full(i); });
#endif
}

void USIEngine::benchmark(std::istream& args) {

	
	
	Cyfamate::bench_guard() = true;
	Cyfamate::mate_table().disable(); Cyfamate::mate_worker().stop_join();   
	struct BenchGuardOff { ~BenchGuardOff(){ Cyfamate::bench_guard() = false; } } _cyf_bench_guard;

	
    
    static constexpr int NUM_WARMUP_POSITIONS = 3;

    std::string token;
    uint64_t    nodes = 0, cnt = 1;
    uint64_t    nodesSearched = 0;

    engine.set_on_update_full([&](const Engine::InfoFull& i) { nodesSearched = i.nodes; });

    engine.set_on_iter([](const auto&) {});
    engine.set_on_update_no_moves([](const auto&) {});
    engine.set_on_bestmove([](const auto&, const auto&) {});
    engine.set_on_verify_networks([](const auto&) {});

#if !STOCKFISH
	
    isready();
#endif

    Benchmark::BenchmarkSetup setup = Benchmark::setup_benchmark(args);

#if STOCKFISH
    const int numGoCommands = count_if(setup.commands.begin(), setup.commands.end(), [](const std::string& s) { return s.find("go ") == 0; });
#else
	
	const size_t numGoCommands = count_if(setup.commands.begin(), setup.commands.end(),
                                       [](const std::string& s) { return s.find("go ") == size_t(0); });
#endif

    TimePoint totalTime = 0;

    
    auto ss = std::istringstream("name Threads value " + std::to_string(setup.threads));
    setoption(ss);
#if STOCKFISH
	ss = std::istringstream("name Hash value " + std::to_string(setup.ttSize));
    setoption(ss);
    ss = std::istringstream("name UCI_Chess960 value false");
    setoption(ss);
#else
    ss = std::istringstream("name USI_Hash value " + std::to_string(setup.ttSize));
    setoption(ss);
#endif

    
    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go")
        {
            
            std::cerr << "\rWarmup position " << cnt++ << '/' << NUM_WARMUP_POSITIONS;

            Search::LimitsType limits = parse_limits(is);

            
            engine.go(limits);
            engine.wait_for_search_finished();
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  
        }

        if (cnt > NUM_WARMUP_POSITIONS)
            break;
    }

    std::cerr << "\n";

    cnt   = 1;
    nodes = 0;

    int           numHashfullReadings                    = 0;
    constexpr int hashfullAges[]                         = {0, 999};  
    int           totalHashfull[std::size(hashfullAges)] = {0};
    int           maxHashfull[std::size(hashfullAges)]   = {0};

    auto updateHashfullReadings = [&]() {
        numHashfullReadings += 1;

        for (int i = 0; i < static_cast<int>(std::size(hashfullAges)); ++i)
        {
            const int hashfull = engine.get_hashfull(hashfullAges[i]);
            maxHashfull[i]     = std::max(maxHashfull[i], hashfull);
            totalHashfull[i] += hashfull;
        }
    };

    engine.search_clear();  

    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go")
        {
            
            std::cerr << "\rPosition " << cnt++ << '/' << numGoCommands;

            Search::LimitsType limits = parse_limits(is);

            nodesSearched = 0;
            TimePoint elapsed = now();

            
            engine.go(limits);
            engine.wait_for_search_finished();

            totalTime += now() - elapsed;

            updateHashfullReadings();

            nodes += nodesSearched;
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  
        }
    }

    totalTime = std::max<TimePoint>(totalTime, 1);  

    dbg_print();

    std::cerr << "\n";

    static_assert(std::size(hashfullAges) == 2 && hashfullAges[0] == 0 && hashfullAges[1] == 999,
                  "Hardcoded for display. Would complicate the code needlessly in the current state.");

    std::string threadBinding = engine.thread_binding_information_as_string();
    if (threadBinding.empty())
        threadBinding = "none";

    

    std::cerr << "==========================="
              << "\nVersion                    : "
              << engine_version_info()
              
              << compiler_info()
              << "Large pages                : " << (has_large_pages() ? "yes" : "no")
              << "\nUser invocation            : " << BenchmarkCommand << " "
              << setup.originalInvocation << "\nFilled invocation          : " << BenchmarkCommand
              << " " << setup.filledInvocation
              << "\nAvailable processors       : " << engine.get_numa_config_as_string()
              << "\nThread count               : " << setup.threads
              << "\nThread binding             : " << threadBinding
              << "\nTT size [MiB]              : " << setup.ttSize
              << "\nHash max, avg [per mille]  : "
              << "\n    single search          : " << maxHashfull[0] << ", "
              << totalHashfull[0] / numHashfullReadings
              << "\n    single game            : " << maxHashfull[1] << ", "
              << totalHashfull[1] / numHashfullReadings
              << "\nTotal nodes searched       : " << nodes
              << "\nTotal search time [s]      : " << totalTime / 1000.0
              << "\nNodes/second               : " << 1000 * nodes / totalTime << std::endl;

    

    init_search_update_listeners();

}


void USIEngine::setoption(std::istringstream& is) {
    engine.wait_for_search_finished();
    engine_options().setoption(is);
}

std::uint64_t USIEngine::perft(const Search::LimitsType& limits) {
    auto nodes = engine.perft(engine.sfen(), limits.perft );
    sync_cout << "\nNodes searched: " << nodes << "\n" << sync_endl;
    return nodes;
}


void USIEngine::position(std::istringstream& is) {
    const std::string fullCommand = is.str();

	std::string token, sfen;

    is >> token;

    if (token == "startpos")
    {
        
        sfen = StartSFEN;
        is >> token;  
					  
    }
    
    
#if STOCKFISH
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;
#else
    
    
    else
    {
        
        if (token != "sfen")
            sfen += token + " ";

        while (is >> token && token != "moves")
            sfen += token + " ";
    }
#endif

    std::vector<std::string> moves;

	
    while (is >> token)
    {
        moves.push_back(token);
	}

    auto err = engine.set_position(sfen, moves);
    if (err.has_value())
    {
        terminate_on_critical_error(fullCommand, err->what());
    }

    
    engine.cyf_resource_sync(false);
}

#if STOCKFISH

namespace {

struct WinRateParams {
    double a;
    double b;
};

WinRateParams win_rate_params(const Position& pos) {

    int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>() + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();

    
    double m = std::clamp(material, 17, 78) / 58.0;

    
    constexpr double as[] = {-13.50030198, 40.92780883, -36.82753545, 386.83004070};
    constexpr double bs[] = {96.53354896, -165.79058388, 90.89679019, 49.29561889};

    double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
    double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

    return {a, b};
}



int win_rate_model(Value v, const Position& pos) {

    auto [a, b] = win_rate_params(pos);

    
    return int(0.5 + 1000 / (1 + std::exp((a - double(v)) / b)));
}
}

#endif



std::string USIEngine::format_score(const Score& s) {
    constexpr int TB_CP = 20000;
    const auto    format =
      overload{[](Score::Mate mate) -> std::string {
#if STOCKFISH
                   auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
        
#else
                   auto m = mate.plies;
#endif
                   return std::string("mate ") + std::to_string(m);
               },
#if STOCKFISH
               [](Score::Tablebase tb) -> std::string {
                   return std::string("cp ")
                        + std::to_string((tb.win ? TB_CP - tb.plies : -TB_CP - tb.plies));
               },
#endif
               [](Score::InternalUnits units) -> std::string {
                   return std::string("cp ") + std::to_string(units.value);
               }};

    return s.visit(format);
}



constexpr int NormalizeToPawnValue = Eval::PawnValue;




#if STOCKFISH
int UCIEngine::to_cp(Value v, const Position& pos) {

    
    
    

    auto [a, b] = win_rate_params(pos);

    return std::round(100 * int(v) / a);
}

std::string UCIEngine::wdl(Value v, const Position& pos) {
    std::stringstream ss;

    int wdl_w = win_rate_model(v, pos);
    int wdl_l = win_rate_model(-v, pos);
    int wdl_d = 1000 - wdl_w - wdl_l;
    ss << wdl_w << " " << wdl_d << " " << wdl_l;

    return ss.str();
}

#else

int USIEngine::to_cp(Value v) { return 100 * v / NormalizeToPawnValue; }

#endif


std::string USIEngine::square(Square s) {
	return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}


#if STOCKFISH
std::string USIEngine::move(Move m , bool chess960) { return USIEngine::move(m.to_move16()); }

#else

std::string USIEngine::move(Move m ) { return USIEngine::move(m.to_move16()); }

std::string USIEngine::move(Move16 m){

	std::stringstream ss;
	if (!m.is_ok())
	{
		ss << ((m.to_u16() == MOVE_RESIGN) ? "resign":
			   (m.to_u16() == MOVE_WIN)    ? "win"   :
			   (m.to_u16() == MOVE_NULL)   ? "null"  :
			   (m.to_u16() == MOVE_NONE)   ? "none"  :
			"");
	}
	else if (m.is_drop())
	{
		ss << m.move_dropped_piece();
		ss << '*';
		ss << m.to_sq();
	}
	else {
		ss << m.from_sq();
		ss << m.to_sq();
		if (m.is_promote())
			ss << '+';
	}
	return ss.str();
}



std::string USIEngine::move(const std::vector<Move>& moves)
{
	std::ostringstream oss;
	for (const auto& move : moves) {
		oss << " " << move;
	}
	return oss.str();
}
#endif


std::string USIEngine::to_lower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(), [](auto c) { return std::tolower(c); });

	return str;
}




#if STOCKFISH
Move USIEngine::to_move(const Position& pos, std::string str) {
    str = to_lower(str);
    
    

    
    for (const auto& m : MoveList<LEGAL>(pos))
        if (str == move(m, pos.is_chess960()))
            return m;
    
    

    return Move::none();
}
#else
Move USIEngine::to_move(const Position& pos, std::string str) {

	if (str == "resign")
		return Move::resign();

	if (str == "win")
		return Move::win();

	
	if (str == "0000" || str == "null" || str == "pass")
		return Move::null();

	
	Move move = pos.to_move(to_move16(str));

	
	
	if (pos.pseudo_legal_s<true>(move) && pos.legal(move))
		return move;

	
	sync_cout << "info string Error! : Illegal Input Move : " << str << sync_endl;

	return Move::none();
}




Move16 USIEngine::to_move16(const std::string& str)
{
	Move16 move = Move16::none();

	{
		
		if (str.length() <= 3)
			goto END;

		Square to = usi_to_sq(str[2], str[3]);
		if (!is_ok(to))
			goto END;

		bool promote = str.length() == 5 && str[4] == '+';
		bool drop = str[1] == '*';

		if (!drop)
		{
			Square from = usi_to_sq(str[0], str[1]);
			if (is_ok(from))
				move = promote ? make_move_promote16(from, to) : make_move16(from, to);
		}
		else
		{
			for (int i = 1; i <= 7; ++i)
				if (PieceToCharBW[i] == str[0])
				{
					move = make_move_drop16((PieceType)i, to);
					break;
				}
		}
	}

END:
	return move;
}



Square USIEngine::usi_to_sq(char f, char r) {
    File file = toFile(f);
    Rank rank = toRank(r);

    if (is_ok(file) && is_ok(rank))
        return file | rank;

    return SQ_NB;
}



Square USIEngine::to_square(const std::string& str) {
    if (str.size() != 2)
        return SQ_NB;
    return usi_to_sq(str[0], str[1]);
}

#endif

void USIEngine::on_update_no_moves(const Engine::InfoShort& info) {
    sync_cout << "info depth " << info.depth << " score " << format_score(info.score) << sync_endl;
}

void USIEngine::on_update_full(const Engine::InfoFull& info ) {

	std::stringstream ss;

    ss << "info";

#if STOCKFISH
	ss << " depth " << info.depth                 
       << " seldepth " << info.selDepth           
       << " multipv " << info.multiPV             
       << " score " << format_score(info.score);  
#else

	ss << " depth " << info.depth;

	
    if (info.selDepth)
        ss << " seldepth " << info.selDepth;

	ss << " multipv " << info.multiPV             
       << " score " << format_score(info.score);  
#endif

    if (!info.bound.empty())
        ss << " " << info.bound;

#if STOCKFISH
    if (showWDL)
        ss << " wdl " << info.wdl;
#endif

    ss << " nodes " << info.nodes        
       << " nps " << info.nps            
       << " hashfull " << info.hashfull  
#if STOCKFISH
       << " tbhits " << info.tbHits  
#endif
       << " time " << info.timeMs  
       << " pv " << info.pv;       

    sync_cout << ss.str() << sync_endl;
}

void USIEngine::on_iter(const Engine::InfoIter& info) {

	std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                     
       << " currmove " << info.currmove               
       << " currmovenumber " << info.currmovenumber;  

    sync_cout << ss.str() << sync_endl;
}

void USIEngine::on_bestmove(std::string_view bestmove, std::string_view ponder) {

    sync_cout << "bestmove " << bestmove;
    if (!ponder.empty())
        std::cout << " ponder " << ponder;
    std::cout << sync_endl;
}

void USIEngine::terminate_on_critical_error(const std::string& fullCommand,
                                            const std::string& message) {
    sync_cout << "info string CRITICAL ERROR: Command `" << fullCommand
              << "` failed. Reason: " << message << '\n'
              << sync_endl;
    std::exit(1);
}

void USIEngine::on_update_string(std::string_view info) { sync_cout << "info string " << info << sync_endl; }





Value USIEngine::cp_to_value(int v) {
    return Value((std::abs(v) < VALUE_MATE_IN_MAX_PLY) ? (NormalizeToPawnValue * v / 100) : v);
}



void USIEngine::isready() {
	
	
	engine.wait_for_search_finished();

	auto& options = engine.get_options();

	
    
    options.read_engine_options("engine_options.txt");

    

	
	if (options.count("EvalDir"))
    {
        auto eval_options_path = Path::Combine(options["EvalDir"], "eval_options.txt");
        options.read_engine_options(eval_options_path);
    }

    
    
    if (options.count("Cyf_YO_Compat") && bool(options["Cyf_YO_Compat"]))
    {
        int n = 0;
        for (const auto& r : CyfYoCompat::kDiff)
            if (options.count(r.option))
            {
                std::istringstream is(std::string("name ") + r.option + " value " + r.yo_value);
                options.setoption(is);
                ++n;
            }
        sync_cout << "info string Cyf_YO_Compat: " << n << " YO-DIFF options set to their YaneuraOu values" << sync_endl;
    }

	
#if defined(CYF_BLEND)
    {   
        auto& g = Cyfamate::blend_config();
        auto set = [&](const std::string& n, const std::string& v) { std::istringstream is("name " + n + " value " + v); options.setoption(is); };
        if (g.auto_detect && options.count("Cyf_Blend_Mode")) {
            if (g.model.empty() || g.model == "<empty>") { const std::string m = Cyfamate::auto_default_model(); if (m != "<empty>") set("Cyf_Blend_Model", m); }
            const bool present = !g.model.empty() && g.model != "<empty>" && GetFileAttributesA(g.model.c_str()) != INVALID_FILE_ATTRIBUTES;
            const bool was_set = g.mode_set;
            if (!was_set && !g.mode && present) { set("Cyf_Blend_Mode", "2"); g.mode_set = false; sync_cout << "info string cyfnet auto: model found (" << g.model << ") -> Blend mode 2" << sync_endl; }
            else if (g.mode && !present && !(g.mode == 2 && g.fallback_only)) { set("Cyf_Blend_Mode", "0"); g.mode_set = was_set; sync_cout << "info string cyfnet auto: no model -> NNUE only" << sync_endl; }
        }
    }
#endif
    engine.isready();
}


void USIEngine::moves() {
    auto& pos = engine.get_position();
    for (auto m : MoveList<LEGAL_ALL>(pos))
        std::cout << m << ' ';
    std::cout << std::endl;
}



void USIEngine::getoption(std::istringstream& is) {
    auto& options = engine_options();

    
    std::string option_name;
    is >> option_name;
    sync_cout << options.get_option(option_name) << sync_endl;
}




void USIEngine::qsearch_psv(std::istringstream& is) {
    std::string inputPath, outputPath;
    size_t      workerCount = 0;

    is >> inputPath >> outputPath;
    if (!(is >> workerCount))
        workerCount = 0;

    std::string message;
    const bool  ok = engine.qsearch_psv(inputPath, outputPath, workerCount, message);

    if (!message.empty())
        sync_cout << "info string " << message << sync_endl;

    if (!ok)
        sync_cout << "info string qsearch_psv failed" << sync_endl;
}


void USIEngine::unittest(std::istringstream& is) { Test::UnitTest(is, engine); }



void USIEngine::enqueue_command_line_commands(const CommandLine& cli) {
    
    std_input.parse_args(cli);
}


void USIEngine::enqueue_startup_file_commands(const std::string& filename) {
    std::vector<std::string> lines;
    if (SystemIO::ReadAllLines(filename, lines).is_ok())
    {
        for (auto& line : lines)
            std_input.push(line);
    }
}


void USIEngine::enqueue_startup_commands(const CommandLine& cli) {
    enqueue_command_line_commands(cli);
    enqueue_startup_file_commands("startup.txt");
}


void USIEngine::enqueue_command_from_file(std::istringstream& is) {
    std::string filename = "";
    is >> filename;
    if (!filename.empty())
    {
        filename += ".txt";
        sync_cout << "USI Commands from File = " << filename << sync_endl;

        std::vector<std::string> lines;
        if (SystemIO::ReadAllLines(filename, lines).is_ok())
            for (auto& line : lines)
                std_input.push(line);
        else
            sync_cout << "Error : File Not Found." << sync_endl;
    }
}






#if defined(ENABLE_TEST_CMD)




namespace Test {

bool normal_test_cmd(IEngine& engine, std::istringstream& is, const std::string& token);


bool mate_test_cmd(IEngine& engine, std::istringstream& is, const std::string& token);

void test_cmd(IEngine& engine, std::istringstream& is) {
    std::string token;
    is >> token;

    

    
    if (normal_test_cmd(engine, is, token))
        return;

    
    if (mate_test_cmd(engine, is, token))
        return;

    sync_cout << "info string Error! : unknown command = " << token << sync_endl;
}
}

#endif  




void USIEngine::UnitTest(Test::UnitTester& tester, IEngine& engine) {
    auto section1 = tester.section("USI");

    Position  pos;
    StateInfo si;

    
    auto hirate_init = [&] { pos.set_hirate(&si); };

    
    auto sfen_init = [&](const std::string& sfen) { pos.set(sfen, &si); };

    {
        auto section2 = tester.section("to_move()");
        {
            

            sfen_init("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

            auto moves =
              "6a5b 1g1f 4a3b 1f1e 3a2b 2g2f 2c2d 7g7f 3b2c 2f2e 2d2e 2h2e P*2d 2e9e 7a8b 8h6f 8c8d 3i3h 5b6b 7i6h 6b7b 9e9f 7b8c 6h7g 7c7d 7f7e 7d7e 7g8f 8b7c 1e1d 1c1d 8f7e P*7d 7e8f 5a5b P*7b 5b6b 7b7a+ 6b7a P*7e 7a6b 7e7d 7c7d 8f9e P*7e 9f8f 8d8e 8f9f 5c5d 5g5f 6c6d 5f5e 6b6c 5e5d 6c5d 8i7g 6d6e 6f5g 2c3d 6i7h 3d4e 4g4f P*5f 5g6h 4e5e 3h4g 6e6f 4f4e 9c9d 9e9d 8c8d 6g6f P*9e 9d9c 8d9d P*5g 9e9f P*2c 2b2c 5g5f 5e4e P*4f 4e4d 4f4e 4d4e 9c8b 7e7f 7g6e 7d7e P*4f 4e4d 4f4e 4d4e 7h6g R*8i 5i4h 8i9i+ P*4f 4e4d 4f4e 4d4e 6e7c+ 4c4d 7c7d 9d8d 7d8d 7e8d 4g4f 4e4f 6h4f L*4e 6g5g 4e4f 5g4f 5d4c 5f5e P*5c G*2b 2c3d L*5i 4d4e 4f3f 4e4f 3f4f P*4e 4f3f 4e4f 3f4f P*4e 4f3f 4e4f 3f4f P*4e 4f3f 4e4f 3f4f P*4e 4f3f 4e4f 3f4f P*4e 4f3f N*4d 4h3h 4d3f 3g3f 4e4f P*2g S*4g 3h2h 4g3f P*4d 4c4d P*4h 2d2e P*3g 3f2g 2h2g 2e2f 2g2f 3d3e 2f2e G*2d 2e1f 1d1e 1f1g P*2f N*3f 3e3f 3g3f B*2g S*3h 2g3f+ 1g2h N*3e S*1h 2f2g+ 1h2g 3e2g+ 3h2g 3f6c N*3f 4d3d 3f2d P*2f 2g2f S*2g 2h3i P*2h G*3g 2h2i+ 3i2i P*2h 2i3i N*5g 3g4f 5g4i+ 3i4i G*6h P*7i 9i7i G*5h 6h5h 4i5h 7i7h G*6h G*6g 5h4g 7h6h N*3f G*5g 4g3g 6h4h 3g2g 4h4f P*3g 3d4e S*3e 4f4g P*4i 4e5f 4i4h 4g4h 5i5g 5f5g 1i1e 5g5h 2g1f 5h5i P*4f 4h4i 1e1c+ 4i5h 1f1e 5h4g G*3i 4g5h 8b7a+ 6c6d 1e1d 6d5e 8g8f 8e8f P*8g 8f8g";
            
            

            std::istringstream is(moves);
            std::string        token;
            bool          fail = false;

            StateInfo si[512];
            while (is >> token)
            {
                Move m = USIEngine::to_move(pos, token);
                if (m == Move::none())
                    fail = true;

                pos.do_move(m, si[pos.game_ply()]);
            }

            tester.test("pawn's unpromoted move", !fail);
        }
    }
}




#if 0




void user_test(Position& pos, std::istringstream& is);

#if defined(ENABLE_TEST_CMD)
	void generate_moves_cmd(Position& pos);
#endif

#if defined(USE_MATE_DFPN)

void mate_cmd(Position& pos, std::istream& is);
#endif



#if defined(USE_GAMEOVER_HANDLER) || defined(YANEURAOU_ENGINE_DEEP)
void gameover_handler(const string& cmd);
#endif







bool parse_ponderhit(istringstream& is, Search::LimitsType& limits)
{
	string token;
	bool token_processed = false;

	while (is >> token)
	{
		
		token_processed = true;

		
		if (token == "wtime")     is >> limits.time[WHITE];
		else if (token == "btime")     is >> limits.time[BLACK];

		
		else if (token == "winc")      is >> limits.inc[WHITE];
		else if (token == "binc")      is >> limits.inc[BLACK];

		
		else if (token == "rtime")     is >> limits.rtime;

		
		else if (token == "byoyomi") {
			TimePoint t = 0;
			is >> t;

			
			limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = t;
		}
	}
	return token_processed;
}






#if 0

void USIEngine::isready()
{
	
	
	

	
	
	

	auto& options = engine_options();

	
	

	options.read_engine_options(Path::Combine(options["EvalDir"], "eval_options.txt"));

	
	
#if !defined(__EMSCRIPTEN__)
	

	
	
	
	
	
	

	
	
	
	

	
	auto thread_started = false;

	
	auto thread_end = false;

	
	auto th = std::thread([&] {
		
		thread_started = true;

		int count = 0;
		while (!thread_end)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (++count >= 50 )
			{
				count = 0;
				sync_cout << sync_endl; 

				
				
				
			}
		}
		});
	SCOPE_EXIT({ thread_end = true; th.join(); });

	
	while (!thread_started)
		Tools::sleep(100);

	
#endif

	

#if defined(YANEURAOU_ENGINE_DEEP)

	
	int threads_num =
		(int)Options["UCT_Threads1"] + (int)Options["UCT_Threads2"] + (int)Options["UCT_Threads3"] + (int)Options["UCT_Threads4"] +
		(int)Options["UCT_Threads5"] + (int)Options["UCT_Threads6"] + (int)Options["UCT_Threads7"] + (int)Options["UCT_Threads8"];

	Threads.set(std::max(threads_num,1));
#else
	
#endif

#if defined (USE_EVAL_HASH)
	Eval::EvalHash_Resize(Options["EvalHash"]);
#endif

	
	engine.isready();

#if 0
	

#if defined(YANEURAOU_ENGINE_DEEP)

	
	
	Eval::load_eval();
	engine.engine.eval_loaded = true;

#else

	
	
	engine.verify_networks();
#endif

	
	

#if defined(YANEURAOU_ENGINE_DEEP)
	
#else
	
#endif

	

#if defined (USE_EVAL_HASH)
	Eval::EvalHash_Clear();
#endif

	

	
	

#endif

	sync_cout << "readyok" << sync_endl;

}
#endif












std::string USIEngine::value(Value v)
{
	ASSERT_LV3(-VALUE_INFINITE < v && v < VALUE_INFINITE);

	std::stringstream ss;

	
	if (v == VALUE_NONE)
		ss << "none";
	else if (std::abs(v) < VALUE_MATE_IN_MAX_PLY)
		
		ss << "cp " << USIEngine::to_cp(v);
	
	else if (v == -VALUE_MATE)
		
		
		
		
		ss << "mate -0";
	else
		ss << "mate " << (v > 0 ? VALUE_MATE - v : -VALUE_MATE - v);

	return ss.str();
}



#if defined(__EMSCRIPTEN__)



static StateListPtr states(new StateList(1));


EMSCRIPTEN_KEEPALIVE extern "C" int usi_command(const char *c_cmd) {
	std::string cmd(c_cmd);

	static Position pos;
	string token;

	for (Thread* th : Threads) {
		if (!th->threadStarted)
			return 1;
	}

	usi_cmdexec(pos, states, cmd);

	return 0;
}
#endif

#endif

} 