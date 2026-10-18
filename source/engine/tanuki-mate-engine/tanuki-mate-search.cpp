#include "../../config.h"
#if defined(TANUKI_MATE_ENGINE)

#include <unordered_set>
#include <cstring>	

#include "../../extra/all.h"

















































using namespace YaneuraOu;
using YaneuraOu::Search::Worker;

namespace {
	
	static const constexpr char* kMorePreciseMatePv = "MorePreciseMatePv";

	
	static const constexpr uint32_t kInfinitePnDn = 100000000;

	
	static const constexpr uint16_t kMaxDepth = MAX_PLY;

	std::vector<Move16> pv_check;
}

namespace TanukiMate {


struct MovePicker
{
	
	
	MovePicker(Position& pos, bool or_node) {
		
		
		
		endMoves = or_node ?
			generateMoves<CHECKS_ALL>(pos, moves) :
			generateMoves<EVASIONS_ALL>(pos, moves);

		
		
		endMoves = std::remove_if(moves, endMoves, [&pos](const auto& move) {
			return !pos.legal(move);
		});
	}

	
	bool empty() const {
		return moves == endMoves;
	}

	ExtMove* begin() { return moves; }
	ExtMove* end() { return endMoves; }
	const ExtMove* begin() const { return moves; }
	const ExtMove* end() const { return endMoves; }

private:
	ExtMove moves[MAX_MOVES], *endMoves = moves;
};





struct TranspositionTable {

	TranspositionTable(OptionsMap& options) : options(options) {}
	OptionsMap& options;

	
	static const constexpr uint16_t kInfiniteDepth = UINT16_MAX;

	
	static const constexpr int CacheLineSize = 64;

	
	struct TTEntry
	{
		
		uint32_t hash_high; 

		
		uint32_t pn; 
		uint32_t dn; 

		
		uint32_t num_searched; 

		
		
		uint16_t minimum_distance; 

		
		uint16_t generation;

		

		
		void init(uint32_t hash_high_ , uint16_t generation_)
		{
			hash_high = hash_high_;
			pn = 1;
			dn = 1;
			minimum_distance = kInfiniteDepth;
			num_searched = 0;
			generation = generation_;
		}
	};
	static_assert(sizeof(TTEntry) == 20, "");

	
	struct Cluster {
		
		static constexpr int kNumEntries = 3;
		int padding;

		TTEntry entries[kNumEntries];
	};
	
	static_assert((sizeof(Cluster) % CacheLineSize) == 0, "");

	virtual ~TranspositionTable() {
		Release();
	}

	
	size_t Size() const {
		return sizeof(Cluster) * num_clusters;
	}

	
	TTEntry& LookUp(Key key, Color root_color) {
		auto& entries = tt[key & clusters_mask];
		uint32_t hash_high = ((key >> 32) & ~1) | root_color;

		

		for (auto& entry : entries.entries)
			if (hash_high == entry.hash_high && entry.generation == generation)
				return entry;
			
			
			
			

		

		for (auto& entry : entries.entries)
			
			
			if (entry.generation != generation)
			{
				entry.init(hash_high , generation);
				return entry;
			}

		

		
		TTEntry* best_entry = nullptr;
		uint32_t best_node_searched = UINT32_MAX;

		for (auto& entry : entries.entries)
		{
			if (best_node_searched > entry.num_searched) {
				best_entry = &entry;
				best_node_searched = entry.num_searched;
			}
		}

		best_entry->init(hash_high , generation);
		return *best_entry;
	}

	TTEntry& LookUp(Position& n, Color root_color) {
		return LookUp(n.key(), root_color);
	}

	
	TTEntry& LookUpChildEntry(Position& n, Move move, Color root_color) {
		return LookUp(n.key_after(move), root_color);
	}

	
	
	void Resize(OptionsMap& option)
	{
		int64_t hash_size_mb = (int)option["USI_Hash"];

		
		int64_t new_num_clusters = 1LL << MSB64((hash_size_mb * 1024 * 1024) / sizeof(Cluster));

		
		if (new_num_clusters == num_clusters) {
			return;
		}

		num_clusters = new_num_clusters;

		Release();

		tt = (Cluster*)aligned_large_pages_alloc(new_num_clusters * sizeof(Cluster) + CacheLineSize);
		clusters_mask = num_clusters - 1;
	}

	
	void Release()
	{
		if (tt) {
			aligned_large_pages_free(tt);
			tt = nullptr;
		}
	}

	
	void NewSearch() {
		++generation;
	}

	
	
