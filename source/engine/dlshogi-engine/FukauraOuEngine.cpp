#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "FukauraOuEngine.h"

#include "../../position.h"
#include "../../usi.h"
#include "../../thread.h"
#include "../../misc.h"

#include "dlshogi_searcher.h"
#include "UctSearch.h"

#include "../../eval/deep/nn.h"
#include "../../eval/deep/nn_types.h"

using namespace YaneuraOu;

namespace dlshogi {


FukauraOuEngine::FukauraOuEngine() :
    searcher(*this){}

void FukauraOuEngine::add_nn_options()
{
    OptionsMap& options = get_options();


    options.add("EvalDir", Option("eval"));

	options.add("Max_GPU", Option(1, 1, 1024));

	options.add("Disabled_GPU", Option(""));

    options.add("UCT_Threads", Option(2, 0, 256));

#if defined(COREML)
    options.add("DNN_Model", Option(R"(model.mlmodel)"));
#else
    options.add("DNN_Model", Option(R"(model.onnx)"));
#endif

    options.add("ModelArchitecture",
                Option(Eval::dlshogi::model_architecture_names(),
                       Eval::dlshogi::DEFAULT_MODEL_ARCHITECTURE));

#if defined(TENSOR_RT)
    options.add("DNN_Batch_Size", Option(128, 1, 1024));
#elif defined(ONNXRUNTIME)
    options.add("DNN_Batch_Size", Option(32, 1, 1024));
#elif defined(COREML)
    options.add("DNN_Batch_Size", Option(8, 1, 1024));
#endif
}

void FukauraOuEngine::add_options() {

    Engine::add_base_options();

    searcher.add_options(options);

    add_nn_options();

    searcher.book.add_options(options);
}

std::vector<int> FukauraOuEngine::get_thread_settings() {

    int option_max_gpu = int(options["Max_GPU"]);

    int device_count = Eval::dlshogi::NN::get_device_count();

    if (device_count == -1)
        device_count = option_max_gpu;

    int max_gpu = std::min(option_max_gpu, device_count);

    int thread_num = int(options["UCT_Threads"]);

    std::vector<int> thread_settings;

    thread_settings.assign(max_gpu, thread_num);

    for (auto&& disabled : split(std::string(options["Disabled_GPU"]), ","))
    {
        int d = StringExtension::to_int(std::string(disabled), 0);
        if (d == 0)
            continue;

        if (1 <= d && d <= max_gpu)
            thread_settings[d - 1] = 0;
    }

    return thread_settings;
}

void FukauraOuEngine::init_gpu() {

    auto& options = get_options();

    auto thread_settings = get_thread_settings();

    int dnn_batch_size = int(options["DNN_Batch_Size"]);

    auto eval_dir      = options["EvalDir"];
    auto abs_eval_path = Path::Combine(Directory::GetBinaryFolder(), eval_dir);
    auto model_name    = options["DNN_Model"];
    auto model_path    = Path::Combine(abs_eval_path, model_name);
    std::string model_architecture = options["ModelArchitecture"];

    if (!Eval::dlshogi::set_model_architecture(model_architecture))
    {
        sync_cout << "Error! : unknown ModelArchitecture = " << model_architecture << sync_endl;
        Tools::exit();
    }

    const auto& input_spec = Eval::dlshogi::input_feature_spec();
    sync_cout << "info string ModelArchitecture = " << input_spec.architecture
              << ", input1 = " << input_spec.features1_channels
              << ", input2 = " << input_spec.features2_channels
              << sync_endl;

    if (!Path::Exists(model_path))
    {
        sync_cout << "Error! : " << model_path << " file not found" << sync_endl;
        Tools::exit();
    }

    searcher.InitGPU(model_path, model_architecture, thread_settings, dnn_batch_size);
}


void FukauraOuEngine::isready() {

    Eval::dlshogi::init();


    searcher.book.read_book();


	init_gpu();


	searcher.InitializeUctSearch();

	searcher.SetPvMateSearch(int(options["PV_Mate_Search_Threads"]), int(options["PV_Mate_Search_Nodes"]));


	Engine::isready();
}

void FukauraOuEngine::set_ponderhit(bool b) {

    searcher.search_limits.time_manager.ponderhitTime = now();

	searcher.search_limits.ponder           = b;
}

std::string FukauraOuEngine::get_engine_name() const { return "FukauraOu"; }

std::string FukauraOuEngine::get_engine_author() const { return "Tadao Yamaoka , yaneurao"; }



FukauraOuWorker::FukauraOuWorker(Search::SharedState&     sharedState,
                                const Search::ThreadIds& ids,
                                DlshogiSearcher&          searcher,
                                FukauraOuEngine&          engine) :
    Worker(sharedState, ids),
    searcher(searcher),
    engine(engine) {}

void FukauraOuWorker::pre_start_searching() {

    rootPos.set_ekr(searcher.search_options.enteringKingRule);

	if (is_mainthread())
        searcher.search_limits.ponder = limits.ponderMode;
}


void FukauraOuWorker::start_searching()
{
    if (!is_mainthread())
    {
        parallel_search();
        return;
    }

    searcher.search_limits.root_color = rootPos.side_to_move();

    searcher.SetLimits(rootPos, limits);

    if (engine.game_root_sfen.empty())
        engine.game_root_sfen = StartSFEN;

    Move ponderMove;
    Move move = searcher.UctSearchGenmove(rootPos, engine.game_root_sfen,
                                          engine.moves_from_game_root, ponderMove);


    while (!threads.stop && (searcher.search_limits.ponder || limits.infinite))
    {
        Tools::sleep(1);

    }


    std::string bestmove = to_usi_string(move);
    std::string ponder;
    if (ponderMove)
        ponder = to_usi_string(ponderMove);

    engine.updateContext.onBestmove(bestmove, ponder);
}

void FukauraOuWorker::parallel_search()
{

	
	searcher.parallel_search(rootPos, threadIdx);
}

FukauraOuWorker::~FukauraOuWorker()
{
}

#if 0

void GetSearchResult(std::vector < std::pair<Move, float>>& result)
{
	Node* root_node = searcher.search_limits.current_root;

	ChildNumType num = root_node->child_num;

	result.clear();
	result.reserve(num);

	for (ChildNumType i = 0; i < num; ++i)
	{
		auto& child = root_node->child[i];
		Move m = child.move;
		float win = child.move_count == 0 ? child.nnrate : (float)child.win / child.move_count;
		result.emplace_back(std::pair<Move, float>(m, win));
	}
}

struct TopVisited
{
	TopVisited(size_t n_) : n(n_)
	{
		tops.reserve(n);
	}

