#include "../../config.h"

#if defined(YANEURAOU_ENGINE_DEEP)

#include "Node.h"
#include "UctSearch.h"
#include "dlshogi_searcher.h"
#include "PrintInfo.h"
#include "FukauraOuEngine.h"
#include "misc/fastmath.h"  

#include "../../thread.h"
#include "../../usi.h"
#include "../../mate/mate.h"

#include <limits>           
#include <sstream>









using namespace std;

#if defined(LOG_PRINT)

struct MoveIntFloat
{
	MoveIntFloat(Move move, int label, float nnrate) : move(move),label(label),nnrate(nnrate){}

	bool operator < (const MoveIntFloat& rhs) const {
		return nnrate < rhs.nnrate;
	}

	std::string to_string()
	{
		return to_usi_string(move) + " " + std::to_string(label) + " " + std::to_string(nnrate);
		
	}

	Move move;
	int label;
	float nnrate;
};

struct MoveMoveLabel
{
	MoveMoveLabel(Move move, int label) : move(move), label(label) {}
	bool operator < (const MoveMoveLabel& rhs) const {
		return label < rhs.label;
	}

	Move move;
	int label;
};

class MyLogger
{
public:
	MyLogger() {
		fs.open("my_log_yane.txt");
	}

	~MyLogger()
	{
		fs.close();
	}

	void print(const std::string& s)
	{
		fs << s << std::endl;
	}
	void print(std::vector<MoveIntFloat>& m)
	{
		

		for (auto ml : m)
			fs << ml.to_string() << std::endl;

		fs.flush();
	}

	std::ofstream fs;
};
MyLogger logger;
#endif



using namespace YaneuraOu::Eval::dlshogi;

#define LOCK_EXPAND grp->get_dlsearcher()->mutex_expand.lock();
#define UNLOCK_EXPAND grp->get_dlsearcher()->mutex_expand.unlock();

