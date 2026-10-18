#include "tt.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>



#include "memory.h"
#include "misc.h"
#include "thread.h"
#include "engine.h"


#include "extra/key128.h"
#include "testcmd/unit_test.h"

namespace YaneuraOu {




































































static constexpr uint8_t GENERATION_BITS = 5;                            
static constexpr uint8_t GENERATION_MASK = (1 << GENERATION_BITS) - 1;   
static constexpr uint8_t BOUND_SHIFT     = GENERATION_BITS;              
static constexpr uint8_t BOUND_MASK      = 0b11 << BOUND_SHIFT;          
static constexpr uint8_t PV_SHIFT        = BOUND_SHIFT + 2;              
static constexpr uint8_t PV_MASK         = 1 << PV_SHIFT;                

struct TTEntry;

CyfTT::Lock& cyf_tt_lock(const TTEntry* entry);

struct TTEntry {

    
    

	std::optional<std::pair<bool, TTData>> read_matching(TTE_KEY_TYPE wanted) const {
		CyfTT::Guard guard(cyf_tt_lock(this));
		if (key != wanted) return std::nullopt;
		return std::pair<bool, TTData>{bool(depth8), read_unlocked()};
	}

	TTData read() const {
		CyfTT::Guard guard(cyf_tt_lock(this));
		return read_unlocked();
	}

   private:
	TTData read_unlocked() const {
		return TTData{
		  Move(u32(move16.to_u16())), Value(value16), Value(eval16), Depth(DEPTH_NONE + depth8),
		  Bound((genBound8 & BOUND_MASK) >> BOUND_SHIFT), bool(genBound8 & PV_MASK)};
	}

   public:
	
	bool is_occupied() const { CyfTT::Guard guard(cyf_tt_lock(this)); return bool(depth8); };

	int replacement_score(uint8_t generation) const {
		CyfTT::Guard guard(cyf_tt_lock(this));
		return depth8 - 8 * relative_age_unlocked(generation);
	}

#if STOCKFISH
    void save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);
#else
    void
    save(TTE_KEY_TYPE k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);
#endif

	uint8_t relative_age(const uint8_t curr_generation) const;

   private:
    friend class TranspositionTable;

	uint8_t relative_age_unlocked(uint8_t generation) const {
		return (generation - genBound8) & GENERATION_MASK;
	}

    TTE_KEY_TYPE key;
    uint8_t      depth8;
    uint8_t      genBound8;
    Move16       move16;
    int16_t      value16;
    int16_t      eval16;
};

#if defined(CYF_BLEND_TTSHADOW)
#if !defined(CYF_BLEND)
#error "CYF_BLEND_TTSHADOW is a debug option of the -DCYF_BLEND build"
#endif




enum : uint8_t { CYF_TT_SITE_UNKNOWN = 0, CYF_TT_SITE_STATIC = 1, CYF_TT_SITE_PROBCUT = 2,
                 CYF_TT_SITE_PROBCUT_EXCLUDED = 3, CYF_TT_SITE_SEARCH_END = 4, CYF_TT_SITE_QS_MATE = 5,
                 CYF_TT_SITE_QS_STANDPAT = 6, CYF_TT_SITE_QS_END = 7, CYF_TT_SITE_MATE1PLY = 8 };
