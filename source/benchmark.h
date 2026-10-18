#ifndef BENCHMARK_H_INCLUDED
#define BENCHMARK_H_INCLUDED

#include <iosfwd>
#include <string>
#include <vector>

namespace YaneuraOu::Benchmark {

	std::vector<std::string> setup_bench(const std::string& currentFen, std::istream& is);

	struct BenchmarkSetup {
		size_t                   ttSize;

		size_t                   threads;

		std::vector<std::string> commands;

		std::string              originalInvocation;

		std::string              filledInvocation;
	};

	BenchmarkSetup setup_benchmark(std::istream& is);

}  

#endif  
