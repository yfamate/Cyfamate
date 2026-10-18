#ifndef __UCTSEARCH_H_INCLUDED__
#define __UCTSEARCH_H_INCLUDED__

#include "../../config.h"
#if defined(YANEURAOU_ENGINE_DEEP)
#include "../../position.h"
#include "../../mate/mate.h"

#include "Node.h"
#include "PvMateSearch.h"


#include "../../eval/deep/nn_types.h"
#include "../../eval/deep/nn.h"

namespace dlshogi
{
using namespace YaneuraOu::Eval::dlshogi;

class UctSearcher;
class DlshogiSearcher;
struct SearchOptions;

class UctSearcherGroup
{
public:
	UctSearcherGroup() :  threads(0) , gpu_id(-1) , policy_value_batch_maxsize(0){}

	void Initialize(const std::string& model_path, const std::string& model_architecture,
	                const int new_thread, const int gpu_id, const int policy_value_batch_maxsize);

	void nn_forward(const int slot_id, const int batch_size, PType* p1, PType* p2, NN_Input1* x1, NN_Input2* x2, NN_Output_Policy* y1, NN_Output_Value* y2)
	{
#if !defined(UNPACK_CUDA)
		extract_input_features(batch_size, p1, p2, x1, x2);
#endif
#if defined(TENSOR_RT)
		nn->forward(slot_id, batch_size, p1, p2, x1, x2, y1, y2);
#else
		mutex_gpu.lock();
		nn->forward(batch_size, p1, p2, x1, x2, y1, y2);
		mutex_gpu.unlock();
#endif
	}

	void set_device() { nn->set_device(gpu_id); }





	template <class T>
	T* gpu_memalloc(size_t num) { return (T*)nn->alloc(sizeof(T) * num ); }

	template <class T>
	void gpu_memfree(T* t) { nn->free((void*)t); }

	DlshogiSearcher* get_dlsearcher() const { return dlshogi_searcher; }
	void set_dlsearcher(DlshogiSearcher* ds) { dlshogi_searcher = ds; }

	UctSearcher* get_uct_searcher(int n) { return &searchers[n]; }

private:

	DlshogiSearcher* dlshogi_searcher;

	int gpu_id;

	size_t threads;

	std::vector<UctSearcher> searchers;

	std::shared_ptr<Eval::dlshogi::NN> nn;

	int policy_value_batch_maxsize;

	std::mutex mutex_gpu;


	std::string model_path;

	std::string model_architecture;
};

struct NodeTrajectory {
	Node*			node;   
	ChildNumType	index;  

	NodeTrajectory(Node* node, ChildNumType index) : node(node), index(index) {}
};

typedef std::vector<NodeTrajectory> NodeTrajectories;

struct NodeVisitor {
	NodeTrajectories trajectories;

	float value_win;
};

struct BatchElement {
	Node*	node;       
	Color	color;      

#if defined(USE_POLICY_BOOK)
	HASH_KEY key;       
#endif

	float* value_win; 
};

class UctSearcher
{
public:
	UctSearcher(UctSearcherGroup* grp, const int thread_id, const int policy_value_batch_maxsize) :
		grp(grp),
		thread_id(thread_id),
		policy_value_batch_maxsize(policy_value_batch_maxsize),
		mate_solver(Mate::Dfpn::DfpnSolverType::Node16bitOrdering)
	{

		packed_features1 = grp->gpu_memalloc<PType>(packed_input1_byte_count(policy_value_batch_maxsize));
		packed_features2 = grp->gpu_memalloc<PType>(packed_input2_byte_count(policy_value_batch_maxsize));
		features1 = grp->gpu_memalloc<NN_Input1>(input1_element_count(policy_value_batch_maxsize));
		features2 = grp->gpu_memalloc<NN_Input2>(input2_element_count(policy_value_batch_maxsize));
		y1        = grp->gpu_memalloc<NN_Output_Policy>(policy_value_batch_maxsize);
		y2        = grp->gpu_memalloc<NN_Output_Value >(policy_value_batch_maxsize);

		policy_value_batch = new BatchElement[policy_value_batch_maxsize];

#ifdef MAKE_BOOK
		policy_value_book_key = new Key[policy_value_batch_maxsize];
#endif
	}

	UctSearcher(UctSearcher&& o) :
		grp(o.grp),
		thread_id(o.thread_id),
		mt(std::move(o.mt)),
		packed_features1(o.packed_features1),packed_features2(o.packed_features2),
		features1(o.features1),features2(o.features2),y1(o.y1),y2(o.y2),
		mate_solver(std::move(o.mate_solver))
	{
		o.packed_features1 = nullptr;
		o.packed_features2 = nullptr;
		o.features1 = nullptr;
		o.features2 = nullptr;
		o.y1 = nullptr;
		o.y2 = nullptr;
		policy_value_batch = nullptr;
	}

	~UctSearcher() {
		if (features1) 
		{
			grp->gpu_memfree<PType           >(packed_features1);
			grp->gpu_memfree<PType           >(packed_features2);
			grp->gpu_memfree<NN_Input1       >(features1);
			grp->gpu_memfree<NN_Input2       >(features2);
			grp->gpu_memfree<NN_Output_Policy>(y1);
			grp->gpu_memfree<NN_Output_Value >(y2);

			delete[] policy_value_batch;
		}
		}



	void ParallelUctSearchStart(const Position& rootPos);

	void InitMateSearcher(const SearchOptions& options);

	void SetMateSearcher(const SearchOptions& options);

	void DummyForward();

private:
	void ParallelUctSearch(const Position& rootPos);

	float UctSearch(Position* pos, ChildNode* parent, Node* current, NodeVisitor& visitor);

	ChildNumType SelectMaxUcbChild(ChildNode* parent, Node* current);

	void QueuingNode(const Position* pos, Node* node, float* value_win);

	void EvalNode();

	UctSearcherGroup* grp;

	int thread_id;

	PRNG mt;


	int policy_value_batch_maxsize;

	Eval::dlshogi::PType* packed_features1;
	Eval::dlshogi::PType* packed_features2;
	Eval::dlshogi::NN_Input1* features1;
	Eval::dlshogi::NN_Input2* features2;

	Eval::dlshogi::NN_Output_Policy* y1;
	Eval::dlshogi::NN_Output_Value * y2;

	BatchElement* policy_value_batch;

#ifdef MAKE_BOOK
	Key* policy_value_book_key;
#endif

	int current_policy_value_batch_index;

	NodeTree* get_node_tree() const;


	Mate::Dfpn::MateDfpnSolver mate_solver;
};

unsigned int select_max_child_node(const Node* uct_node);

void select_nth_child_node(const Node* uct_node, int n , int (&indices)[MAX_MOVES]);

} 

#endif

#endif 