struct CyfTTShadowEntry {
    uint64_t key = 0, origin_key = 0;
    uint32_t rule = 0, origin_rule = 0;
    int16_t  eval = 0;
    uint8_t  site = 0, origin_site = 0, valid = 0;
};
struct CyfTTShadow {
    std::vector<CyfTTShadowEntry> s;
    const char* base = nullptr;
    size_t      cluster_bytes = 0, per_cluster = 0, entry_bytes = 0;
    uint64_t    pending_key = 0;
    uint8_t     pending_site = CYF_TT_SITE_UNKNOWN;
    uint32_t    rule = 0;
    uint64_t    writes = 0, inherited_writes = 0, probcut_excluded_writes = 0, unknown_site_writes = 0;
    uint64_t    probcut_excluded_skips = 0;
    void attach(const void* table, size_t clusters, size_t cbytes, size_t per, size_t ebytes) {
        base = static_cast<const char*>(table); cluster_bytes = cbytes; per_cluster = per; entry_bytes = ebytes;
        s.assign(clusters * per, CyfTTShadowEntry{});
    }
    void clear() { std::fill(s.begin(), s.end(), CyfTTShadowEntry{}); }
    void scrub() { for (auto& x : s) if (x.valid) x.eval = int16_t(VALUE_NONE); }   
    size_t index(const void* e) const {
        const size_t off = size_t(static_cast<const char*>(e) - base);
        return off / cluster_bytes * per_cluster + (off % cluster_bytes) / entry_bytes;
    }
    const CyfTTShadowEntry* find(const void* e) const {
        if (!base || !e) return nullptr;
        const size_t i = index(e);
        return i < s.size() ? &s[i] : nullptr;
    }
    void on_save(const void* e, int ev) {
        const uint8_t site = pending_site;
        pending_site = CYF_TT_SITE_UNKNOWN;
        if (!base) return;
        const size_t i = index(e);
        if (i >= s.size()) return;
        CyfTTShadowEntry& x = s[i];
        const uint64_t    k = pending_key;
        ++writes;
        unknown_site_writes += site == CYF_TT_SITE_UNKNOWN;
        probcut_excluded_writes += site == CYF_TT_SITE_PROBCUT_EXCLUDED;
        
        const bool inherit = x.valid && ev != int(VALUE_NONE) && int(x.eval) == ev && uint16_t(x.key) == uint16_t(k);
        if (inherit)
            ++inherited_writes;
        else
        {
            x.origin_key = k; x.origin_rule = rule; x.origin_site = site; x.eval = int16_t(ev);
        }
        x.key = k; x.rule = rule; x.site = site; x.valid = 1;
    }
    void reset_counters() { writes = inherited_writes = probcut_excluded_writes = unknown_site_writes = 0; }
};
inline CyfTTShadow& cyf_tt_shadow() { static CyfTTShadow s; return s; }
#endif















void TTEntry::save(TTE_KEY_TYPE k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t curr_generation) {
	CyfTT::Guard guard(cyf_tt_lock(this));

	
	

	if (m || k != key)
		move16 = m.to_move16();

	
	

	if (b == BOUND_EXACT || k != key || d - DEPTH_NONE + 2 * pv > depth8 - 4
		|| relative_age_unlocked(curr_generation))
	{
		assert(d > DEPTH_NONE);
		assert(d - DEPTH_NONE < 256);
		assert(curr_generation <= GENERATION_MASK); 

		key       = TTE_KEY_TYPE(k);
		depth8    = uint8_t(d - DEPTH_NONE);
		genBound8 = uint8_t(curr_generation | b << BOUND_SHIFT | uint8_t(pv) << PV_SHIFT);
		value16   = int16_t(v);
		eval16    = int16_t(ev);
#if defined(CYF_BLEND_TTSHADOW)
		cyf_tt_shadow().on_save(this, int(ev));
#endif

		
		ASSERT_LV3(-VALUE_INFINITE <   v  && v < VALUE_INFINITE  ||  v == VALUE_NONE);
		ASSERT_LV3(-VALUE_MAX_EVAL <= ev && ev <= VALUE_MAX_EVAL || ev == VALUE_NONE);
		
	}
}


uint8_t TTEntry::relative_age(const uint8_t curr_generation) const {
	CyfTT::Guard guard(cyf_tt_lock(this));
	
	
	

	
	
	

	return relative_age_unlocked(curr_generation);
}





TTWriter::TTWriter(TTEntry* tte) :
	entry(tte) {}

#if STOCKFISH
void TTWriter::write(
  Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8) {
    entry->save(k, v, pv, b, d, m, ev, generation8);
}
#else

void TTWriter::write(
  const Key k_, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8) {

#if HASH_KEY_BITS <= 64
    const TTE_KEY_TYPE k = TTE_KEY_TYPE(k_);
#else
    const TTE_KEY_TYPE k = TTE_KEY_TYPE(k_.extract64<1>());
#endif

#if defined(CYF_BLEND_TTSHADOW)
    cyf_tt_shadow().pending_key = uint64_t(k_);
#endif
    entry->save(k, v, pv, b, d, m, ev, generation8);
#if defined(CYF_BLEND_TTSHADOW)
    cyf_tt_shadow().pending_site = CYF_TT_SITE_UNKNOWN;   
#endif
}
#endif























static constexpr int ClusterSize = TT_CLUSTER_SIZE;


struct alignas(std::bit_ceil(ClusterSize * sizeof(TTEntry))) Cluster {
	TTEntry entry[ClusterSize];

#if TT_CLUSTER_SIZE == 3
	mutable CyfTT::Lock lock;  
	char    padding[1];        
						 
#endif
};


static_assert((sizeof(Cluster) % 32) == 0, "Unexpected Cluster size");
static_assert((sizeof(Cluster) & (sizeof(Cluster) - 1)) == 0,
              "cyf_tt_lock finds the cluster by address mask; cluster size must be a power of two");

inline CyfTT::Lock& cyf_tt_cluster_lock(const Cluster* c) {
#if TT_CLUSTER_SIZE == 3
	return c->lock;
#else
	return CyfTT::stripe_for(c);
#endif
}