	int hashfull() const
	{
		
		int num_used = 0;

		
		int num_checked = 0;

		for (int cluster_index = 0; ; ++cluster_index)
			for (int entry_index = 0; entry_index < Cluster::kNumEntries; ++entry_index)
			{
				auto& entry = tt[cluster_index].entries[entry_index];
				
				if (entry.generation == generation)
					++num_used;

				if (++num_checked == 1000)
					return num_used;
			}
	}

	
	void* tt_raw = nullptr;

	
	Cluster* tt = nullptr;

	
	int64_t num_clusters = 0;

	
	
	int64_t clusters_mask = 0;

	
	uint16_t generation;
};


class TanukiMateClass
{
public:
	TanukiMateClass(ThreadPool& threads, OptionsMap& options)
		: threads(threads), options(options) , transposition_table(options) {}
	ThreadPool& threads;
	OptionsMap& options;
	Search::LimitsType* limits_ptr;

	void set_limits(Search::LimitsType& limits)
	{
		this->limits_ptr = &limits;
	}

	
	void dfpn(Position& r) {
		auto& limits = *limits_ptr;

		threads.wait_for_search_finished();
			
		
				

				
		transposition_table.NewSearch();

		auto start = now();

		bool timeup = false;
		Color root_color = r.side_to_move();
		DFPNwithTCA(r, kInfinitePnDn, kInfinitePnDn, false, true, 0, root_color, start, timeup);
		const auto& entry = transposition_table.LookUp(r, root_color);

		auto nodes_searched = threads.nodes_searched();
		sync_cout << "info string" <<
			" pn " << entry.pn <<
			" dn " << entry.dn <<
			" nodes_searched " << nodes_searched << sync_endl;

		std::vector<Move> moves;
		if (options[kMorePreciseMatePv]) {
			std::unordered_map<Key, MateState> memo;
			SearchMatePvMorePrecise(true, root_color, r, memo);

			
			StateInfo state_info[2048] = {};
			bool found = false;
			for (int play = 0; ; ++play) {
				const auto& mate_state = memo[r.key()];
				if (mate_state.num_moves_to_mate == kNotMate) {
					break;
				}
				else if (mate_state.num_moves_to_mate == 0) {
					found = true;
					break;
				}

				moves.push_back(mate_state.move_to_mate);
				r.do_move(mate_state.move_to_mate, state_info[play]);
			}

			
			for (int play = static_cast<int>(moves.size()) - 1; play >= 0; --play) {
				r.undo_move(moves[play]);
			}

			
			if (!found) {
				moves.clear();
			}
		}
		else {
			std::unordered_set<Key> visited;
			SearchMatePvFast(true, root_color, r, moves, visited);
		}

		auto end = now();
		if (!moves.empty()) {
			auto time_ms = end - start;
			time_ms = std::max(time_ms, TimePoint(1));
			int64_t nps = nodes_searched * 1000LL / time_ms;
			sync_cout << "info  time " << time_ms << " nodes " << nodes_searched << " nps "
				<< nps << " hashfull " << transposition_table.hashfull() << sync_endl;
		}

								
		while (!threads.stop && ( limits.infinite))
		{
			
			
			Tools::sleep(1);
		}


		if (timeup) {
			
			sync_cout << "checkmate timeout" << sync_endl;
		}
		else if (moves.empty()) {
			
			sync_cout << "checkmate nomate" << sync_endl;
		}
		else {
			
			std::ostringstream oss;
			oss << "checkmate";
			for (const auto& move : moves) {
				oss << " " << move;
			}
			sync_cout << oss.str() << sync_endl;
		}

		threads.stop = true;
	}

	
	void DFPNwithTCA(Position& n, uint32_t thpn, uint32_t thdn, bool inc_flag, bool or_node, uint16_t depth,
		Color root_color, TimePoint start_time, bool& timeup) {

		if (threads.stop.load(std::memory_order_relaxed)) {
			return;
		}

		auto nodes_searched = threads.nodes_searched();

		if (nodes_searched && (nodes_searched % 1000000) == 0)
		{
			
			

			auto current_time = now();
			auto time_ms = current_time - start_time;
			time_ms = std::max(time_ms, decltype(time_ms)(1));
			int64_t nps = nodes_searched * 1000LL / time_ms;

			sync_cout << "info  time " << time_ms << " nodes " << nodes_searched << " nps "
				<< nps << " hashfull " << transposition_table.hashfull() << sync_endl;
		}

		auto& limits = *limits_ptr;

#if 0

		
		
		
		if (limits.mate != INT32_MAX && nodes_searched % 4096 == 0) {
			auto elapsed_ms = Time.elapsed_from_ponderhit();
			if (elapsed_ms > limits.mate)
			{
				timeup = true;
				threads.stop = true;
				return;
			}
		}
#endif

		
		
		
		if (limits.nodes != 0 && nodes_searched >= (uint64_t)limits.nodes)
		{
			timeup = true;
			threads.stop = true;
			return;
		}

		auto& entry = transposition_table.LookUp(n, root_color);

		if (depth > kMaxDepth) {
			entry.pn = kInfinitePnDn;
			entry.dn = 0;
			entry.minimum_distance = std::min(entry.minimum_distance, depth);
			return;
		}

		

		
		if (or_node && !n.in_check() && Mate::mate_1ply(n)) {
			entry.pn = 0;
			entry.dn = kInfinitePnDn;
			entry.minimum_distance = std::min(entry.minimum_distance, depth);
			return;
		}

		MovePicker move_picker(n, or_node);
		bool has_loop = false;
		bool avoid_loop = false;
		
		for (const auto& move : move_picker) {
			
			
			const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
			if (entry.minimum_distance > child_entry.minimum_distance &&
				child_entry.pn != kInfinitePnDn &&
				child_entry.dn != kInfinitePnDn) {
				has_loop = true;
				break;
			}
		}

		if (has_loop) {
			
			
			
			
			
			
			
			
			auto draw_type = n.is_repetition(n.game_ply());
			switch (draw_type) {
			case REPETITION_WIN:
				
				if (or_node) {
					
					entry.pn = 0;
					entry.dn = kInfinitePnDn;
					entry.minimum_distance = std::min(entry.minimum_distance, depth);
				}
				else {
					entry.pn = kInfinitePnDn;
					entry.dn = 0;
					entry.minimum_distance = std::min(entry.minimum_distance, depth);
				}
				return;

			case REPETITION_LOSE:
				avoid_loop = true;
				
				break;
				
				if (or_node) {
					entry.pn = kInfinitePnDn;
					entry.dn = 0;
					entry.minimum_distance = std::min(entry.minimum_distance, depth);
				}
				else {
					
					entry.pn = 0;
					entry.dn = kInfinitePnDn;
					entry.minimum_distance = std::min(entry.minimum_distance, depth);
				}
				return;

			case REPETITION_DRAW:
				
				
				entry.pn = kInfinitePnDn;
				entry.dn = 0;
				entry.minimum_distance = std::min(entry.minimum_distance, depth);
				return;

			default:
				break;
			}
		}

		if (move_picker.empty()) {
			

			if (or_node) {
				
				entry.pn = kInfinitePnDn;
				entry.dn = 0;
			}
			else {
				
				entry.pn = 0;
				entry.dn = kInfinitePnDn;
			}

			entry.minimum_distance = std::min(entry.minimum_distance, depth);
			return;
		}

		
		
		entry.minimum_distance = std::min(entry.minimum_distance, depth);

		bool first_time = true;
		while (!threads.stop.load(std::memory_order_relaxed)) {
			++entry.num_searched;

			
			
			if (entry.pn == 1 && entry.dn == 1) {
				inc_flag = false;
			}

			
			for (const auto& move : move_picker) {
				
				
				const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
				if (entry.minimum_distance > child_entry.minimum_distance &&
					child_entry.pn != kInfinitePnDn &&
					child_entry.dn != kInfinitePnDn) {
					inc_flag = true;
					break;
				}
			}

			
			if (or_node) {
				entry.pn = kInfinitePnDn;
				entry.dn = 0;
				bool is_mate = false;
				for (const auto& move : move_picker) {
					const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
					if (child_entry.pn == 0) {
						is_mate = true;
					}
					if (avoid_loop && entry.minimum_distance > child_entry.minimum_distance && child_entry.pn != 0) {
						continue;
					}
					entry.pn = std::min(entry.pn, child_entry.pn);
					entry.dn += child_entry.dn;
				}
				if (is_mate) {
					entry.dn = kInfinitePnDn;
				}
				else {
					entry.dn = std::min(entry.dn, kInfinitePnDn - 1);
				}
			}
			else {
				entry.pn = 0;
				entry.dn = kInfinitePnDn;
				bool is_nomate = false;
				for (const auto& move : move_picker) {
					const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
					if (child_entry.dn == 0) {
						is_nomate = true;
					}
					entry.pn += child_entry.pn;
					entry.dn = std::min(entry.dn, child_entry.dn);
				}
				if (is_nomate) {
					entry.pn = kInfinitePnDn;
				}
				else {
					entry.pn = std::min(entry.pn, kInfinitePnDn - 1);
				}
			}

			
			
			
			
			
			if (first_time && inc_flag) {
				thpn = std::max(thpn, entry.pn + 1);
				thpn = std::min(thpn, kInfinitePnDn);
				thdn = std::max(thdn, entry.dn + 1);
				thdn = std::min(thdn, kInfinitePnDn);
			}

			
			
			if (entry.pn >= thpn || entry.dn >= thdn) {
				break;
			}

			
			first_time = false;

			
			
			
			
			
			
			
			
			Move best_move = Move::none(); 
			int thpn_child;
			int thdn_child;
			if (or_node) {
				
				uint32_t best_pn = kInfinitePnDn;
				uint32_t second_best_pn = kInfinitePnDn;
				uint32_t best_dn = 0;
				uint32_t best_num_search = UINT32_MAX;
				for (const auto& move : move_picker) {
					const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
					if (avoid_loop && entry.minimum_distance > child_entry.minimum_distance && child_entry.pn != 0) {
						continue;
					}
					if (child_entry.pn < best_pn ||
						(child_entry.pn == best_pn && best_num_search > child_entry.num_searched)) {
						second_best_pn = best_pn;
						best_pn = child_entry.pn;
						best_dn = child_entry.dn;
						best_move = move;
						best_num_search = child_entry.num_searched;
					}
					else if (child_entry.pn < second_best_pn) {
						second_best_pn = child_entry.pn;
					}
				}
				thpn_child = std::min(thpn, second_best_pn + 1);
				thdn_child = std::min(thdn - entry.dn + best_dn, kInfinitePnDn);
			}
			else {
				
				uint32_t best_dn = kInfinitePnDn;
				uint32_t second_best_dn = kInfinitePnDn;
				uint32_t best_pn = 0;
				uint32_t best_num_search = UINT32_MAX;
				for (const auto& move : move_picker) {
					const auto& child_entry = transposition_table.LookUpChildEntry(n, move, root_color);
					if (child_entry.dn < best_dn ||
						(child_entry.dn == best_dn && best_num_search > child_entry.num_searched)) {
						second_best_dn = best_dn;
						best_dn = child_entry.dn;
						best_pn = child_entry.pn;
						best_move = move;
					}
					else if (child_entry.dn < second_best_dn) {
						second_best_dn = child_entry.dn;
					}
				}

				thpn_child = std::min(thpn - entry.pn + best_pn, kInfinitePnDn);
				thdn_child = std::min(thdn, second_best_dn + 1);
			}

			if (best_move == Move::none() && or_node) {
				entry.pn = kInfinitePnDn;
				entry.dn = 0;
				return;
			}
			StateInfo state_info;
			n.do_move(best_move, state_info);
			DFPNwithTCA(n, thpn_child, thdn_child, inc_flag, !or_node, depth + 1, root_color,
				start_time, timeup);
			n.undo_move(best_move);
		}
	}

