#include "../config.h"

#if defined(ENABLE_TEST_CMD)



#include <sstream>

#include "../mate/mate.h"

#include "../position.h"
#include "../usi.h"
#include "../thread.h"
#include "../search.h"

using namespace std;
namespace YaneuraOu {
namespace {


	void mate_bench(IEngine& engine, std::istringstream& is)
	{
		auto& options = engine.get_options();
		auto& threads = engine.get_threads();

		Position pos;

#if !(defined (USE_MATE_SOLVER) || defined(USE_MATE_DFPN))
		cout << "Error! define USE_MATE_SOLVER or USE_MATE_DFPN" << endl;
#else

		size_t num = 10000;

		size_t loop = 1;

		string filename = "mate.sfen";

		int mate_ply = 3;

		u32 nodes_limit = 10000;

		u32 dfpn_mem = 1024;

		u32 dfpn_hash = 1024;

		bool verbose = false;

		int test_mode = 3; 

		std::string token;
		while (is >> token)
		{
			if (token == "num")
				is >> num;
			else if (token == "loop")
				is >> loop;
			else if (token == "file")
				is >> filename;
			else if (token == "mate_ply")
				is >> mate_ply;
			else if (token == "nodes")
				is >> nodes_limit;
			else if (token == "verbose")
				verbose = true;
			else if (token == "mode")
				is >> test_mode;

#if defined(USE_MATE_DFPN)
			else if (token == "dfpn_mem")
				is >> dfpn_mem;
			else if (token == "dfpn_hash")
				is >> dfpn_hash;
#endif
		}

		std::cout << "mate bench :" << std::endl
			<< " num (number of problems) = " << num << endl
			<< " sfen file name           = " << filename << endl
			<< " mate_ply                 = " << mate_ply << endl
			<< " nodes(nodes limit)       = " << nodes_limit << endl
			<< " verbose                  = " << verbose << endl
			<< " mode                     = " << test_mode << endl
#if defined(USE_MATE_DFPN)
			<< " dfpn_mem [MB]            = " << dfpn_mem << endl
			<< " dfpn_hash[MB]            = " << dfpn_hash << endl
#endif
			;

#if defined(USE_MATE_DFPN)
		Mate::MateHashTable mate_hash;
		mate_hash.resize(dfpn_hash);

		Mate::Dfpn::MateDfpnSolver dfpn1(Mate::Dfpn::DfpnSolverType::Node32bit);
		dfpn1.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn2(Mate::Dfpn::DfpnSolverType::Node16bitOrdering);
		dfpn2.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn3(Mate::Dfpn::DfpnSolverType::Node48bitOrdering);
		dfpn3.alloc(dfpn_mem);

		Mate::Dfpn::MateDfpnSolver dfpn4(Mate::Dfpn::DfpnSolverType::Node32bitWithHash);
		dfpn4.alloc(dfpn_mem);
		dfpn4.set_hash_table(&mate_hash);

		Mate::Dfpn::MateDfpnSolver dfpn5(Mate::Dfpn::DfpnSolverType::Node48bitOrderingWithHash);
		dfpn5.alloc(dfpn_mem);
		dfpn5.set_hash_table(&mate_hash);

#endif

		ifstream f(filename);
		unique_ptr<vector<string>> problems = make_unique<vector<string>>();
		problems->reserve(num);

		cout << "read problems.." << endl;
		for (size_t i = 0; i < num; ++i)
		{
			string sfen;
			if (!getline(f, sfen) || f.eof())
				break;
			problems->emplace_back(sfen);

			if (i % 10000 == 0 && i > 0)
				cout << ".";
		}
		auto problem_num = problems->size();
		cout << "..number of problems read = " << problem_num << endl;

		if (problem_num == 0)
			return;

		string sfen2;
		u64 nodes_searched;

		auto bench = [&](function<Move()> solver, string test_name) {
			ElapsedTimer timer;
			timer.reset();

			cout << "===== " << test_name << " =====" << endl
				<< "test start" << endl;

			u32 solved = 0;
			nodes_searched = 0;

			TimePoint last_pv = 0;

			TimePoint elapsed = 0;

			size_t times = 0;
			
			auto output_pv = [&]()
			{
				cout << " number of times = " << times << " , elapsed = " << elapsed
					<< " , called per second = " << (double)times * 1000 / (elapsed + 0.00000001)
					<< " , solved per second = " << (double)solved * 1000 / (elapsed + 0.00000001)
					<< " , nps = " << nodes_searched * 1000 / elapsed
					<< " , solved = " << solved << " , accuracy = " << (100.0 * solved / times) << "%" << endl;
			};

			size_t size = min(num, problem_num);
			for (size_t i = 0; i < size; ++i)
			{
				elapsed = timer.elapsed();
				if (last_pv + 2000 < elapsed)
				{
					last_pv = elapsed;
					times = i * loop;
					
					output_pv();
				}

				auto sfen = (*problems)[i];
				sfen2 = sfen;
				StateInfo si;
				pos.set(sfen, &si);
				Move move;
				for (size_t j = 0; j < loop; ++j)
				{
					move = solver();
					if (move)
						solved++;
					else if (verbose && j == 0)
						cout << "unsolved : line = " << i + 1 << endl <<
						"sfen " << sfen << endl; 

				}
			}

			elapsed = timer.elapsed();
			times = size * loop;
			output_pv();

			cout << test_name << " test end" << endl;

		};

#if defined(USE_MATE_SOLVER)
		if (test_mode & 1)
			bench([&]() { Mate::MateSolver solver; return solver.mate_odd_ply(pos, mate_ply, true); }, "mate_odd_ply");
#endif

#if defined(USE_MATE_DFPN)
		if (test_mode & 2)
			bench([&]() { auto m = dfpn1.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn1.get_nodes_searched(); return m; }, "mate_dfpn1 : Node32bit");

		if (test_mode & 4)
			bench([&]() { auto m = dfpn2.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn2.get_nodes_searched(); return m; }, "mate_dfpn2 : Node16bitOrdering");

		if (test_mode & 8)
			bench([&]() { auto m = dfpn3.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn3.get_nodes_searched(); return m; }, "mate_dfpn3 : Node48bitOrdering");

		if (test_mode & 16)
		{
			mate_hash.clear(threads);
			bench([&]() { auto m = dfpn4.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn4.get_nodes_searched(); return m; }, "mate_dfpn4 : Node32bitWithHash");
		}

		if (test_mode & 32)
		{
			mate_hash.clear(threads);
			bench([&]() { auto m = dfpn5.mate_dfpn(pos, nodes_limit); nodes_searched += dfpn5.get_nodes_searched(); return m; }, "mate_dfpn5 : Node48bitOrderingWithHash");
		}

#endif


#endif 
	}