inline CyfTT::Lock& cyf_tt_lock(const TTEntry* entry) {
	const auto a = reinterpret_cast<std::uintptr_t>(entry) & ~std::uintptr_t(sizeof(Cluster) - 1);
	return cyf_tt_cluster_lock(reinterpret_cast<const Cluster*>(a));
}









void TranspositionTable::resize(size_t mbSize, ThreadPool& threads) {
#if STOCKFISH
    aligned_large_pages_free(table);

    clusterCount = mbSize * 1024 * 1024 / sizeof(Cluster);

#else

	
	
	size_t newClusterCount = mbSize * 1024 * 1024 / sizeof(Cluster);

	
	
	
	
	ASSERT_LV3((newClusterCount & 1) == 0);

	

	
	

	if (newClusterCount == clusterCount)
		return;

	aligned_large_pages_free(table);

	clusterCount = newClusterCount;
#endif

	
	
	
	

	

	table = static_cast<Cluster*>(aligned_large_pages_alloc(clusterCount * sizeof(Cluster)));

	if (!table)
	{
		std::cerr << "Failed to allocate " << mbSize << "MB for transposition table." << std::endl;
		exit(EXIT_FAILURE);
	}
#if defined(CYF_BLEND_TTSHADOW)
	cyf_tt_shadow().attach(table, clusterCount, sizeof(Cluster), ClusterSize, sizeof(TTEntry));
#endif

#if STOCKFISH
	clear(threads);

	
	
#endif
}






void TranspositionTable::clear(ThreadPool& threads) {

#if defined(TANUKI_MATE_ENGINE) || defined(YANEURAOU_MATE_ENGINE)
	
	return;
#endif

	generation8 = 0;

	
#if 0
	const size_t threadCount = threads.num_threads();

	for (size_t i = 0; i < threadCount; ++i)
	{
		threads.run_on_thread(i, [this, i, threadCount]() {
			
			const size_t stride = clusterCount / threadCount;
			const size_t start = stride * i;
			const size_t len = i + 1 != threadCount ? stride : clusterCount - start;

			std::memset(&table[start], 0, len * sizeof(Cluster));
			});
	}

	for (size_t i = 0; i < threadCount; ++i)
		threads.wait_on_thread(i);
#endif

	auto size = clusterCount * sizeof(Cluster);

	
	
	Tools::memclear(threads, "USI_Hash", table, size);
#if defined(CYF_BLEND_TTSHADOW)
	cyf_tt_shadow().clear();
#endif
}

#if defined(CYF_BLEND_DIRTY_SCRUB)


struct CyfDirtyClusters {
    const void* base=nullptr; size_t count=0;
    std::vector<uint8_t> marked;
    std::vector<size_t> dirty;
    bool valid=false;
};
static CyfDirtyClusters& cyf_dirty_clusters(){static CyfDirtyClusters d;return d;}
#endif
#if defined(CYF_BLEND)
void TranspositionTable::scrub_values() {
#if defined(CYF_BLEND_TTSHADOW)
	cyf_tt_shadow().scrub();
#endif
    auto scrub_cluster=[&](size_t c){
		CyfTT::Guard guard(cyf_tt_cluster_lock(&table[c]));
		for (int i = 0; i < ClusterSize; ++i)
		{
			TTEntry& e = table[c].entry[i];
			if (!e.depth8)
				continue;
			e.value16   = int16_t(VALUE_NONE);
			e.eval16    = int16_t(VALUE_NONE);
			e.genBound8 = uint8_t(e.genBound8 & ~BOUND_MASK);   
			e.depth8    = 1;                                     
		}
    };
#if defined(CYF_BLEND_DIRTY_SCRUB)
    auto& d=cyf_dirty_clusters();
    if(d.valid && d.base==table && d.count==clusterCount){
        for(auto c:d.dirty){scrub_cluster(c);d.marked[c]=0;}
    }else{
        for(size_t c=0;c<clusterCount;++c)scrub_cluster(c);
        d.base=table;d.count=clusterCount;d.marked.assign(clusterCount,0);d.valid=true;
    }
    d.dirty.clear();
#else
    for(size_t c=0;c<clusterCount;++c)scrub_cluster(c);
#endif
}
#endif









int TranspositionTable::hashfull(int maxAge) const {
	int cnt = 0;
	for (int i = 0; i < 1000; ++i) {
		CyfTT::Guard guard(cyf_tt_cluster_lock(&table[i]));
		for (int j = 0; j < ClusterSize; ++j) {
			const auto& e = table[i].entry[j];
			cnt += bool(e.depth8) && e.relative_age_unlocked(generation8) <= maxAge;
		}
	}

	return cnt / ClusterSize;
}