	void pv_check_from_table(Position& pos, vector<Move16> pv_check) {
		Color root_color = pos.side_to_move();
		const auto& entry0 = transposition_table.LookUp(pos, root_color);
		cout << pos << endl;
		cout << "pn-dn" << entry0.pn << "," << entry0.dn << endl;
		cout << "key-gen" << entry0.hash_high << "," << entry0.generation << endl;

		for (auto m : pv_check) {
			StateInfo state_info;
			Move move = pos.to_move(m);
			pos.do_move(move, state_info);
			const auto& entry = transposition_table.LookUp(pos, root_color);
			cout << pos << endl;
			cout << "pn,dn = " << entry.pn << "," << entry.dn << endl;
			cout << "key-gen = " << entry.hash_high << endl;
			if (entry.pn != 0 && entry.dn != 0) {
				continue;
			}
			bool or_node = (entry.minimum_distance % 2 == 0);
			if (or_node && entry.pn == 0) {
				cout << "example of mate moves : ";
				MovePicker move_picker(pos, or_node);
				for (const auto& move : move_picker) {
					auto entry_child = transposition_table.LookUpChildEntry(pos, move, root_color);
					if (entry_child.pn == 0) {
						cout << move << " ";
					}
				}
				cout << endl;
			}
			else if (!or_node && entry.dn == 0) {
				MovePicker move_picker(pos, or_node);
				cout << "example of escape moves : ";
				for (const auto& move : move_picker) {
					auto entry_child = transposition_table.LookUpChildEntry(pos, move, root_color);
					if (entry_child.pn == 0) {
						cout << move << " ";
					}
				}
				cout << endl;
			}
		}
	}


	
	
