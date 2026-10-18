#include "search.h"
#include "thread.h"
#include "tt.h"
#include "position.h"
#include "usi.h"
#include "misc.h"

using namespace YaneuraOu;

namespace
{
	void print_file(const std::string& path)
	{
		SystemIO::TextReader reader;
		if (reader.Open(path).is_not_ok())
			return;

		std::string line;
		while (reader.ReadLine(line).is_ok())
			sync_cout << line << sync_endl;
	}
}


#include "cyf_csa.h"   
static int cyf_zyf_child_entry(int argc, char* argv[]);
int main(int argc, char* argv[])
{
    if(argc>1 && std::string(argv[1])=="--cyf-zyf-worker")return cyf_zyf_child_entry(argc,argv);
	const bool cyf_csa_mode = CyfCsa::start_if_requested(argc, argv);
	CommandLine::g.set_arg(cyf_csa_mode ? 1 : argc, argv);

	#if 0
	#endif

	print_file("startup_info.txt");


	Bitboards::init();
	Position::init();

	YaneuraOu::run_engine_entry();

	return 0;
}
