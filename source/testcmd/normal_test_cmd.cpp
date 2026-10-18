#include "../config.h"

#if defined(ENABLE_TEST_CMD)


#include <fstream>
#include <iomanip>
#include <sstream>
#include "../position.h"
#include "../usi.h"
#include "../thread.h"
#include "../search.h"
#include "../movegen.h"
#include "../evaluate.h"

namespace YaneuraOu {
namespace {

	void gen_moves(IEngine& engine, std::istringstream& is)
	{
		auto& pos = engine.get_position();

		int64_t loop = 30000000;

		std::string token;
		while (is >> token)
		{
			if (token == "loop")
				is >> loop;
		}


		auto start = now();
		std::cout << "Generate Moves Test : " << std::endl
				  << "  loop = " << loop << std::endl;

		for (int i = 0; i < loop; ++i)
		{
			if (pos.checkers())
				MoveList<EVASIONS> ml(pos);
			else
				MoveList<NON_EVASIONS> ml(pos);
		}

		auto end = now();

		std::cout << "..done." << std::endl;

		std::cout << pos;

		if (pos.checkers())
			for (auto m : MoveList<EVASIONS>(pos))
				std::cout << Move(m) << ' ';
		else
			for (auto m : MoveList<NON_EVASIONS>(pos))
				std::cout << Move(m) << ' ';

		std::cout << std::endl << (1000 * loop / (end - start)) << " times per second." << std::endl;
	}