	bool SearchMatePvFast(bool or_node, Color root_color, Position& pos, std::vector<Move>& moves, std::unordered_set<Key>& visited) {
		
		if (visited.find(pos.key()) != visited.end()) {
			return false;
		}
		visited.insert(pos.key());

		MovePicker move_picker(pos, or_node);
		Move mate1ply = Mate::mate_1ply(pos);
		if (mate1ply || move_picker.empty()) {
			if (mate1ply) {
				moves.push_back(mate1ply);
			}
			
			
			
			
			
			
			
			
			
			return true;
		}

		const auto& entry = transposition_table.LookUp(pos, root_color);

		for (const auto& move : move_picker) {
			const auto& child_entry = transposition_table.LookUpChildEntry(pos, move, root_color);
			if (child_entry.pn != 0) {
				continue;
			}

			StateInfo state_info;
			pos.do_move(move, state_info);
			moves.push_back(move);
			if (SearchMatePvFast(!or_node, root_color, pos, moves, visited)) {
				pos.undo_move(move);
				return true;
			}
			moves.pop_back();
			pos.undo_move(move);
		}

		return false;
	}

	
	static constexpr const int kSearching = -1;
	
	static constexpr const int kLoop = -2;
	
	static constexpr const int kNotMate = -3;

	struct MateState {
		int num_moves_to_mate = kSearching;
		Move move_to_mate = Move::none();
	};

	
	
	
	
	
	
	
	
	
	
