#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED



#include "types.h"
#include "misc.h"
#include "memory.h"
#include "thread.h"

namespace YaneuraOu {

struct Key128;
struct Key256;




struct TTEntry;
struct Cluster;






























struct TTData {
	Move   move;
	Value  value, eval;
	Depth  depth;
	Bound  bound;
	bool   is_pv;

	TTData() = delete;

	
	TTData(Move m, Value v, Value ev, Depth d, Bound b, bool pv) :
		move(m),
		value(v),
		eval(ev),
		depth(d),
		bound(b),
		is_pv(pv) {
	};
	
};







struct TTWriter {
public:
    void write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);
#if defined(CYF_BLEND_TTSHADOW)
    const void* debug_entry() const { return entry; }   
#endif

private:
	friend class TranspositionTable;
	TTEntry* entry;
	TTWriter(TTEntry* tte);
};






#if TT_CLUSTER_SIZE == 3
typedef uint16_t TTE_KEY_TYPE;
#else 
typedef uint64_t TTE_KEY_TYPE;
#endif








class TranspositionTable {

public:
	~TranspositionTable() { aligned_large_pages_free(table); }

	
	

	void resize(size_t mbSize,ThreadPool& threads);  

	
	

	
	

	void clear(ThreadPool& threads);                  

#if defined(CYF_BLEND)
	
	
	void scrub_values();
#endif

	
	
	

	void new_search();

	
	

	uint8_t generation() const;

	
	
	

	int hashfull(int maxAge = 0) const;

	
	
	
	
	
	
	
	
	

	
	
	
	
	
	

#if STOCKFISH
    std::tuple<bool, TTData, TTWriter> probe(
          const Key key) const;  
#else
	std::tuple<bool, TTData, TTWriter> probe(const Key key, const Position& pos) const;
#endif

	
	

	 

#if STOCKFISH
    TTEntry* first_entry(const Key key)
      const;  
#else
	TTEntry* first_entry(const Key& key, Color side_to_move) const;
#endif

	static void UnitTest(Test::UnitTester& unittest, IEngine& engine);

private:
	friend struct TTEntry;

	
	
	
	
	size_t clusterCount = 0;

	
	
	
	Cluster* table = nullptr;

	
	uint8_t generation8 = 0;
};

} 

#endif 