	void mate_dfpn(IEngine& engine, std::istringstream& is)
	{
#if !defined(USE_MATE_DFPN)
		cout << "Error! : define USE_MATE_DFPN" << endl;
		return;
#else
		auto& pos = engine.get_position();

		size_t nodes = 10000000;

		size_t mem = 1024;
		size_t dfpn_hash = 1024;

		string token;
		while (is >> token)
		{
			if (token == "nodes")
				is >> nodes;
			else if (token == "mem")
				is >> mem;
			else if (token == "hash")
				is >> dfpn_hash;
		}

		cout << "df-pn mate :" << endl
			 << " nodes = " << nodes << endl
			 << " mem   = " << mem << "[MB]"<< endl
			 << " hash  = " << dfpn_hash << "[MB]" << endl
			;

#if 1
		Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node64bit);
#else
		Mate::Dfpn::MateDfpnSolver dfpn(Mate::Dfpn::DfpnSolverType::Node48bitOrderingWithHash);
		Mate::MateHashTable mate_hash;
		mate_hash.resize(dfpn_hash);
		mate_hash.clear();
		dfpn.set_hash_table(&mate_hash);
#endif

		dfpn.alloc(mem);

		ElapsedTimer time;
		cout << "start mate." << endl;
		time.reset();
		Move m = dfpn.mate_dfpn(pos, (u32)nodes);
		cout << "time = " << time.elapsed() << endl;
		auto nodes_searched = dfpn.get_nodes_searched();
		if (m != Move::none() && m != Move::null())
		{
			cout << "solved! , nodes_searched = " << nodes_searched << endl;

			auto pv = dfpn.get_pv();
			for (auto m : pv)
				cout << m << " ";

			cout << endl;
		}
		else {
			if (m == Move::null())
				cout << "solved! this is no mate. nodes_searched = " << nodes_searched << endl;
			else if (dfpn.is_out_of_memory())
				cout << "out of memory" << endl;
			else if (dfpn.get_nodes_searched() >= nodes)
				cout << "unsolved , nodes limit." << endl;
			else
				cout << "unsolved. why?" << endl;
		}

#endif
	}



#if defined (TANUKI_MATE_ENGINE) || defined(YANEURAOU_MATE_ENGINE)
	static const char* TestMateEngineSfen[] = {
		"3sks3/9/4+P4/9/9/+B8/9/9/9 b S2rb4gs4n4l17p 1",
		"7nl/7k1/6p2/6S1p/9/9/9/9/9 b GS2r2b3g2s3n3l16p 1",
		"4k4/9/PPPPPPPPP/9/9/9/9/9/9 b B4L2rb4g4s4n9p 1",
		"l2g5/2s3g2/3k1p2p/P2pp2P1/1pP4s1/p1+B6/NP1P+nPS1P/K1G4+p1/L6NL b RBGNLPrs3p 1",
		"6lnk/6+Rbl/2n4pp/7s1/1p2P2NP/p1P2PPP1/1P4GS1/6GK1/LNr5L b B2G2S6Pp 1",
		"lnks5/1pg1s4/2p5p/p4+r3/P1g6/1Nn6/BKN1P3P/9/LG2s4 w GSL2Prbl9p 1",
		"l7l/2+Rbk4/3rp4/2p3pPs/p2P1p2p/2P1G4/P1N1PPN2/2GK2G2/L7L b B2S6Pgs2n 1",
		"l5g1l/2s+B5/p2ppp2p/5kpP1/3n5/6Pp1/P3PP1lP/2+nr2SS1/3N1GKRL w G2Pbgsn3p 1",
		"l4g2l/7k1/p1+Pp3pp/5ss1P/3Pp1gP1/P3SL3/N2GPK3/1+rP6/+p6RL w BG2N2Pbsn3p 1",
		"l2s3nl/3g1p+R+R1/p1k5p/2pPp4/1p1p5/5Sp2/PPP1PP2P/3G5/L1K4NL b BG2S2Pbg2np 1",
		"ln7/2gk1S+S2/2+rpPp2G/2p5p/PP4P2/3B4P/K1SP3PN/1Sg2P+np1/L+r6L w L2Pbgn3p 1",
		"6p1l/1+R1G2g2/5pns1/pp1pk3p/2p3P2/P7P/1L1PSP+b2/1SG1K2P1/L5G1L w N2Prbs2n3p 1",
		"lng3+R2/2kgs4/ppp6/1B1pp4/7B1/2P2pLp1/PP1PP3P/1S1K2p2/LN5GL b RG2SP2n3p 1",
	};
#endif

