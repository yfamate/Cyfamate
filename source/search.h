#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED


#include <cstring>
#include <iterator>


#include "config.h"

#include "history.h"
#include "misc.h"


#include "numa.h"
#include "position.h"
#include "score.h"


#include "timeman.h"


namespace YaneuraOu {







enum NodeType {
	NonPV,
	PV,
	Root
};

class TranspositionTable;
class ThreadPool;
class OptionsMap;


namespace Search {






struct PVMoves {
    Move        moves[MAX_PLY + 1];
    std::size_t length = 0;

    Move*       begin() { return moves; }
    const Move* begin() const { return moves; }
    Move*       end() { return moves + length; }
    const Move* end() const { return moves + length; }

    Move&       operator[](std::size_t index) { return moves[index]; }
    const Move& operator[](std::size_t index) const { return moves[index]; }

    bool        empty() const { return length == 0; }
    std::size_t size() const { return length; }

    void clear() { length = 0; }

    void push_back(Move move) {
        assert(length < MAX_PLY + 1);
        moves[length++] = move;
    }

    void resize(std::size_t newSize) {
        assert(newSize <= length);
        length = newSize;
    }

    void update(Move move, const PVMoves* childPv) {
        assert(childPv == nullptr || childPv->size() <= MAX_PLY);
        length = childPv ? childPv->length : 0;

        if (childPv)
        {
            std::memcpy(moves + 1, childPv->moves, length * sizeof(Move));
        }

        moves[0] = move;
        ++length;
    }
};












struct RootMove
{
	
    explicit RootMove(Move m) { pv.push_back(m); }

	
	
	
	

	
	
	
	

	bool extract_ponder_from_tt(const TranspositionTable& tt, Position& pos, Move ponder_candidate);

	
	bool operator==(const Move& m) const { return pv[0] == m; }

	
	
	bool operator<(const RootMove& m) const {
		return m.score != score ? m.score < score
								: m.previousScore < previousScore;
	}

	
	
    uint64_t effort        = 0;

	
	Value score			   = -VALUE_INFINITE;

	
	
	Value previousScore    = -VALUE_INFINITE;

	
	Value averageScore	   = -VALUE_INFINITE;

	
	Value meanSquaredScore = - VALUE_INFINITE * VALUE_INFINITE;

	
	
	
	Value uciScore		   = -VALUE_INFINITE;

	
	bool scoreLowerbound   = false;
    bool zyfMateLower = false; 

	
	bool scoreUpperbound   = false;

	
	int selDepth           = 0;

#if defined(CYF_BLEND)
	
	
	uint64_t cyfRule = 0;
	int      cyfPass = 0;
#endif

#if STOCKFISH
	
	int tbRank          = 0;
	Value tbScore;
#endif

	
	PVMoves pv;
};

using RootMoves = std::vector<RootMove>;



struct LimitsType {

    
    
    LimitsType() {

        time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK]  = movetime = TimePoint(0);
        depth = mate = perft = infinite = 0;
        nodes                                           = 0;
        ponderMode                                      = false;

        

        byoyomi[WHITE] = byoyomi[BLACK] = TimePoint(0);
        rtime                           = 0;
#if !STOCKFISH
        disablePvInterval               = false;
        cyfGoGen                        = 0;
#endif
    }

    
    
    
    bool use_time_management() const {
        
        
        return !(mate | movetime | depth | nodes | perft | infinite);
    }

    
    std::vector<std::string> searchmoves;

    
    
    
    
    
    
    
#if STOCKFISH
    TimePoint time[COLOR_NB], inc[COLOR_NB], npmsec, movetime, startTime;
#else
    TimePoint time[COLOR_NB], inc[COLOR_NB] , movetime, startTime;
#endif

    
    
    
    
    
    
    
    
    
    
    
#if STOCKFISH
    int movestogo, depth, mate, perft, infinite;
#else
    int  depth, mate, perft, infinite;
#endif

    
    
    
    uint64_t nodes;

    
    
    
    bool ponderMode;

#if STOCKFISH
#else
    

    
    TimePoint byoyomi[COLOR_NB];

    
    TimePoint rtime;

    
    bool disablePvInterval;

    
    
    
    uint64_t cyfGoGen;
#endif
};







struct SharedState {
    SharedState(const OptionsMap&                                         optionsMap,
                ThreadPool&                                               threadPool,
                TranspositionTable&                                       transpositionTable,
                std::map<NumaIndex, SharedHistories>&                     sharedHists
				
				
	) :
        options(optionsMap),
        threads(threadPool),
        tt(transpositionTable),
        sharedHistories(sharedHists)
        