namespace dlshogi
{

template <typename T>
inline void atomic_fetch_add(std::atomic<T>* obj, T arg) {
	T expected = obj->load();
	while (!atomic_compare_exchange_weak(obj, &expected, expected + arg))
		;
}


inline void AddVirtualLoss(ChildNode* child, Node* current)
{
	current->move_count += VIRTUAL_LOSS;
	child  ->move_count += VIRTUAL_LOSS;
}


inline void SubVirtualLoss(ChildNode* child, Node* current)
{
	current->move_count -= VIRTUAL_LOSS;
	child  ->move_count -= VIRTUAL_LOSS;
}




inline void UpdateResult(ChildNode* child, float result, Node* current)
{
	atomic_fetch_add(&current->win, (WinType)result);
	if constexpr (VIRTUAL_LOSS != 1) current->move_count += 1 - VIRTUAL_LOSS;
	atomic_fetch_add(&child  ->win, (WinType)result);
	if constexpr (VIRTUAL_LOSS != 1) child  ->move_count += 1 - VIRTUAL_LOSS;
}













void UctSearcherGroup::Initialize(const std::string& model_path, const std::string& model_architecture,
                                  const int new_thread, const int gpu_id, const int policy_value_batch_maxsize)
{
	
	
	this->gpu_id = gpu_id;
	const bool architecture_changed = this->model_architecture != model_architecture;
#if defined(TENSOR_RT)
	const bool slot_capacity_changed = nn && nn->slot_capacity() < new_thread;
#else
	constexpr bool slot_capacity_changed = false;
#endif

	
	
	
	if (this->model_path != model_path || architecture_changed
	    || policy_value_batch_maxsize != this->policy_value_batch_maxsize
	    || slot_capacity_changed)
	{
		std::lock_guard<std::mutex> lk(mutex_gpu);

		
		if (nn)
			nn.reset();

		nn = NN::build_nn(model_path, gpu_id, policy_value_batch_maxsize, new_thread);

		
		this->model_path = model_path;
		this->model_architecture = model_architecture;
	}
	nn->prepare_slots(new_thread);

	
	if (searchers.size() != (size_t)new_thread || architecture_changed
	    || policy_value_batch_maxsize != this->policy_value_batch_maxsize
	    || slot_capacity_changed)
	{
		searchers.clear();
		searchers.reserve(new_thread); 

		for (int i = 0; i < new_thread; ++i)
			searchers.emplace_back(this, i, policy_value_batch_maxsize);

		this->policy_value_batch_maxsize = policy_value_batch_maxsize;
		this->threads = new_thread;
	}

	for (int i = 0; i < new_thread; ++i) {
		searchers[i].DummyForward();
	}
}


#if 0

void
UCTSearcherGroup::Run()
{
	if (threads > 0) {
		
		for (int i = 0; i < threads; i++) {
			searchers[i].Run();
		}
	}
}


void
UCTSearcherGroup::Join()
{
	if (threads > 0) {
		
		for (int i = 0; i < threads; i++) {
			searchers[i].Join();
		}
	}
}

#ifdef THREAD_POOL

void
UCTSearcherGroup::Term()
{
	if (threads > 0) {
		
		for (int i = 0; i < threads; i++) {
			searchers[i].Term();
		}
	}
}
#endif

#endif






NodeTree* UctSearcher::get_node_tree() const { return grp->get_dlsearcher()->get_node_tree(); }


void UctSearcher::QueuingNode(const Position *pos, Node* node, float* value_win)
{
#if defined(LOG_PRINT)
	logger.print("sfen "+pos->sfen(0));
#endif

	
	

	

	
	

	make_input_features(*pos, current_policy_value_batch_index, packed_features1, packed_features2);

	
	policy_value_batch[current_policy_value_batch_index] = { node, pos->side_to_move() ,
#if defined(USE_POLICY_BOOK)
		pos->hash_key() ,
#endif
		value_win};

#ifdef MAKE_BOOK
	policy_value_book_key[current_policy_value_batch_index] = Book::bookKey(*pos);
#endif

	current_policy_value_batch_index++;
	
}




void UctSearcher::InitMateSearcher(const SearchOptions& options)
{
	

	
	
	
	
	mate_solver.alloc(1);
}



void UctSearcher::SetMateSearcher(const SearchOptions& options)
{
	

	
	mate_solver.set_max_game_ply(options.max_moves_to_draw);

}





void UctSearcher::DummyForward()
{
	
	auto dummy_sfen = [](u32 value) {
		Piece board[SQ_NB];
		for (size_t sq = SQ_ZERO; sq < SQ_NB; ++sq)
			board[sq] = NO_PIECE;
		board[FILE_1 | ((((value >>  0) & 1) == 0) ? RANK_9 : RANK_8)] = B_LANCE;
		board[FILE_9 | ((((value >>  1) & 1) == 0) ? RANK_9 : RANK_8)] = B_LANCE;
		board[FILE_2 | RANK_9] = B_KNIGHT;
		board[FILE_8 | RANK_9] = B_KNIGHT;
		board[FILE_3 | ((((value >>  2) & 1) == 0) ? RANK_9 : RANK_8)] = B_SILVER;
		board[FILE_7 | ((((value >>  3) & 1) == 0) ? RANK_9 : RANK_8)] = B_SILVER;
		board[FILE_4 | ((((value >>  4) & 1) == 0) ? RANK_9 : RANK_8)] = B_GOLD;
		board[FILE_6 | ((((value >>  5) & 1) == 0) ? RANK_9 : RANK_8)] = B_GOLD;
		board[FILE_5 | ((((value >>  6) & 1) == 0) ? RANK_9 : RANK_8)] = B_KING;
		board[FILE_2 | RANK_8] = B_ROOK;
		board[FILE_8 | RANK_8] = B_BISHOP;
		board[FILE_1 | ((((value >>  7) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_9 | ((((value >>  8) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_2 | ((((value >>  9) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_8 | ((((value >> 10) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_3 | ((((value >> 11) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_7 | ((((value >> 12) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_4 | ((((value >> 13) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_6 | ((((value >> 14) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_5 | ((((value >> 15) & 1) == 0) ? RANK_7 : RANK_6)] = B_PAWN;
		board[FILE_9 | ((((value >> 16) & 1) == 0) ? RANK_1 : RANK_2)] = W_LANCE;
		board[FILE_1 | ((((value >> 17) & 1) == 0) ? RANK_1 : RANK_2)] = W_LANCE;
		board[FILE_8 | RANK_1] = W_KNIGHT;
		board[FILE_2 | RANK_1] = W_KNIGHT;
		board[FILE_7 | ((((value >> 18) & 1) == 0) ? RANK_1 : RANK_2)] = W_SILVER;
		board[FILE_3 | ((((value >> 19) & 1) == 0) ? RANK_1 : RANK_2)] = W_SILVER;
		board[FILE_6 | ((((value >> 20) & 1) == 0) ? RANK_1 : RANK_2)] = W_GOLD;
		board[FILE_4 | ((((value >> 21) & 1) == 0) ? RANK_1 : RANK_2)] = W_GOLD;
		board[FILE_5 | ((((value >> 22) & 1) == 0) ? RANK_1 : RANK_2)] = W_KING;
		board[FILE_8 | RANK_2] = W_ROOK;
		board[FILE_2 | RANK_2] = W_BISHOP;
		board[FILE_9 | ((((value >> 23) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_1 | ((((value >> 24) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_8 | ((((value >> 25) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_2 | ((((value >> 26) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_7 | ((((value >> 27) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_3 | ((((value >> 28) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_6 | ((((value >> 29) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_4 | ((((value >> 30) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;
		board[FILE_5 | ((((value >> 31) & 1) == 0) ? RANK_3 : RANK_4)] = W_PAWN;

		std::ostringstream ss;
		int emptyCnt;
		for (Rank r = RANK_1; r <= RANK_9; ++r)
		{
			for (File f = FILE_9; f >= FILE_1; --f)
			{
				for (emptyCnt = 0; f >= FILE_1 && board[f | r] == NO_PIECE; --f)
					++emptyCnt;
				if (emptyCnt)
					ss << emptyCnt;
				if (f >= FILE_1)
					ss << board[f | r];
			}
			if (r < RANK_9)
				ss << '/';
		}
		ss << " b - " << 1;

		return ss.str();
	};

	if (policy_value_batch_maxsize < 1)
		return;

	
	TimePoint tpforwardbegin = now();
	
	Position pos;
	StateInfo si;
	for (int i = 0; i < policy_value_batch_maxsize; ++i) {
		pos.set(dummy_sfen((u32)i), &si);
		make_input_features(pos, i, packed_features1, packed_features2);
	}
	
	grp->set_device();
	
	grp->nn_forward(thread_id, policy_value_batch_maxsize, packed_features1, packed_features2, features1, features2, y1, y2);
	grp->nn_forward(thread_id, 1, packed_features1, packed_features2, features1, features2, y1, y2);
	
	TimePoint tpforwardend = now();

	sync_cout << "info string engine forward test. batch_size = " << policy_value_batch_maxsize << ", Processing time = " << tpforwardend - tpforwardbegin << "ms." << sync_endl;
}



void UctSearcher::ParallelUctSearchStart(const Position& rootPos)
{
	
	grp->set_device();

	
	auto& options = grp->get_dlsearcher()->search_options;
	SetMateSearcher(options);

	
	ParallelUctSearch(rootPos);
}




void UctSearcher::ParallelUctSearch(const Position& rootPos) {
    DlshogiSearcher* ds            = grp->get_dlsearcher();
    auto&            search_limits = ds->search_limits;
    auto             stop = [&]() { return ds->engine.threads.stop || search_limits.interruption; };

    

    Node* current_root = get_node_tree()->GetCurrentHead();

    
    LOCK_EXPAND;
    if (!current_root->IsEvaled())
    {
        current_policy_value_batch_index = 0;
        float value_win;  
        QueuingNode(&rootPos, current_root, &value_win);
        EvalNode();
    }
    UNLOCK_EXPAND;

    
    vector<NodeVisitor>      visitor_batch;
    vector<NodeTrajectories> trajectories_batch_discarded;
    visitor_batch.reserve(policy_value_batch_maxsize);
    trajectories_batch_discarded.reserve(policy_value_batch_maxsize);

    
    while (!stop())
    {
        visitor_batch.clear();
        trajectories_batch_discarded.clear();
        current_policy_value_batch_index = 0;

        
        
        
        
        for (int i = 0; i < policy_value_batch_maxsize && !stop(); i++)
        {

            

            
            
            Position pos;
            std::memcpy(&pos, &rootPos, sizeof(Position));

            
            visitor_batch.emplace_back();
            const float result = UctSearch(&pos, nullptr, current_root, visitor_batch.back());

            if (result != DISCARDED)
            {
                atomic_fetch_add(&search_limits.nodes_searched, (NodeCountType) 1);
                
                
            }
            else
            {
                
                trajectories_batch_discarded.emplace_back(
                  std::move(visitor_batch.back().trajectories));
            }

            
            if (result == DISCARDED || result != QUEUING)
            {
                visitor_batch.pop_back();
            }
        }

        
        EvalNode();

        
        for (auto& trajectories : trajectories_batch_discarded)
        {
            for (auto it = trajectories.rbegin(); it != trajectories.rend(); ++it)
            {
                NodeTrajectory&    current_next = *it;
                Node*              current      = current_next.node;
                ChildNode*         uct_child    = current->child.get();
                const ChildNumType next_index   = current_next.index;

                SubVirtualLoss(&uct_child[next_index], current);
            }
        }

        
        
        

        
        
        for (auto& visitor : visitor_batch)
        {
            
            float result = 1.0f - visitor.value_win;

            auto& trajectories = visitor.trajectories;
            for (auto it = trajectories.rbegin(); it != trajectories.rend(); ++it)
            {
                auto&              current_next = *it;
                Node*              current      = current_next.node;
                const ChildNumType next_index   = current_next.index;
                ChildNode*         uct_child    = current->child.get();

                UpdateResult(&uct_child[next_index], result, current);

                
                result = 1.0f - result;
            }
        }
    }
}












float UctSearcher::UctSearch(Position* pos, ChildNode* parent , Node* current, NodeVisitor& visitor)
{
#if defined(USE_POLICY_BOOK)
	
	
	if (parent!=nullptr && current->policy_book_value != FLT_MAX)
		return 1.0f - current->policy_book_value;
#endif

	auto ds = grp->get_dlsearcher();
	auto& options = ds->search_options;

	

	float result;

	
	ChildNode* uct_child = current->child.get();

	
	auto& trajectories = visitor.trajectories;

	
	
	auto& mutex = ds->get_node_mutex(pos);
	mutex.lock();

	
	if (!current->child_nodes) current->InitChildNodes();

	
	const ChildNumType next_index = SelectMaxUcbChild(parent, current);

#if defined(LOG_PRINT)
	logger.print("do_move = " + to_usi_string(uct_child[next_index].move));
#endif

	
	StateInfo st;
	pos->do_move(uct_child[next_index].move, st);

	
	
	
	AddVirtualLoss(&uct_child[next_index], current);

	
	
	if (!current->child_nodes[next_index]) {
		
		Node* child_node = current->CreateChildNode(next_index);
		

		

		
		mutex.unlock();
		
		
		

		
		trajectories.emplace_back(current, next_index);

		

		RepetitionState rep;

		

		
		
		
		
		
		
		
		
		
		

		if (options.max_moves_to_draw < pos->game_ply())
			rep = pos->is_mated() ? REPETITION_LOSE  : REPETITION_DRAW;
		else
			rep = pos->is_repetition(16);

		switch (rep)
		{
			case REPETITION_WIN     : 
			case REPETITION_SUPERIOR: 
				
				uct_child[next_index].SetWin();
				result = 0.0f;
				break;

			case REPETITION_LOSE    : 
			case REPETITION_INFERIOR: 
				
				uct_child[next_index].SetLose();
				result = 1.0f;
				break;

			case REPETITION_DRAW    : 
				uct_child[next_index].SetDraw();
				
				result = 1 - ds->draw_value(pos->side_to_move());
				break;

			case REPETITION_NONE    : 
			{
				

#if !defined(LOG_PRINT)

				bool isMate = false;

				

				
				if (options.leaf_dfpn_nodes_limit) {

					
					

					
					Move mate_move = mate_solver.mate_dfpn(*pos, options.leaf_dfpn_nodes_limit);
					if (mate_move == Move::null())
					{
						
						
						child_node->dfpn_proven_unsolvable = true;
					}
					else {
						isMate = mate_move.is_ok();
					}
				}
				if (!isMate)
					
					isMate = pos->DeclarationWin() != Move::none();

#else
				
				bool isMate = (pos->DeclarationWin() != Move::none());            
#endif

				
				if (isMate) {
					
					
					uct_child[next_index].SetWin();
					result = 0.0f;
				}
				else {
					
					child_node->ExpandNode(pos, options.generate_all_legal_moves);
					if (child_node->child_num == 0) {
						
						uct_child[next_index].SetLose();
						result = 1.0f;
					}
					else
					{
						
						QueuingNode(pos, child_node , &visitor.value_win);

						
						

						return QUEUING;
					}
				}

				break;
			}

			default: UNREACHABLE;
		}

		
		child_node->SetEvaled();

	}
	else {
		
		mutex.unlock();

		
		trajectories.emplace_back(current, next_index);

		Node* next_node = current->child_nodes[next_index].get();

		
		if (!next_node->IsEvaled())
			return DISCARDED;

		if (uct_child[next_index].IsWin()) {
			
			result = 0.0f;  
		}
		else if (uct_child[next_index].IsLose()) {
			
			result = 1.0f; 
		}
		
		else if (uct_child[next_index].IsDraw()) {
			
			result = 1 - ds->draw_value(pos->side_to_move());
		}
		
		else if (next_node->child_num == 0) {
			result = 1.0f; 
		}
		else {
			
			result = UctSearch(pos, &uct_child[next_index], next_node, visitor);
		}
	}

	if (result == QUEUING)
		return result;
	else if (result == DISCARDED) {
		
		
		return result;
	}

	
	
	UpdateResult(&uct_child[next_index], result, current);

	
	return 1.0f - result;
}






ChildNumType UctSearcher::SelectMaxUcbChild(ChildNode* parent, Node* current)
{
	

	auto ds = grp->get_dlsearcher();
	auto& options = ds->search_options;

	

	
	const ChildNode *uct_child = current->child.get();
	
	const ChildNumType child_num = current->child_num;

	
	ChildNumType max_child = 0;

	
	const NodeCountType sum = current->move_count;

	
	ASSERT_LV3(sum != NOT_EXPANDED);

	
	const WinType sum_win = current->win;

	float q, u, max_value;

	max_value = -FLT_MAX;

	
	
	const float sqrt_sum = sqrtf((float)sum);
	const float c = parent == nullptr ?
		FastLog((sum + options.c_base_root + 1.0f) / options.c_base_root) + options.c_init_root :
		FastLog((sum + options.c_base + 1.0f) / options.c_base) + options.c_init;
	const float fpu_reduction = (parent == nullptr ? options.c_fpu_reduction_root : options.c_fpu_reduction) * sqrtf(current->visited_nnrate);
	const float parent_q = sum_win > 0 ? std::max(0.0f, (float)(sum_win / sum) - fpu_reduction) : 0.0f;
	const float init_u = sum == 0 ? 1.0f : sqrt_sum;

	
	
	
	u32 and_move = 0xffffffff;

	
	for (ChildNumType i = 0; i < child_num; i++) {

		Move move = uct_child[i].move;

		
		if (ChildNode::IsMoveWin(move))
			continue;

		if (ChildNode::IsMoveLose(move)) {
			
			if (parent != nullptr)
				parent->SetWin();
			return i;
		}

		

		
		
		

		
		

		and_move &= move.to_u32();

		

		const WinType       win        = uct_child[i].win;
		const NodeCountType move_count = uct_child[i].move_count;

		if (move_count == 0) {
			
			
			q = parent_q;
			u = init_u;
		}
		else {
			q = (float)(win / move_count);
			u = sqrt_sum / (1 + move_count);
		}

		
		const float rate = uct_child[i].nnrate;

		
		
		
		
		
		
		
		
		
		
		
		
		
		

		const float ucb_value = q + c * u * rate;

		if (ucb_value > max_value) {
			max_value = ucb_value;
			max_child = i;
		}
	}

	if (ChildNode::IsMoveWin((Move)and_move)) {
		
		
		if (parent != nullptr)
			parent->SetLose();

	} else if (ChildNode::IsMoveDraw((Move)and_move)) {

		
		
		if (parent != nullptr)
			parent->SetDraw();

	} else {

		
		atomic_fetch_add(&current->visited_nnrate, uct_child[max_child].nnrate);
	}

	return max_child;
}



void UctSearcher::EvalNode() {
    
    if (current_policy_value_batch_index == 0)
        return;

    
    const int policy_value_batch_size = current_policy_value_batch_index;
    auto      ds                      = grp->get_dlsearcher();

#if defined(LOG_PRINT)
    
    std::stringstream ss;
    for (size_t i = 0; i < input1_element_count(1); ++i)
        ss << features1[i] << ",";
    ss << endl << "Input2" << endl;
    for (size_t i = 0; i < input2_element_count(1); ++i)
        ss << features2[i] << ",";
    logger.print(ss.str());
#endif

    
    
    grp->nn_forward(thread_id, policy_value_batch_size, packed_features1, packed_features2, features1,
                    features2, y1, y2);

    

    const NN_Output_Policy* logits = y1;
    const NN_Output_Value*  value  = y2;

    for (int i = 0; i < policy_value_batch_size; i++, logits++, value++)
    {
        Node*              node      = policy_value_batch[i].node;
        const Color        color     = policy_value_batch[i].color;
        const ChildNumType child_num = node->child_num;
        ChildNode*         uct_child = node->child.get();

#if defined(USE_POLICY_BOOK)
        HASH_KEY key               = policy_value_batch[i].key;
        auto*    policy_book_entry = grp->get_dlsearcher()->policy_book.probe_policy_book(key);
#endif

        
        std::vector<float> legal_move_probabilities;
        
        legal_move_probabilities.reserve(child_num);

#if defined(LOG_PRINT)
        
        vector<int>           move_labels;
        vector<MoveMoveLabel> moves;
        for (int j = 0; j < child_num; j++)
        {
            Move      move       = uct_child[j].move;
            const int move_label = make_move_label(move, color);
            moves.emplace_back(move, move_label);
        }
        
        std::sort(moves.begin(), moves.end());
        for (int j = 0; j < child_num; j++)
        {
            uct_child[j].move = moves[j].move;
            move_labels.push_back(make_move_label(moves[j].move, color));
        }
#endif

        for (ChildNumType j = 0; j < child_num; j++)
        {
            Move        move       = uct_child[j].move;
            const int   move_label = make_move_label(move, color);
            const float logit      = (*logits)[move_label];
            legal_move_probabilities.emplace_back(logit);

            
            
        }

        
        softmax_temperature_with_normalize(legal_move_probabilities);

#if !defined(USE_POLICY_BOOK)
        for (ChildNumType j = 0; j < child_num; j++)
        {
            uct_child[j].nnrate = legal_move_probabilities[j];
        }
#else
        if (policy_book_entry == nullptr)
        {
            for (ChildNumType j = 0; j < child_num; j++)
            {
                uct_child[j].nnrate = legal_move_probabilities[j];
            }
        }
        else
        {
            

            
            float v = policy_book_entry->value;
            if (v != FLT_MAX)
                node->policy_book_value = v;

            u32    total = 0;
            size_t k1;
            for (k1 = 0; k1 < POLICY_BOOK_NUM; ++k1)
            {
                if (policy_book_entry->move_freq[k1].move16 == Move16::none())
                    break;
                total += policy_book_entry->move_freq[k1].freq;
            }
            

            
            
            
            
            
            
            
            
            
            
            
            

            float book_policy_ratio = 0.7f + 0.1f * std::clamp(log10f(float(total)), 0.0f, 3.0f);

            for (ChildNumType j = 0; j < child_num; j++)
            {

                uct_child[j].nnrate = (1.0f - book_policy_ratio) * legal_move_probabilities[j];

                
                for (size_t k2 = 0; k2 < k1; ++k2)
                {
                    if (uct_child[j].move.to_move16() == policy_book_entry->move_freq[k2].move16)
                    {
                        uct_child[j].nnrate +=
                          book_policy_ratio * policy_book_entry->move_freq[k2].freq / total;
                        break;
                    }
                }
            }
        }
#endif

        
        *policy_value_batch[i].value_win = *value;

#if defined(LOG_PRINT)
        std::vector<MoveIntFloat> m;
        for (int j = 0; j < child_num; ++j)
            m.emplace_back(uct_child[j].move, move_labels[j], uct_child[j].nnrate);
        logger.print(m);
        logger.print("NN value = " + std::to_string(*value));
        static int visit_count = 0;
        ++visit_count;
        logger.print("visit = " + std::to_string(visit_count));
#endif


        
#ifdef MAKE_BOOK
        
        constexpr float alpha = 0.5f;
        const Key&      key   = policy_value_book_key[i];
        const auto      itr   = bookMap.find(key);
        if (itr != bookMap.end())
        {
            const auto& entries = itr->second;
            
            std::map<u16, u16> count_map;
            int                sum = 0;
            for (const auto& entry : entries)
            {
                count_map.insert(std::make_pair(entry.fromToPro, entry.count));
                sum += entry.count;
            }
            
            for (int j = 0; j < child_num; ++j)
            {
                const Move& move     = uct_child[j].move;
                const auto  itr2     = count_map.find((u16) move.proFromAndTo());
                const float bookrate = itr2 != count_map.end() ? (float) itr2->second / sum : 0.0f;
                uct_child[j].nnrate  = (1.0f - alpha) * uct_child[j].nnrate + alpha * bookrate;
            }
        }
#endif
        node->SetEvaled();
    }
}



unsigned int select_max_child_node(const Node* uct_node)
{
	const ChildNode* uct_child = uct_node->child.get();

	unsigned int select_index = 0;
	NodeCountType max_count = 0;
	const int child_num = uct_node->child_num;
	NodeCountType child_win_count = 0;
	NodeCountType child_lose_count = 0;

	for (int i = 0; i < child_num; i++) {
		if (uct_child[i].IsWin()) {
			
			if (child_win_count == NodeCountType(i) && uct_child[i].move_count > max_count) {
				
				select_index = i;
				max_count = uct_child[i].move_count;
			}
			child_win_count++;
			continue;
		}
		else if (uct_child[i].IsLose()) {
			
			if (child_lose_count == 0 || uct_child[i].move_count > max_count) {
				
				select_index = i;
				max_count = uct_child[i].move_count;
			}
			child_lose_count++;
			continue;
		}

		if (child_lose_count == 0 && uct_child[i].move_count > max_count) {
			select_index = i;
			max_count = uct_child[i].move_count;
		}
	}

	return select_index;
}



void select_nth_child_node(const Node* uct_node, int n, int(&indices)[MAX_MOVES])
{
	const ChildNode* uct_child = uct_node->child.get();
	const int child_num = uct_node->child_num;

	int index = 0;
	for (int i = 0; i < child_num; i++) {
		
		
		if (!(uct_child[i].IsWin() || uct_child[i].IsLose() || uct_child[i].move_count < 10))
			indices[index++] = i;
	}
	std::partial_sort(indices, indices + std::min(n, index) , indices + index, [&uct_child](int a, int b) {
		return uct_child[a].move_count > uct_child[b].move_count;
		});

	
	for (int i = index; i < n; ++i)
		indices[i] = -1;
}

} 

#endif 