	void mate_bench2(IEngine& engine, [[maybe_unused]] std::istringstream& is)
	{
#if !defined (TANUKI_MATE_ENGINE) && !defined(YANEURAOU_MATE_ENGINE)
		cout << "Error! : define TANUKI_MATE_ENGINE or YANEURAOU_MATE_ENGINE" << endl;
#else
		auto& options = engine.get_options();
		auto& threads = engine.get_threads();

		string token;

		string ttSize = (is >> token) ? token : "1024";

		options.set_option_if_exists("USI_Hash", ttSize);

		Search::LimitsType limits;
		limits.nodes = 0;
		limits.mate = 100000; 

		engine.isready();

		int64_t nodes = 0;

		int64_t nodes_main = 0;

		ElapsedTimer time;

		for (const char* sfen : TestMateEngineSfen) {
			Position pos;
			StateListPtr st(new StateList(1));
			pos.set(sfen, &st->back());

			sync_cout << "\nPosition: " << sfen << sync_endl;

			time.reset();

			StateListPtr states(new std::deque<StateInfo>(1));

            limits.startTime = now();
			threads.start_thinking(options, pos, st, limits);
			threads.main_thread()->wait_for_search_finished(); 


			nodes += threads.nodes_searched();
			nodes_main += threads.nodes_searched();
		}

		auto elapsed = time.elapsed() + 1; 

		sync_cout << "\n==========================="
			<< "\nTotal time (ms) : " << elapsed
			<< "\nNodes searched  : " << nodes
			<< "\nNodes/second    : " << 1000 * nodes / elapsed;

		if ((int)options["Threads"] > 1)
			cout
			<< "\nNodes searched(main thread) : " << nodes_main
			<< "\nNodes/second  (main thread) : " << 1000 * nodes_main / elapsed;

		cout << sync_endl;

#endif 
	}

} 



namespace Test
{
	bool mate_test_cmd(IEngine& engine, std::istringstream& is, const std::string& token)
	{
		if (token == "matebench")  mate_bench(engine,is);       
		else if (token == "matebench2") mate_bench2(engine,is);      
		else if (token == "dfpn")       mate_dfpn(engine,is);        
		else return false;									         
			
		return true;
	}

} 

} 

#endif 