	{}

    const OptionsMap&                                         options;
    ThreadPool&                                               threads;
    TranspositionTable&                                       tt;
    std::map<NumaIndex, SharedHistories>&                     sharedHistories;

	
};




struct InfoShort {
    int   depth;
    Score score;
};



struct InfoFull: InfoShort {
    
    int selDepth;

    
    size_t multiPV;

#if STOCKFISH
    
    std::string_view wdl;
#endif

    
	
    std::string_view bound;

    
    size_t timeMs;

    
    size_t nodes;

    
    size_t nps;

#if STOCKFISH
    
    size_t           tbHits;
#endif

    
    std::string_view pv;

    
    int hashfull;
};


struct InfoIteration {
    
    int depth;

	
    std::string_view currmove;

	
    size_t currmovenumber;
};








using UpdateShort    = std::function<void(const InfoShort&)>;
using UpdateFull     = std::function<void(const InfoFull&)>;
using UpdateIter     = std::function<void(const InfoIteration&)>;
using UpdateBestmove = std::function<void(std::string_view, std::string_view)>;
using UpdateInfo     = std::function<void(std::string_view)>;


struct UpdateContext {
    UpdateShort    onUpdateNoMoves;  
    UpdateFull     onUpdateFull;     
    UpdateIter     onIter;           
    UpdateBestmove onBestmove;       
    UpdateInfo     onUpdateString;   
};











class Worker
{
public:

	Worker(SharedState& sharedState,
#if STOCKFISH
	,std::unique_ptr<ISearchManager> sm,
	size_t threadIdx,
	size_t numaThreadIdx,
	size_t numaTotal,
	NumaReplicatedAccessToken numaAccessToken)
#else
          
          const ThreadIds& ids
#endif
	);

	virtual ~Worker() { }

	
    
	
    

	
	
	
    virtual void clear() {}

	
    
    
    

	
	
	
	
	
	
	virtual void start_searching(){}

	
    
	virtual void pre_start_searching() {}

	
	bool is_mainthread() const { return threadIdx == 0; }

	
	virtual void ensure_network_replicated() {}

	
#if STOCKFISH
    
    ButterflyHistory mainHistory;
    LowPlyHistory    lowPlyHistory;

    CapturePieceToHistory           captureHistory;
    ContinuationHistory             continuationHistory[2][2];
    CorrectionHistory<Continuation> continuationCorrectionHistory;

    TTMoveHistory    ttMoveHistory;
    SharedHistories& sharedHistory;
#endif

protected:

	
	
	

#if STOCKFISH
    

	void do_move(Position& pos, const Move move, StateInfo& st);
	void do_move(Position& pos, const Move move, StateInfo& st, const bool givesCheck);
	void do_null_move(Position& pos, StateInfo& st);
	void undo_move(Position& pos, const Move move);
	void undo_null_move(Position& pos);

    
    template<NodeType nodeType>
    Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);

    
    template<NodeType nodeType>
    Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta);

    Depth reduction(bool i, Depth d, int mn, int delta) const;

    
    SearchManager* main_manager() const {
        assert(threadIdx == 0);
        return static_cast<SearchManager*>(manager.get());
    }

    TimePoint elapsed() const;
    TimePoint elapsed_time() const;

    Value evaluate(const Position&);
#endif

    
	LimitsType limits;

	
    

	
    
    
    std::atomic<uint64_t> nodes ;

	
#if STOCKFISH
    int selDepth, nmpMinPly;

    Value optimism[COLOR_NB];
#endif

	
    
public:

	
    Position rootPos;

    
    StateInfo rootState;

    
    RootMoves rootMoves;
    
    
    std::vector<Move> rootMoveKeys;

protected:

	
#if STOCKFISH
    
    

    
    
#endif

    
	
	
	
	
	
    size_t                    threadIdx, numaThreadIdx, numaTotal;
    NumaReplicatedAccessToken numaAccessToken;

	
    
    

	
    

	
    
    const OptionsMap& options;

    
    
	ThreadPool& threads;

	
    TranspositionTable& tt;

#if defined(EVAL_SFNN)
	

	const LazyNumaReplicatedSystemWide<Eval::NNUE::Networks>& networks;

    
    Eval::NNUE::AccumulatorStack  accumulatorStack;
    Eval::NNUE::AccumulatorCaches refreshTable;
#endif

	friend class YaneuraOu::ThreadPool;
#if STOCKFISH
    friend class SearchManager;
#endif
};


} 
} 


#endif 

