#ifndef USI_H_INCLUDED
#define USI_H_INCLUDED

#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>

#include "types.h"
#include "engine.h"
#include "position.h"
#include "testcmd/unit_test.h"

namespace YaneuraOu {








class USIEngine {
public:

#if STOCKFISH
	
	
	USIEngine(int argc, char** argv);
#else
	 
	 
	 
	 
	 void set_engine(IEngine& _engine);
#endif

	
	
	void loop();

	
	
	

	
#if STOCKFISH
	static int         to_cp(Value v, const Position& pos);
    
#else
	static int   to_cp(Value v);
#endif

    
    static std::string format_score(const Score& s);

	
    static std::string square(Square s);

    
    static std::string move(Move m );

#if STOCKFISH
    
    
    static std::string wdl(Value v, const Position& pos);
#endif

	
    static std::string to_lower(std::string str);

    
    
    
    static Move to_move(const Position& pos, std::string str);

#if !STOCKFISH
	
    
    static Move16 to_move16(const std::string& str);
#endif

	
    
#if STOCKFISH
    static Search::LimitsType parse_limits(std::istream& is);
#else
    Search::LimitsType parse_limits(std::istream& is);
#endif

    
    OptionsMap& engine_options() { return engine.get_options(); }

#if !STOCKFISH
    
    
    

	
	static Value cp_to_value(int v);

	
	
	
	
	
	static std::string value(Value v);

    
    
    static Square usi_to_sq(char f, char r);

    
    
    static Square to_square(const std::string& str);

    
    static std::string move(Move16 m );

    
    static std::string move(const std::vector<Move>& moves);

	
	
	StandardInput std_input;

    
    
    void enqueue_command_line_commands(const CommandLine& cli);

    
    void enqueue_startup_file_commands(const std::string& filename);

    
    void enqueue_startup_commands(const CommandLine& cli);

	
	static void UnitTest(Test::UnitTester& tester, IEngine& engine);
#endif

private:
	
#if STOCKFISH
	
	Engine engine;

	CommandLine cli;
	
#else
	
	
	EngineWrapper engine;
#endif

	
	static void print_info_string(std::string_view str);

	
	
	

	
	

	void          go(std::istringstream& is);
	void          bench(std::istream& args);
	void          benchmark(std::istream& args);
	void          position(std::istringstream& is);
	void          setoption(std::istringstream& is);
	std::uint64_t perft(const Search::LimitsType&);

#if !STOCKFISH
	

	void isready();
    void moves();
    void getoption(std::istringstream& is);
    void qsearch_psv(std::istringstream& is);
    void unittest(std::istringstream& is);
#endif

	
	
	
	

	static void on_update_no_moves(const Engine::InfoShort& info);
    static void on_update_full(const Engine::InfoFull& info );
	static void on_iter(const Engine::InfoIter& info);
	static void on_bestmove(std::string_view bestmove, std::string_view ponder);
#if !STOCKFISH
	static void on_update_string(std::string_view info);
#endif

    
	
	
	
	
    void init_search_update_listeners();

    
    [[noreturn]] void terminate_on_critical_error(const std::string& fullCommand,
                                                  const std::string& message);

#if !STOCKFISH
	

	
	void enqueue_command_from_file(std::istringstream& is);

	
	void set_option_if_exists(const std::string& option_name, const std::string& option_value);

	
	
	bool usi_cmdexec(const std::string& cmd);

	

	
	std::string last_position_cmd_string = "position startpos";

	
	std::string last_go_cmd_string;

	
	
	
	bool sp_ponder_pending = false;

	
	
	
	bool open_infinite = false, open_ponder = false;

#endif
};

} 

#endif 