void TranspositionTable::new_search() {

	++generation8;

	
	
	generation8 &= GENERATION_MASK;
}


uint8_t TranspositionTable::generation() const { return generation8; }















std::tuple<bool, TTData, TTWriter> TranspositionTable::probe(const Key key, const Position& pos) const {

    TTEntry* const tte = first_entry(key, pos.side_to_move());
#if defined(CYF_BLEND_DIRTY_SCRUB)
    auto& dirty=cyf_dirty_clusters();
    if(dirty.base!=table || dirty.count!=clusterCount)dirty.valid=false;
    if(dirty.valid){
        const size_t c=(reinterpret_cast<const char*>(tte)-reinterpret_cast<const char*>(table))/sizeof(Cluster);
        if(!dirty.marked[c]){dirty.marked[c]=1;dirty.dirty.push_back(c);}
    }
#endif

#if HASH_KEY_BITS <= 64
    const TTE_KEY_TYPE key_for_ttentry = TTE_KEY_TYPE(key);
#else
    const TTE_KEY_TYPE key_for_ttentry = TTE_KEY_TYPE(key.extract64<1>());
#endif

	
	

	
	
	
	
	
	
	
	static_assert(std::is_trivially_copyable_v<TTEntry>);
	union SnapStorage { SnapStorage() {} TTEntry e[ClusterSize]; } snapStorage;
	TTEntry* const snap = snapStorage.e;
	{
		CyfTT::Guard guard(cyf_tt_lock(tte));
		std::memcpy(static_cast<void*>(snap), tte, sizeof(TTEntry) * ClusterSize);
	}

	for (int i = 0; i < ClusterSize; ++i) {
		if (snap[i].key == key_for_ttentry)
		{
			auto ttData = snap[i].read_unlocked();
			if (ttData.move)
			{
				
				
				Move move = pos.to_move(ttData.move.to_move16());
				if (!move)
					continue;
				ttData.move = move;
			}
			return { bool(snap[i].depth8), ttData, TTWriter(&tte[i]) };
		}
	}

	
	

	auto score_of = [&](int i) { return snap[i].depth8 - 8 * snap[i].relative_age_unlocked(generation8); };
	TTEntry* replace = tte;
	int replaceScore = score_of(0);
	for (int i = 1; i < ClusterSize; ++i)
	{
		const int score = score_of(i);
		if (replaceScore > score) {
			replace = &tte[i];
			replaceScore = score;
		}
	}

	return { false,
			TTData{Move::none(), VALUE_NONE, VALUE_NONE, DEPTH_NONE, BOUND_NONE, false},
			TTWriter(replace) };
}




TTEntry* TranspositionTable::first_entry(const Key& key_, Color side_to_move) const {

#if STOCKFISH

    return &table[mul_hi64(key, clusterCount)].entry[0];
	

	
    
    

#else

	
	const Key64 key = Key64(key_);

	
	ASSERT_LV3((clusterCount & 1) == 0);

	
	uint64_t index = mul_hi64((u64)key, clusterCount);

	
	
	return &table[(index & ~1) | side_to_move].entry[0];

#endif
}





void TranspositionTable::UnitTest(Test::UnitTester& unittest, IEngine& engine)
{
	auto section1 = unittest.section("TT");
	{
		TranspositionTable tt;

		
		tt.resize(1024,engine.get_threads());
		tt.clear(engine.get_threads());

		auto section2 = unittest.section("probe()");
		Position pos;
		StateInfo si;
		pos.set_hirate(&si);

		Key posKey = pos.key();
		auto [ttHit, ttData, ttWriter] = tt.probe(posKey, pos);
		for (int i = 0; i < 10; ++i)
		{
			Value v = Value(i*100-500);
			bool pv = (i % 2) != 0;
			Bound b = BOUND_LOWER;
			Depth d = 16 + i;
			Move m = make_move(SQ_77, SQ_76, BLACK, PAWN);
			Value ev = Value(i*200-1000);
			int g = 5;
			ttWriter.write(posKey, v, pv, b, d, m, ev, g);

			auto [ttHit, ttData, ttWriter] = tt.probe(posKey, pos);
			bool ok = true;
			ok &= ttHit;
			if (ttHit)
			{
				ok &= ttData.value == v;
				ok &= ttData.is_pv == pv;
				ok &= ttData.bound == b;
				ok &= ttData.depth == d;
				ok &= ttData.move  == m;
				ok &= ttData.eval  == ev;
			}
			unittest.test("write & probe", ok);
		}
	}
}

} 