	void auto_play(IEngine& engine, std::istringstream& is)
	{
		auto& options = engine.get_options();
		auto& threads = engine.get_threads();

		Position pos;

		uint64_t loop = 50000; 

		bool verbose = false;

		int movetime = 500;    

		s64 nodes_limit = 0;
		
		std::string token;
		while (is >> token)
		{
			if (token == "loop")
				is >> loop;
			else if (token == "verbose")
				verbose = true;
			else if (token == "movetime")
				is >> movetime;
			else if (token == "nodes")
				is >> nodes_limit;
		}

		std::cout   << "Auto Play test : " << std::endl
					<< "  loop     = " << loop << std::endl
					<< "  movetime = " << movetime << std::endl
					<< "  verbose  = " << verbose << std::endl
					<< "  nodes    = " << nodes_limit << std::endl
			;

		const int MAX_PLY = 256; 

		auto start = now();



		if (options.count("RootMateSearchNodesLimit"))
			options.set_option_if_exists("RootMateSearchNodesLimit",std::to_string(100)); 


		engine.isready();

		PRNG prng;

		for (uint64_t i = 0; i < loop ; ++i)
		{
			StateListPtr states(new StateList(1));
			pos.set_hirate(&states->back());

			if (verbose)
				std::cout << "position startpos moves";

			char result = ' ';
			Color mateScoreColor = (Color)-1;

			for (int ply = 0;; ++ply)
			{
				if (!(ply < MAX_PLY))
				{
					result = 'M'; 
					break;
				}

				if (pos.is_mated())
				{
					result = (pos.side_to_move() == BLACK) ? 'W' : 'B'; 
					break;
				}

				auto rep = pos.is_repetition(16);
				if (rep == REPETITION_DRAW)
				{
					result = '.'; 
					break;
				}

#if defined(USE_ENTERING_KING_WIN)
				
				if (pos.DeclarationWin())
				{
					result = (pos.side_to_move() == BLACK) ? 'b' : 'w'; 
					break;
				}
#endif

				StateListPtr states0(new StateList(0));
				*states0.get() = *states.get(); 

				Search::LimitsType lm;
                lm.startTime = now();
				lm.movetime = movetime * (1000 + prng.rand(3000)) / 1000;
				lm.nodes    = nodes_limit * (1000 + prng.rand(3000)) / 1000;

				threads.start_thinking(options, pos, states0 , lm);
				threads.main_thread()->wait_for_search_finished(); 
				auto& rootMoves = threads.main_thread()->worker->rootMoves;

				ASSERT_LV3(rootMoves.size());

				Move m  = rootMoves.at(0).pv[0]; 
				Value v = rootMoves.at(0).score;

				if (mateScoreColor == pos.side_to_move() && v < VALUE_MATE)
				{
					std::cout << std::endl << "[Error!:MateColor]" << std::endl; 
				}

				if (v >= VALUE_MATE)
				{
					mateScoreColor = pos.side_to_move();
				}

				if (verbose)
					std::cout << " " << m;

				states->emplace_back();
				pos.do_move(m, states->back());
			}

			if (verbose)
				std::cout << std::endl << "result = " << result << std::endl;
			else
				std::cout << result;
		}
	}

#if defined(YANEURAOU_ENGINE)
	void eval_accuracy(IEngine& engine, std::istringstream& is)
	{
		std::string psv_path;
		is >> psv_path;

		if (psv_path.empty())
		{
			std::cout << "Error: usage: test eval_accuracy <psv_path>" << std::endl;
			return;
		}

		std::ifstream f(psv_path, std::ios::binary);
		if (!f)
		{
			std::cout << "Error: cannot open " << psv_path << std::endl;
			return;
		}

		f.seekg(0, std::ios::end);
		auto file_size = (uint64_t)f.tellg();
		f.seekg(0, std::ios::beg);
		if (file_size % sizeof(PsvRecord) != 0)
		{
			std::cout << "Error: file size " << file_size
					  << " is not a multiple of " << sizeof(PsvRecord)
					  << " byte (PsvRecord size) — possibly corrupted/truncated"
					  << std::endl;
			return;
		}
		const uint64_t total_records = file_size / sizeof(PsvRecord);

		std::cout << "test eval_accuracy: " << psv_path << std::endl
				  << "  records = " << total_records << std::endl;

		engine.isready();

		auto& pos = engine.get_position();
		StateInfo si;

		uint64_t compared = 0;
		uint64_t sign_match = 0;
		uint64_t drawn = 0;
		uint64_t skipped = 0;

		auto start_time = now();
		auto last_report = start_time;

		PsvRecord rec;
		while (f.read(reinterpret_cast<char*>(&rec), sizeof(PsvRecord)))
		{
			if (pos.set_from_packed_sfen(rec.sfen, &si, false, rec.gamePly).is_not_ok())
			{
				skipped++;
				continue;
			}

			if (rec.game_result == 0)
			{
				drawn++;
				continue;
			}

			Value v = engine.evaluate();

			bool pred  = v >= VALUE_ZERO;
			bool truth = rec.game_result > 0;
			if (pred == truth) sign_match++;
			compared++;

			auto cur = now();
			if (cur - last_report >= 5000)
			{
				last_report = cur;
				double pct = (total_records > 0)
					? 100.0 * (double)compared / (double)total_records
					: 0.0;
				std::cout << "  processed " << compared << " / " << total_records
						  << " (" << std::fixed << std::setprecision(1) << pct
						  << "%)" << std::endl;
			}
		}

		auto elapsed_ms = now() - start_time;
		double accuracy = (compared > 0)
			? (double)sign_match / (double)compared
			: 0.0;

		std::cout << "----" << std::endl;
		std::cout << "test eval_accuracy result:" << std::endl;
		std::cout << "  decisive   = " << compared
				  << " (skipped " << skipped << " malformed records)" << std::endl;
		std::cout << "  accuracy   = " << std::fixed << std::setprecision(4)
				  << (accuracy * 100.0) << "% (" << sign_match << "/" << compared
				  << " matches; W vs L sign agreement)" << std::endl;
		std::cout << "  drawn      = " << drawn
				  << " (excluded from accuracy)" << std::endl;
		std::cout << "  elapsed    = " << elapsed_ms << " ms ("
				  << (elapsed_ms > 0
					  ? ((compared + drawn) * 1000) / (uint64_t)elapsed_ms
					  : 0)
				  << " positions/sec)" << std::endl;
	}
#endif 
} 


namespace Test
{
	bool normal_test_cmd(IEngine& engine , std::istringstream& is, const std::string& token)
	{
		if (token == "genmoves")              gen_moves(engine, is);       
		else if (token == "autoplay")         auto_play(engine, is);       
#if defined(YANEURAOU_ENGINE)
		else if (token == "eval_accuracy")    eval_accuracy(engine, is);   
#endif
		else return false;									               
			
		return true;
	}

} 

} 

#endif 