	int SearchMatePvMorePrecise(bool or_node, Color root_color, Position& pos, std::unordered_map<Key, MateState>& memo) {
		
		auto key = pos.key();
		if (memo.find(key) != memo.end()) {
			auto& mate_state = memo[key];
			if (mate_state.num_moves_to_mate == kSearching) {
				
				return kLoop;
			}
			else if (mate_state.num_moves_to_mate == kNotMate) {
				return kNotMate;
			}
			else {
				return mate_state.num_moves_to_mate;
			}
		}
		auto& mate_state = memo[key];

		auto mate1ply = Mate::mate_1ply(pos);
		if (or_node && !pos.in_check() && mate1ply) {
			mate_state.num_moves_to_mate = 1;
			mate_state.move_to_mate = mate1ply;

			
			StateInfo state_info = {};
			pos.do_move(mate1ply, state_info);
			auto& mate_state_mated = memo[pos.key()];
			mate_state_mated.num_moves_to_mate = 0;
			pos.undo_move(mate1ply);
			return 1;
		}

		MovePicker move_picker(pos, or_node);
		if (move_picker.empty()) {
			if (or_node) {
				
				
				mate_state.num_moves_to_mate = kNotMate;
				return kNotMate;
			}
			else {
				
				
				mate_state.num_moves_to_mate = 0;
				return 0;
			}
		}

		auto best_num_moves_to_mate = or_node ? int_max : int_min;
		auto best_move_to_mate = Move::none();
		const auto& entry = transposition_table.LookUp(pos, root_color);

		for (const auto& move : move_picker) {
			const auto& child_entry = transposition_table.LookUpChildEntry(pos, move, root_color);
			if (child_entry.pn != 0) {
				continue;
			}

			StateInfo state_info;
			pos.do_move(move, state_info);
			int num_moves_to_mate_candidate = SearchMatePvMorePrecise(!or_node, root_color, pos, memo);
			pos.undo_move(move);

			if (num_moves_to_mate_candidate < 0) {
				continue;
			}
			else if (or_node) {
				
				if (best_num_moves_to_mate > num_moves_to_mate_candidate) {
					best_num_moves_to_mate = num_moves_to_mate_candidate;
					best_move_to_mate = move;
				}
			}
			else {
				
				if (best_num_moves_to_mate < num_moves_to_mate_candidate) {
					best_num_moves_to_mate = num_moves_to_mate_candidate;
					best_move_to_mate = move;
				}
			}
		}

		if (best_num_moves_to_mate == int_max || best_num_moves_to_mate == int_min) {
			mate_state.num_moves_to_mate = kNotMate;
			return kNotMate;
		}
		else {
			ASSERT_LV3(best_num_moves_to_mate >= 0);
			mate_state.num_moves_to_mate = best_num_moves_to_mate + 1;
			mate_state.move_to_mate = best_move_to_mate;
			return best_num_moves_to_mate + 1;
		}
	}

	
	