	NodeCountType nth_nodes() const {
		if (tops.size() == 0)
			return 0;
		return tops.back();
	}

	void append(NodeCountType m)
	{
		if (tops.size() < n)
			tops.push_back(0);
			
		for(size_t i = tops.size() - 1 ; ; --i)
		{
			auto c = (i == 0) ?  std::numeric_limits<NodeCountType>::max() : tops[i - 1];

			if (c >= m)
			{
				ASSERT_LV3(i < tops.size() );
				tops[i] = m;
				break;
			}

			tops[i] = c;
		}
	}

	size_t size() const { return tops.size(); }

private:
	std::vector<NodeCountType> tops;

	size_t n;
};

void dfs_for_node_visited(Node* node, TopVisited& tv , int ply, bool same_color)
{
	if (   ply != 0
		&& (ply % 2) == (same_color ? 0 : 1)
		&& node->move_count > tv.nth_nodes()
		)
		tv.append(node->move_count);

	ChildNumType num = node->child_num;
	for (int i = 0; i < num; ++i)
	{
		auto& child = node->child[i];
		NodeCountType move_count = child.move_count;

		if (move_count > tv.nth_nodes())
			dfs_for_node_visited(node->child_nodes[i].get(), tv , ply + 1, same_color);
	}
}

void dfs_for_sfen(Node* node, TopVisited& tv , int ply , bool same_color, SfenNodeList& snlist , std::vector<Move>& pv)
{
	if (   ply != 0
		&& (ply % 2) == (same_color ? 0 : 1)
		&& node->move_count >= tv.nth_nodes()
		&& snlist.size() < tv.size()
		)
	{
		std::string pv_string;

		for(auto m : pv)
			pv_string += " " + to_usi_string(m);
		snlist.emplace_back(SfenNode(pv_string, node->move_count));
	}

	ChildNumType num = node->child_num;
	for (int i = 0; i < num; ++i)
	{
		auto& child = node->child[i];

		NodeCountType move_count = child.move_count;
		Move          m          = child.move;

		if (   move_count >= tv.nth_nodes()
			&& node->child_nodes          != nullptr
			&& node->child_nodes[i].get() != nullptr)
		{
			pv.push_back(m);
			dfs_for_sfen(node->child_nodes[i].get(), tv , ply + 1, same_color, snlist, pv);
			pv.pop_back();
		}
	}
}

void GetTopVisitedNodes(size_t n, SfenNodeList& snlist, bool same_color)
{
	Node* root_node = searcher.search_limits.current_root;


	TopVisited tv(n);
	dfs_for_node_visited(root_node, tv , 0, same_color);


	std::vector<Move> pv;
	dfs_for_sfen        (root_node, tv , 0, same_color, snlist, pv);

	std::sort(snlist.begin(), snlist.end());

	if (snlist.size() > n)
		snlist.resize(n);
}

u64 nodes_visited()
{
	return (u64)searcher.search_limits.nodes_searched;
}

void gameover_handler(const std::string& cmd)
{
	searcher.GameOver();
}

#endif

} 

namespace {

void engine_main() {

    auto engine = std::make_unique<dlshogi::FukauraOuEngine>();

	auto usi = std::make_unique<USIEngine>();
    usi->set_engine(*engine);  
    usi->enqueue_startup_commands(CommandLine::g);

    usi->loop();
}

static YaneuraOu::EngineFuncRegister r(engine_main, "FukauraOuEngine", 0);
}


#endif 
