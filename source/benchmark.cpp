#include "benchmark.h"
#include "numa.h"

#include <fstream>
#include <iostream>




namespace {


const std::vector<std::string> Defaults =
{
	
	"lnsgkgsnl/1r7/p1ppp1bpp/1p3pp2/7P1/2P6/PP1PPPP1P/1B3S1R1/LNSGKG1NL b - 9",

	
	"l4S2l/4g1gs1/5p1p1/pr2N1pkp/4Gn3/PP3PPPP/2GPP4/1K7/L3r+s2L w BS2N5Pb 1",

	
	
	"6n1l/2+S1k4/2lp4p/1np1B2b1/3PP4/1N1S3rP/1P2+pPP+p1/1p1G5/3KG2r1 b GSN2L4Pgs2p 1",

	
	
	"l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w RGgsn5p 1",
};



const std::vector<std::vector<std::string>> BenchmarkPositions = { {Defaults} };

} 

namespace YaneuraOu::Benchmark {


























std::vector<std::string> setup_bench(const std::string& currentFen, std::istream& is) {
	std::vector<std::string> fens, list;
	std::string              go, token;

#if STOCKFISH
	
	std::string ttSize    = (is >> token) ? token : "16";
	std::string threads   = (is >> token) ? token : "1";
	std::string limit     = (is >> token) ? token : "13";
	std::string fenFile   = (is >> token) ? token : "default";
	std::string limitType = (is >> token) ? token : "depth";
#else
	

	std::string ttSize    = (is >> token) ? token : "1024";
    std::string threads   = (is >> token) ? token : "1";
    std::string limit     = (is >> token) ? token : "15000";
    std::string fenFile   = (is >> token) ? token : "default";
    std::string limitType = (is >> token) ? token : "movetime";
#endif

	go = limitType == "eval" ? "eval" : "go " + limitType + " " + limit;

	if (fenFile == "default")
		fens = Defaults;

	else if (fenFile == "current")
		fens.push_back(currentFen);

	else
	{
		std::string   fen;
		std::ifstream file(fenFile);

		if (!file.is_open())
		{
			std::cerr << "Unable to open file " << fenFile << std::endl;
			exit(EXIT_FAILURE);
		}

		while (getline(file, fen))
			if (!fen.empty())
				fens.push_back(fen);

		file.close();
	}

	list.emplace_back("setoption name Threads value " + threads);
#if STOCKFISH
	list.emplace_back("setoption name Hash value " + ttSize);
#else
    list.emplace_back("setoption name USI_Hash value " + ttSize);
#endif
	
    list.emplace_back("ucinewgame");

	for (const std::string& fen : fens)
		if (fen.find("setoption") != std::string::npos)
			list.emplace_back(fen);
		else
		{
#if STOCKFISH
			list.emplace_back("position fen " + fen);
#else
            list.emplace_back("position sfen " + fen);
#endif
			list.emplace_back(go);
		}

	return list;
}

BenchmarkSetup setup_benchmark(std::istream& is) {

	
	

	
	

	static constexpr int TT_SIZE_PER_THREAD = 128;

	static constexpr int DEFAULT_DURATION_S = 150;

	BenchmarkSetup setup{};

	
	int desiredTimeS;

	if (!(is >> setup.threads))
		setup.threads = get_hardware_concurrency();
	else
		setup.originalInvocation += std::to_string(setup.threads);

	if (!(is >> setup.ttSize))
		setup.ttSize = TT_SIZE_PER_THREAD * setup.threads;
	else
		setup.originalInvocation += " " + std::to_string(setup.ttSize);

	if (!(is >> desiredTimeS))
		desiredTimeS = DEFAULT_DURATION_S;
	else
		setup.originalInvocation += " " + std::to_string(desiredTimeS);

	setup.filledInvocation += std::to_string(setup.threads) + " " + std::to_string(setup.ttSize)
		+ " " + std::to_string(desiredTimeS);

	auto getCorrectedTime = [&](int ply) {
		
		
		
		
		
		return 50000.0 / (static_cast<double>(ply) + 15.0);
		};

	float totalTime = 0;
	for (const auto& game : BenchmarkPositions)
	{
		int ply = 1;
		for (int i = 0; i < static_cast<int>(game.size()); ++i)
		{
			const float correctedTime = float(getCorrectedTime(ply));
			totalTime += correctedTime;
			ply += 1;
		}
	}

	float timeScaleFactor = static_cast<float>(desiredTimeS * 1000) / totalTime;

	for (const auto& game : BenchmarkPositions)
	{
		setup.commands.emplace_back("ucinewgame");
		int ply = 1;
		for (const std::string& fen : game)
		{
#if STOCKFISH
			setup.commands.emplace_back("position fen " + fen);
#else
            setup.commands.emplace_back("position sfen " + fen);
#endif
			const int correctedTime = static_cast<int>(getCorrectedTime(ply) * timeScaleFactor);
			setup.commands.emplace_back("go movetime " + std::to_string(correctedTime));

			ply += 1;
		}
	}

	return setup;
}

} 