	void isready()
	{
		transposition_table.Resize(options);

		
		
		Tools::memclear(threads, "Tanuki::USI_Hash", transposition_table.tt, transposition_table.Size());
	}

	
	TanukiMate::TranspositionTable transposition_table;
};

class TanukiMateWorker : public YaneuraOu::Search::Worker
{
public:

	TanukiMateWorker(Search::SharedState& sharedState,
					 const Search::ThreadIds& ids,
					 TanukiMate::TanukiMateClass& mateClass) :
		
		Worker(sharedState, ids), mateClass(mateClass) {
			
	}

	
	
	virtual void clear() override
	{
	}

	
	
	
	virtual void start_searching() override
	{
		mateClass.set_limits(limits);

		if (pv_check.size() != 0) {
			mateClass.pv_check_from_table(rootPos, pv_check);
			return;
		}


		
		
#if 0
		
		
		if (limits.mate == 0) {
			
			while (!threads.stop && limits.infinite)
				Tools::sleep(1);
			sync_cout << "bestmove resign" << sync_endl;
			return;
		}
#endif

		mateClass.dfpn(rootPos);
	}

	
	TanukiMate::TanukiMateClass& mateClass;
};

class TanukiMateEngine : public Engine
{
public:
	TanukiMateEngine() : mateClass(threads, options) {}

	
    virtual std::string get_engine_name() const override { return "tanuki- mate solver"; }
    virtual std::string get_engine_author() const override { return "tanuki-"; }

	
	virtual void isready() override
	{
		mateClass.isready();

		
		Engine::isready();
	}

	
	virtual void add_options() override
	{
		
		Engine::add_options();

		
		options.add(  
			"USI_Hash", Option(1024, 1, MaxHashMB, [this](const Option& o) {
				
				
				return std::nullopt;
				}));

		options.add(kMorePreciseMatePv, Option(true));
	}

	
	virtual void user(std::istringstream& is) override
	{
		sync_cout << "UserEngine::user_cmd" << sync_endl;
	}

	
	virtual void resize_threads() override
	{
		

		
		threads.wait_for_search_finished();

		

		
		
		auto worker_factory = [&](Search::SharedState& sharedState,
								  const Search::ThreadIds& ids)
		{

			auto p = make_unique_large_page<TanukiMateWorker>(
				
                sharedState, ids,

				
				
				mateClass
			);

			return LargePagePtr<Worker>(p.release());  
		};

        threads.set(numaContext.get_numa_config(), {options, threads, tt, sharedHists  },
					updateContext, options["Threads"], worker_factory);

		

		
		threads.ensure_network_replicated();
	}

	TanukiMate::TanukiMateClass mateClass;
};

} 

namespace YaneuraOu::Eval {

void add_options(OptionsMap& options, ThreadPool& threads) {}

} 

namespace {

	
	void engine_main()
	{
		
		auto engine = std::make_unique<TanukiMate::TanukiMateEngine>();

		
		auto usi = std::make_unique<USIEngine>();
		usi->set_engine(*engine);  
		usi->enqueue_startup_commands(CommandLine::g);

		
		usi->loop();
	}

	
	static EngineFuncRegister r(engine_main, "UserEngine", 0);

} 


#endif
