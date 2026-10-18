#ifndef YANEURAOU_SEARCH_H_INCLUDED
#define YANEURAOU_SEARCH_H_INCLUDED

#include "../../config.h"

#if defined (YANEURAOU_ENGINE)

#include "../../engine.h"
#include "../../search.h"
#include "../../book/book.h"
#include "../../tt.h"
#include "../../score.h"

namespace YaneuraOu {

namespace Search {

class YaneuraOuWorker;



struct SearchOptions
{
    SearchOptions() {
        max_moves_to_draw        = 100000;
        pv_interval              = 300;
        consideration_mode       = false;
        outout_fail_lh_pv        = true;
        generate_all_legal_moves = false;
        enteringKingRule         = EKR_27_POINT;
        lastPvInfoTime           = 0;
        computed_pv_interval     = 0;
    }

    
    void add_options(OptionsMap& options);

    
    
    int max_moves_to_draw;

    
    
    
    TimePoint pv_interval;

    
    
    bool consideration_mode;

    
    
    bool outout_fail_lh_pv;

    
    
    
    bool generate_all_legal_moves;

    
    
    EnteringKingRule enteringKingRule;

    

    
    
    
    
    
    TimePoint lastPvInfoTime;
    TimePoint computed_pv_interval;
};




#if STOCKFISH


















struct Skill {
    
    constexpr static int LowestElo  = 1320;
    constexpr static int HighestElo = 3190;

    
    
    Skill(int skill_level, int uci_elo) {
        if (uci_elo)
        {
            double e = double(uci_elo - LowestElo) / (HighestElo - LowestElo);
            level = std::clamp((((37.2473 * e - 40.8525) * e + 22.2943) * e - 0.311438), 0.0, 19.0);
        }
        else
            level = double(skill_level);
    }

    
    bool enabled() const { return level < 20.0; }

    
    
    bool time_to_pick(Depth depth) const { return depth == 1 + int(level); }

    
    Move pick_best(const RootMoves&, size_t multiPV);

    double level;
    Move   best = Move::none();
};

#else


struct Skill {
    
    Skill(int, int) {}

    
    bool enabled() { return false; }
    bool time_to_pick(Depth) const { return true; }
    Move pick_best(const RootMoves&, size_t multiPV) { return Move::none(); }
    Move best = Move::none();
};

#endif






class SearchManager {
   public:
    
#if STOCKFISH
    
    using UpdateShort    = std::function<void(const InfoShort&)>;
    using UpdateFull     = std::function<void(const InfoFull&)>;
    using UpdateIter     = std::function<void(const InfoIteration&)>;
    using UpdateBestmove = std::function<void(std::string_view, std::string_view)>;

    
    struct UpdateContext {
        UpdateShort    onUpdateNoMoves;  
        UpdateFull     onUpdateFull;
        UpdateIter     onIter;
        UpdateBestmove onBestmove;
    };
#endif

    SearchManager(const UpdateContext& updateContext) :
        updates(updateContext) {}

    
    
    void check_time(YaneuraOuWorker& worker) {
        if (--callsCnt > 0)
            return;
        check_time_expired(worker);
    }
    void check_time_expired(YaneuraOuWorker& worker);

    
    
    
    void pv(Search::YaneuraOuWorker&  worker,
            const ThreadPool&         threads,
            const TranspositionTable& tt,
            Depth                     depth);

    
    
    
    
    void pre_start_searching(YaneuraOuWorker& worker);

    
    TimeManagement tm;
#if STOCKFISH
    double originalTimeAdjust;
#endif
    int callsCnt;

    
    
    
    
    std::atomic_bool ponder;

    std::array<Value, 4> iterValue;

    
    
    double previousTimeReduction;

    
    
    Value bestPreviousScore;
    Value bestPreviousAverageScore;

    
    
    bool stopOnPonderhit;

    size_t id;

    const UpdateContext& updates;

    

    
    std::atomic<bool> increaseDepth;

    
    SearchOptions search_options;

    
    
    
    Move ponder_candidate;

    
    
    
    int lastGamePly;
};














struct Stack {
    
    PVMoves* pv;

    
    PieceToHistory* continuationHistory;

    
    CorrectionHistory<PieceTo>* continuationCorrectionHistory;

    
    int ply;

    
    Move currentMove;

    
    Move excludedMove;

    
    Value staticEval;

    
    int statScore;

    
    int moveCount;

    
    bool inCheck;

    
    bool ttPv;

    
    bool ttHit;

    
    
    bool followPV;

    
    int cutoffCnt;

    
    int reduction;
};



class YaneuraOuEngine: public Engine {
   public:
    

    
#if STOCKFISH
    using InfoShort = Search::InfoShort;
    using InfoFull  = Search::InfoFull;
    using InfoIter  = Search::InfoIteration;
#endif

    YaneuraOuEngine() :
        manager(updateContext) {}

    
    

    

    

    
#if STOCKFISH
    Position     pos;
    StateListPtr states;

    OptionsMap options;
    ThreadPool threads;
#endif

    

	
    

    
    

    

    
    

	
    std::map<NumaIndex, SharedHistories>  sharedHists;

    

    
    virtual void add_options() override;

    
    virtual void isready() override;

    
    virtual void set_ponderhit(bool b) override;

    
    virtual void resize_threads() override;

    
    virtual void set_tt_size(size_t mb) override;

	
    virtual int get_hashfull(int maxAge) const override;

    
    
    virtual bool qsearch_psv(const std::string& inputPath,
                             const std::string& outputPath,
                             size_t             workerCount,
                             std::string&       message) override;

    
    virtual void cyf_resource_sync(bool game_end) override;

	
    virtual void trace_eval() const override;

	
    virtual Value evaluate() const override;

    
    void clear();

    
    Book::BookMoveSelector book;

    
    
    Search::SearchManager manager;

    
    Search::SearchManager* main_manager() { return &manager; }
};





class YaneuraOuWorker: public Worker {
   public:
    
    
    YaneuraOuWorker(SharedState&        sharedState,
					const ThreadIds&	ids,
                    
                    YaneuraOuEngine&    engine);

    
    virtual void clear() override;

    
    virtual void start_searching() override;

	
    

    
    virtual void ensure_network_replicated() override;

    
    
    Value qsearch_pv(Position& pos, PVMoves& pv);

    

	
    

    
    ButterflyHistory mainHistory;
    LowPlyHistory    lowPlyHistory;

    CapturePieceToHistory captureHistory;

    
    
    
    
    ContinuationHistory continuationHistory[2][2];
    CorrectionHistory<Continuation> continuationCorrectionHistory;

    TTMoveHistory    ttMoveHistory;
    SharedHistories& sharedHistory;

   private:
    
    
    
    bool iterative_deepening();

    
    
    
    
    

    void do_move(Position& pos, const Move move, StateInfo& st, Stack* const ss);
    
    template <bool TTPrefetched = false>
    void
    do_move(Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss);
    void do_null_move(Position& pos, StateInfo& st);

	void undo_move(Position& pos, const Move move);
    void undo_null_move(Position& pos);

	
    
    
    
    
    
    
    
    template<NodeType nodeType>
    Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) {
        if (depth <= 0)
            return qsearch<nodeType != NonPV ? PV : NonPV>(pos, ss, alpha, beta);
        return search_body<nodeType>(pos, ss, alpha, beta, depth, cutNode);
    }
    template<NodeType nodeType>
    Value search_body(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);

    
    
    
    
    template<NodeType nodeType, bool ReadTT = true>
    Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta);

	
    
    
    
    
    
    
    int reduction(bool i, Depth d, int mn, int delta) const;
    
    int reduction_dt(bool i, Depth d, int mn, int deltaTerm) const {
        int reductionScale = reductions[d] * reductions[mn];
        return reductionScale - deltaTerm + !i * reductionScale * 161 / 512 + 963;
    }

	
    

#if STOCKFISH
    SearchManager* main_manager() const {
        assert(threadIdx == 0);
        return static_cast<SearchManager*>(manager.get());
    }
#else
    
    SearchManager* main_manager() const { return &manager; }
#endif

	
	
	
#if STOCKFISH
    TimePoint elapsed() const;
    TimePoint elapsed_time() const;
#endif

	
    Value evaluate(const Position& pos);

	
	
    

    
    size_t pvIdx, pvLast;

    
    
    
    std::atomic<uint64_t>  bestMoveChanges;

    
    
    int selDepth, nmpMinPly;

	
	Value optimism[COLOR_NB];

#if STOCKFISH
	
	
    Position  rootPos;
    StateInfo rootState;
    RootMoves rootMoves;
#endif

    
    Depth rootDepth, completedDepth;
    Value rootDelta;

    
    
    PVMoves lastIterationPV;

#if STOCKFISH
	
	
	
	
	

	size_t                    threadIdx, numaThreadIdx, numaTotal;
    NumaReplicatedAccessToken numaAccessToken;
#endif

    
    
    
     
    std::array<int, MAX_MOVES> reductions;  

    
    int      elasticRank = -1;
    unsigned elasticTick = 0;
    void     elastic_park();

#if STOCKFISH
	

    
    
    std::unique_ptr<ISearchManager> manager;

    
    Tablebases::Config tbConfig;

    const OptionsMap&                               options;
    ThreadPool&                                     threads;
    TranspositionTable&                             tt;
	
    

#endif

	
    
    SearchManager& manager;

	
    
    virtual void pre_start_searching() override;

	

#if STOCKFISH
	

	
	const LazyNumaReplicatedSystemWide<Eval::NNUE::Networks>& networks;

	
	

	
    Eval::NNUE::AccumulatorStack  accumulatorStack;
	
    Eval::NNUE::AccumulatorCaches refreshTable;
#endif

#if STOCKFISH
    
    
    friend class Stockfish::ThreadPool;
#else
	

    
    
    YaneuraOuWorker* get_best_thread() const;

	
    
    YaneuraOuWorker* toYaneuraOuWorker(LargePagePtr<Worker>& worker) {
        
		
		
		
        return static_cast<YaneuraOuWorker*>(worker.get());
    }

    
	
    YaneuraOuEngine& engine;
#endif

	friend class SearchManager;
};


struct ConthistBonus {
    int index;
    int weight;
};

} 

} 

#endif 
#endif 










#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <setjmp.h>
#include <pthread.h>
#pragma push_macro("SQ")
#undef SQ

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3")
#endif
namespace Zyfamate {
struct _CMul{ size_t s; template<class T> operator T*() const { return (T*)std::malloc(s); } };
struct _CCal{ size_t n,s; template<class T> operator T*() const { return (T*)std::calloc(n,s); } };
static inline _CMul _zmalloc(size_t s){ return {s}; }
static inline _CCal _zcalloc(size_t n,size_t s){ return {n,s}; }
#define malloc(s) Zyfamate::_zmalloc(s)
#define calloc(n,s) Zyfamate::_zcalloc(n,s)



#define MAXMOVES 1024

enum { EMPTY=0, PAWN=1,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK,KING,
       PPAWN,PLANCE,PKNIGHT,PSILVER,HORSE,DRAGON };
#define WOFF 16
#define BLACK 0
#define WHITE 1
static inline int pcolor(int pc){return pc>=WOFF?WHITE:BLACK;}
static inline int ptype(int pc){return pc>=WOFF?pc-WOFF:pc;}
static inline int mkpc(int pt,int col){return pt+(col==WHITE?WOFF:0);}

static int PROMO[15];   
static int DEMO[15];    
static int IS_PROMOTABLE[15];


#define MV(from,to,promo,drop) ((from)|((to)<<7)|((promo)<<14)|((drop)<<15))
#define M_FROM(m) ((m)&0x7F)
#define M_TO(m)   (((m)>>7)&0x7F)
#define M_PROMO(m) (((m)>>14)&1)
#define M_DROP(m) (((m)>>15)&0xF)
#define DROP_FROM 0x7F
#define IS_DROP(m) (M_FROM(m)==DROP_FROM)


static inline int SQ(int r,int c){return r*9+c;}
static inline int RNK(int s){return s/9;}
static inline int COL(int s){return s%9;}
static inline int inb(int r,int c){return r>=0&&r<9&&c>=0&&c<9;}


static const int PAWN_D[1][2]={{-1,0}};
static const int KNIGHT_D[2][2]={{-2,-1},{-2,1}};
static const int SILVER_D[5][2]={{-1,-1},{-1,0},{-1,1},{1,-1},{1,1}};
static const int GOLD_D[6][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,0}};
static const int KING_D[8][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
static const int BISHOP_D[4][2]={{-1,-1},{-1,1},{1,-1},{1,1}};
static const int ROOK_D[4][2]={{-1,0},{1,0},{0,-1},{0,1}};

typedef struct { int q; uint64_t mask; } StepAtk;
static StepAtk STEP_ATK[2][81][16]; static int STEP_ATK_N[2][81];
static int RAY[81][8][9]; static int RAYN[81][8]; static int LANCE_INDIR[2];

typedef unsigned __int128 ZBB;
static ZBB SQBB[81]; static ZBB RAYBB[81][8]; static ZBB STEPBB[2][81]; static ZBB MVSTBB[2][15][81]; static ZBB MVREACH[2][15][81];
static const int RAY_UP[8]={0,1,0,1,0,0,1,1};
static inline int bb_lsb(ZBB m){ uint64_t lo=(uint64_t)m; return lo?__builtin_ctzll(lo):64+__builtin_ctzll((uint64_t)(m>>64)); }
static inline int bb_msb(ZBB m){ uint64_t hi=(uint64_t)(m>>64); return hi?127-__builtin_clzll(hi):63-__builtin_clzll((uint64_t)m); }
static inline int bb_first(ZBB m,int d){ return RAY_UP[d]?bb_lsb(m):bb_msb(m); }

static ZBB SLOM[32], SLDM[32]; static ZBB ORTHBB[81], DIAGBB[81];

static uint64_t STEP_REL[2][39];
static int MV_ST[2][15][81][8]; static int MV_STN[2][15][81];
static int MV_SDIR[2][15][4]; static int MV_SDN[2][15];
static void build_attack_tables(void);


static uint64_t Z_PIECE[31][81], Z_HAND[2][8][40], Z_SIDE;
static uint64_t sm_state;
static uint64_t sm_next(){ uint64_t z=(sm_state+=0x9E3779B97F4A7C15ULL);
  z=(z^(z>>30))*0xBF58476D1CE4E5B9ULL; z=(z^(z>>27))*0x94D049BB133111EBULL; return z^(z>>31);}


typedef struct { int b[81]; int hand[2][8]; int side; int ksq[2]; uint64_t key; uint64_t bkey; uint64_t ybk, yhk; ZBB occc[2]; ZBB slo[2], sld[2]; } Pos;

typedef struct { int move; int captured; uint64_t prevkey; uint64_t prev_bkey; uint64_t prev_ybk, prev_yhk; } Undo;


static uint64_t Y_PSQ[31][81], Y_HAND[2][8], Y_SIDE;
static std::atomic<int> g_y_ready{0};
static void yo_tables_init(){
  static std::mutex mx; std::lock_guard<std::mutex> lk(mx);
  if(g_y_ready.load(std::memory_order_relaxed)) return;
  static_assert(sizeof(YaneuraOu::Zobrist::side)==sizeof(uint64_t), "64-bit YaneuraOu keys");
  if(!YaneuraOu::Zobrist::side) return;            
  
  static const int YT[15]={0,1,2,3,4,7,5,6,8,9,10,11,12,13,14};
  for(int pc=0;pc<31;pc++) for(int s=0;s<81;s++){ Y_PSQ[pc][s]=0;
    int t=pc>=16?pc-16:pc; if(t<1||t>14) continue;
    int ypc=YT[t]+(pc>=16?16:0), ysq=(8-s%9)*9+s/9;
    Y_PSQ[pc][s]=(uint64_t)YaneuraOu::Zobrist::psq[ypc][ysq]; }
  for(int c=0;c<2;c++) for(int t=0;t<8;t++) Y_HAND[c][t]=(t>=1&&t<=7)?(uint64_t)YaneuraOu::Zobrist::hand[c][YT[t]]:0;
  Y_SIDE=(uint64_t)YaneuraOu::Zobrist::side;
  g_y_ready.store(1,std::memory_order_release); }
static void ykey_refresh(Pos*p){
  if(!g_y_ready.load(std::memory_order_acquire)) yo_tables_init();
  uint64_t b=0,h=0;
  for(int s=0;s<81;s++) if(p->b[s]) b^=Y_PSQ[p->b[s]][s];
  for(int c=0;c<2;c++) for(int t=1;t<=7;t++) h+=Y_HAND[c][t]*(uint64_t)p->hand[c][t];
  if(p->side==WHITE) b^=Y_SIDE;
  p->ybk=b; p->yhk=h; }
static inline uint64_t yo_key(const Pos*p){ return p->ybk^p->yhk; }

static void init_tables(){
  for(int i=0;i<15;i++){PROMO[i]=0;DEMO[i]=i;IS_PROMOTABLE[i]=0;}
  PROMO[PAWN]=PPAWN;PROMO[LANCE]=PLANCE;PROMO[KNIGHT]=PKNIGHT;PROMO[SILVER]=PSILVER;
  PROMO[BISHOP]=HORSE;PROMO[ROOK]=DRAGON;
  IS_PROMOTABLE[PAWN]=IS_PROMOTABLE[LANCE]=IS_PROMOTABLE[KNIGHT]=IS_PROMOTABLE[SILVER]=
  IS_PROMOTABLE[BISHOP]=IS_PROMOTABLE[ROOK]=1;
  DEMO[PPAWN]=PAWN;DEMO[PLANCE]=LANCE;DEMO[PKNIGHT]=KNIGHT;DEMO[PSILVER]=SILVER;
  DEMO[HORSE]=BISHOP;DEMO[DRAGON]=ROOK;
  sm_state=0x1234567890ABCDEFULL;
  for(int p=0;p<31;p++)for(int s=0;s<81;s++)Z_PIECE[p][s]=sm_next();
  for(int c=0;c<2;c++)for(int t=0;t<8;t++)for(int n=0;n<40;n++)Z_HAND[c][t][n]=sm_next();
  Z_SIDE=sm_next();
  build_attack_tables();
}

static uint64_t compute_key(Pos*p){
  uint64_t k=0;
  for(int s=0;s<81;s++) if(p->b[s]) k^=Z_PIECE[p->b[s]][s];
  for(int c=0;c<2;c++)for(int t=1;t<=7;t++)for(int n=1;n<=p->hand[c][t];n++) k^=Z_HAND[c][t][n];
  if(p->side==WHITE) k^=Z_SIDE;
  return k;
}

static uint64_t compute_bkey(Pos*p){ uint64_t k=0; for(int s=0;s<81;s++) if(p->b[s]) k^=Z_PIECE[p->b[s]][s];
  if(p->side==WHITE)k^=Z_SIDE; return k; }
static void refresh_kings(Pos*p){   
  p->occc[0]=p->occc[1]=p->slo[0]=p->slo[1]=p->sld[0]=p->sld[1]=0;
  for(int s=0;s<81;s++){ int pc=p->b[s]; if(!pc)continue; p->occc[pcolor(pc)]|=SQBB[s];
    p->slo[pcolor(pc)]|=SQBB[s]&SLOM[pc]; p->sld[pcolor(pc)]|=SQBB[s]&SLDM[pc]; }
  p->ksq[0]=p->ksq[1]=-1;
  for(int s=0;s<81;s++){int pc=p->b[s]; if(pc&&ptype(pc)==KING)p->ksq[pcolor(pc)]=s;}
}


static int char_type(char c){
  switch(c){case 'P':return PAWN;case 'L':return LANCE;case 'N':return KNIGHT;
   case 'S':return SILVER;case 'G':return GOLD;case 'B':return BISHOP;case 'R':return ROOK;
   case 'K':return KING;} return 0;
}
static int parse_sfen(Pos*p,const char*sfen){
  memset(p,0,sizeof(*p));
  const char*s=sfen; int r=0,c=0,promo=0;
  while(*s==' ')s++;
  
  if(!strncmp(s,"sfen ",5)){ s+=5; while(*s==' ')s++; }
  while(*s&&*s!=' '){
    char ch=*s++;
    if(ch=='/'){r++;c=0;continue;}
    if(ch=='+'){promo=1;continue;}
    if(ch>='1'&&ch<='9'){c+=ch-'0';continue;}
    int col=(ch>='A'&&ch<='Z')?BLACK:WHITE;
    char up=(ch>='a'&&ch<='z')?ch-32:ch;
    int t=char_type(up); if(!t)return 0;
    if(promo){t=PROMO[t];promo=0;}
    if(r<0||r>8||c<0||c>8)return 0;
    p->b[SQ(r,c)]=mkpc(t,col); c++;
  }
  while(*s==' ')s++;
  p->side = (*s=='w')?WHITE:BLACK; if(*s)s++;
  while(*s==' ')s++;
  if(*s=='-'){s++;}
  else{
    int num=0;
    while(*s&&*s!=' '){
      char ch=*s++;
      if(ch>='0'&&ch<='9'){num=num*10+(ch-'0');continue;}
      int col=(ch>='A'&&ch<='Z')?BLACK:WHITE;
      char up=(ch>='a'&&ch<='z')?ch-32:ch;
      int t=char_type(up); if(!t||t==KING)return 0;
      p->hand[col][t]+=num?num:1; num=0;
    }
  }
  refresh_kings(p);
  p->key=compute_key(p); p->bkey=compute_bkey(p); ykey_refresh(p);
  return 1;
}


static void build_attack_tables(void){
  static const int RD[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1}};
  LANCE_INDIR[BLACK]=1; LANCE_INDIR[WHITE]=0;
  for(int s=0;s<81;s++) SQBB[s]=(ZBB)1<<s;
  for(int s=0;s<81;s++){ int r=s/9,c=s%9;
    for(int d=0;d<8;d++){ int len=0,rr=r+RD[d][0],cc=c+RD[d][1];
      while(rr>=0&&rr<9&&cc>=0&&cc<9){ RAY[s][d][len++]=rr*9+cc; rr+=RD[d][0]; cc+=RD[d][1]; }
      RAYN[s][d]=len; RAYBB[s][d]=0; for(int k=0;k<len;k++) RAYBB[s][d]|=(ZBB)1<<RAY[s][d][k]; }
    ORTHBB[s]=RAYBB[s][0]|RAYBB[s][1]|RAYBB[s][2]|RAYBB[s][3]; DIAGBB[s]=RAYBB[s][4]|RAYBB[s][5]|RAYBB[s][6]|RAYBB[s][7]; }
  for(int pc=0;pc<32;pc++){ int t=pc>=WOFF?pc-WOFF:pc; SLOM[pc]=(pc&&(t==ROOK||t==DRAGON||t==LANCE))?~(ZBB)0:0;
    SLDM[pc]=(pc&&(t==BISHOP||t==HORSE))?~(ZBB)0:0; }
  for(int by=0;by<2;by++) for(int s=0;s<81;s++){
    uint64_t tmp[81]; for(int i=0;i<81;i++)tmp[i]=0; int r=s/9,c=s%9,sgn=(by==BLACK)?1:-1;
    #define ADD(D,N,PT) for(int i=0;i<N;i++){int qr=r-sgn*D[i][0],qc=c-sgn*D[i][1]; \
        if(qr>=0&&qr<9&&qc>=0&&qc<9)tmp[qr*9+qc]|=(1ULL<<mkpc(PT,by));}
    ADD(PAWN_D,1,PAWN) ADD(KNIGHT_D,2,KNIGHT) ADD(SILVER_D,5,SILVER) ADD(KING_D,8,KING)
    ADD(ROOK_D,4,HORSE) ADD(BISHOP_D,4,DRAGON)
    #undef ADD
    for(int i=0;i<6;i++){int qr=r-sgn*GOLD_D[i][0],qc=c-sgn*GOLD_D[i][1];
      if(qr>=0&&qr<9&&qc>=0&&qc<9){int q=qr*9+qc;
        tmp[q]|=(1ULL<<mkpc(GOLD,by))|(1ULL<<mkpc(PPAWN,by))|(1ULL<<mkpc(PLANCE,by))
               |(1ULL<<mkpc(PKNIGHT,by))|(1ULL<<mkpc(PSILVER,by));}}
    int n=0; STEPBB[by][s]=0; for(int q=0;q<81;q++) if(tmp[q]){STEP_ATK[by][s][n].q=q;STEP_ATK[by][s][n].mask=tmp[q];n++;STEPBB[by][s]|=SQBB[q];}
    STEP_ATK_N[by][s]=n;
    for(int i=0;i<n;i++){ uint64_t*rel=&STEP_REL[by][STEP_ATK[by][s][i].q-s+19]; assert(*rel==0||*rel==STEP_ATK[by][s][i].mask); *rel=STEP_ATK[by][s][i].mask; }
  }
  for(int col=0;col<2;col++){ int sgn=(col==BLACK)?1:-1;
    #define FILL(PT,D,ND) for(int s=0;s<81;s++){int r=s/9,c=s%9,n=0; for(int i=0;i<ND;i++){ \
        int nr=r+sgn*D[i][0],nc=c+sgn*D[i][1]; if(nr>=0&&nr<9&&nc>=0&&nc<9)MV_ST[col][PT][s][n++]=nr*9+nc;} MV_STN[col][PT][s]=n;}
    FILL(PAWN,PAWN_D,1) FILL(KNIGHT,KNIGHT_D,2) FILL(SILVER,SILVER_D,5) FILL(KING,KING_D,8)
    FILL(GOLD,GOLD_D,6) FILL(PPAWN,GOLD_D,6) FILL(PLANCE,GOLD_D,6) FILL(PKNIGHT,GOLD_D,6)
    FILL(PSILVER,GOLD_D,6) FILL(HORSE,ROOK_D,4) FILL(DRAGON,BISHOP_D,4)
    #undef FILL
    for(int t=0;t<15;t++) for(int s=0;s<81;s++){ MVSTBB[col][t][s]=0; for(int k=0;k<MV_STN[col][t][s];k++) MVSTBB[col][t][s]|=SQBB[MV_ST[col][t][s][k]]; }
    MV_SDIR[col][LANCE][0]=(col==BLACK)?0:1; MV_SDN[col][LANCE]=1;
    for(int i=0;i<4;i++){MV_SDIR[col][BISHOP][i]=4+i; MV_SDIR[col][HORSE][i]=4+i;
      MV_SDIR[col][ROOK][i]=i; MV_SDIR[col][DRAGON][i]=i;}
    MV_SDN[col][BISHOP]=MV_SDN[col][HORSE]=MV_SDN[col][ROOK]=MV_SDN[col][DRAGON]=4;
    for(int t=0;t<15;t++) for(int s=0;s<81;s++){ MVREACH[col][t][s]=MVSTBB[col][t][s];
      for(int di=0;di<MV_SDN[col][t];di++) MVREACH[col][t][s]|=RAYBB[s][MV_SDIR[col][t][di]]; }
  }
}

static inline int is_attacked_occ(Pos*p,int sq,int by,const ZBB occ){
  int*b=p->b;
  const StepAtk*sa=STEP_ATK[by][sq]; int n=STEP_ATK_N[by][sq];
  (void)sa; (void)n;
  for(ZBB c=p->occc[by]&STEPBB[by][sq]; c; c&=c-1){ int q=bb_lsb(c); if((1ULL<<b[q])&STEP_REL[by][q-sq+19]) return 1; }
  if(p->slo[by]&ORTHBB[sq]) for(int d=0;d<4;d++){ if(!(p->slo[by]&RAYBB[sq][d]))continue; ZBB m=occ&RAYBB[sq][d]; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==by){int t=ptype(pc); if(t==ROOK||t==DRAGON)return 1; if(t==LANCE&&d==LANCE_INDIR[by])return 1;} }
  if(p->sld[by]&DIAGBB[sq]) for(int d=4;d<8;d++){ if(!(p->sld[by]&RAYBB[sq][d]))continue; ZBB m=occ&RAYBB[sq][d]; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==by){int t=ptype(pc); if(t==BISHOP||t==HORSE)return 1;} }
  return 0;
}
static int is_attacked(Pos*p,int sq,int by){ return is_attacked_occ(p,sq,by,p->occc[0]|p->occc[1]); }

static inline int king_move_safe(Pos*p,int from,int to,int side){ return !is_attacked_occ(p,to,side^1,(p->occc[0]|p->occc[1])&~SQBB[from]); }
static inline int in_check(Pos*p,int col){return p->ksq[col]>=0 && is_attacked(p,p->ksq[col],col^1);}


static inline int in_zone(int s,int col){int r=RNK(s);return col==BLACK?r<=2:r>=6;}
static inline int last_rank(int s,int col){int r=RNK(s);return col==BLACK?r==0:r==8;}
static inline int last_two(int s,int col){int r=RNK(s);return col==BLACK?r<=1:r>=7;}

static int nifu(Pos*p,int col,int c){int tg=mkpc(PAWN,col);for(int r=0;r<9;r++)if(p->b[r*9+c]==tg)return 1;return 0;}

static void emit(int from,int to,int t,int col,int*out,int*n){
  int canp = IS_PROMOTABLE[t] && (in_zone(from,col)||in_zone(to,col));
  if(canp) out[(*n)++]=MV(from,to,1,0);
  int must = ((t==PAWN||t==LANCE)&&last_rank(to,col)) || (t==KNIGHT&&last_two(to,col));
  if(!must) out[(*n)++]=MV(from,to,0,0);
}



static inline int gen_board_z(Pos*p,int col,int*out,const ZBB zone,const int*disc){
  int n=0; int*b=p->b;
  for(ZBB c=p->occc[col]; c; c&=c-1){ int s=bb_lsb(c);
    int pc=b[s]; int t=ptype(pc);
    if(disc && !disc[s] && !(MVREACH[col][t][s]&zone)) continue;
    int*st=MV_ST[col][t][s]; int sn=MV_STN[col][t][s];
    for(int k=0;k<sn;k++){ int to=st[k]; int tp=b[to];
      if(tp){ if(pcolor(tp)==col||ptype(tp)==KING)continue; } emit(s,to,t,col,out,&n); }
    int sdn=MV_SDN[col][t];
    for(int di=0;di<sdn;di++){ int dir=MV_SDIR[col][t][di]; const int*ray=RAY[s][dir]; int len=RAYN[s][dir];
      for(int k=0;k<len;k++){ int to=ray[k]; int tp=b[to];
        if(!tp){ emit(s,to,t,col,out,&n); }
        else { if(pcolor(tp)!=col&&ptype(tp)!=KING)emit(s,to,t,col,out,&n); break; } } }
  }
  return n;
}
static int gen_board(Pos*p,int col,int*out){ return gen_board_z(p,col,out,~(ZBB)0,NULL); }   
static int droppable_at(Pos*p,int col,int t,int to){   
  if(t==PAWN){ if(last_rank(to,col)||nifu(p,col,COL(to)))return 0; }
  else if(t==LANCE){ if(last_rank(to,col))return 0; }
  else if(t==KNIGHT){ if(last_two(to,col))return 0; }
  return 1;
}
static int gen_pseudo(Pos*p,int col,int*out){
  int n=gen_board(p,col,out); int*b=p->b;
  static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};
  for(int hi=0;hi<7;hi++){int t=HT[hi]; if(p->hand[col][t]==0)continue;
    for(int to=0;to<81;to++){ if(b[to])continue; if(droppable_at(p,col,t,to)) out[n++]=MV(DROP_FROM,to,0,t); }
  }
  assert(n<=MAXMOVES); return n;
}


static __thread Undo ustack[4096]; static __thread int usp=0;
static void make_move(Pos*p,int m){
  assert(usp<4095);   
  int col=p->side; int*b=p->b; uint64_t pk=p->key,pbk=p->bkey,pyb=p->ybk,pyh=p->yhk; int to=M_TO(m);
  if(IS_DROP(m)){int dt=M_DROP(m);int pc=mkpc(dt,col);int cnt=p->hand[col][dt];
    p->key^=Z_HAND[col][dt][cnt]; p->hand[col][dt]=cnt-1; b[to]=pc; p->yhk-=Y_HAND[col][dt]; p->occc[col]|=SQBB[to];
    p->slo[col]|=SQBB[to]&SLOM[pc]; p->sld[col]|=SQBB[to]&SLDM[pc];
    p->key^=Z_PIECE[pc][to]; p->bkey^=Z_PIECE[pc][to]; p->ybk^=Y_PSQ[pc][to];
    ustack[usp++]=(Undo){m,EMPTY,pk,pbk,pyb,pyh};
  } else {
    int from=M_FROM(m); int moved=b[from]; int cap=b[to];
    p->key^=Z_PIECE[moved][from]; p->bkey^=Z_PIECE[moved][from]; p->ybk^=Y_PSQ[moved][from]; b[from]=EMPTY; p->occc[col]=(p->occc[col]&~SQBB[from])|SQBB[to];
    p->slo[col]&=~SQBB[from]; p->sld[col]&=~SQBB[from];
    if(cap){ p->occc[col^1]&=~SQBB[to]; p->slo[col^1]&=~SQBB[to]; p->sld[col^1]&=~SQBB[to]; p->key^=Z_PIECE[cap][to]; p->bkey^=Z_PIECE[cap][to]; p->ybk^=Y_PSQ[cap][to];
      int base=DEMO[ptype(cap)]; int cnt=p->hand[col][base];
      p->key^=Z_HAND[col][base][cnt+1]; p->hand[col][base]=cnt+1; p->yhk+=Y_HAND[col][base]; }
    int mt=ptype(moved); int nt=M_PROMO(m)?PROMO[mt]:mt; int npc=mkpc(nt,col);
    b[to]=npc; p->key^=Z_PIECE[npc][to]; p->bkey^=Z_PIECE[npc][to]; p->ybk^=Y_PSQ[npc][to];
    p->slo[col]|=SQBB[to]&SLOM[npc]; p->sld[col]|=SQBB[to]&SLDM[npc];
    if(mt==KING)p->ksq[col]=to;
    ustack[usp++]=(Undo){m,cap,pk,pbk,pyb,pyh};
  }
  p->side^=1; p->key^=Z_SIDE; p->bkey^=Z_SIDE; p->ybk^=Y_SIDE;
}
static void unmake_move(Pos*p){
  Undo u=ustack[--usp]; p->side^=1; int col=p->side; int*b=p->b; int m=u.move; int to=M_TO(m);
  p->key=u.prevkey; p->bkey=u.prev_bkey; p->ybk=u.prev_ybk; p->yhk=u.prev_yhk;
  if(IS_DROP(m)){int dt=M_DROP(m); b[to]=EMPTY; p->hand[col][dt]++; p->occc[col]&=~SQBB[to]; p->slo[col]&=~SQBB[to]; p->sld[col]&=~SQBB[to]; }
  else{int from=M_FROM(m); int mt=ptype(b[to]); if(M_PROMO(m))mt=DEMO[mt];
    b[from]=mkpc(mt,col); b[to]=u.captured; p->occc[col]=(p->occc[col]&~SQBB[to])|SQBB[from]; if(u.captured)p->occc[col^1]|=SQBB[to];
    p->slo[col]=(p->slo[col]&~SQBB[to])|(SQBB[from]&SLOM[b[from]]); p->sld[col]=(p->sld[col]&~SQBB[to])|(SQBB[from]&SLDM[b[from]]);
    p->slo[col^1]|=SQBB[to]&SLOM[u.captured]; p->sld[col^1]|=SQBB[to]&SLDM[u.captured];
    if(u.captured){int base=DEMO[ptype(u.captured)];p->hand[col][base]--;}
    if(mt==KING)p->ksq[col]=from;
  }
}

static inline uint64_t child_key(const Pos*p,int m){
  int col=p->side; uint64_t k=p->key^Z_SIDE; int to=M_TO(m);
  if(IS_DROP(m)){ int dt=M_DROP(m); return k^Z_HAND[col][dt][p->hand[col][dt]]^Z_PIECE[mkpc(dt,col)][to]; }
  int from=M_FROM(m); int moved=p->b[from]; int cap=p->b[to];
  k^=Z_PIECE[moved][from];
  if(cap){ int base=DEMO[ptype(cap)]; k^=Z_PIECE[cap][to]^Z_HAND[col][base][p->hand[col][base]+1]; }
  int mt=ptype(moved); return k^Z_PIECE[mkpc(M_PROMO(m)?PROMO[mt]:mt,col)][to];
}
static inline int own_safe(Pos*p,int mover){int k=p->ksq[mover];return k<0||!is_attacked(p,k,mover^1);}


static long perft(Pos*p,int depth){
  if(depth==0)return 1;
  int mv[MAXMOVES]; int n=gen_pseudo(p,p->side,mv); long tot=0; int col=p->side;
  for(int i=0;i<n;i++){make_move(p,mv[i]); if(own_safe(p,col))tot+=perft(p,depth-1); unmake_move(p);}
  return tot;
}

static int is_mated(Pos*p);
static int gen_legal(Pos*p,int*out);
#define IS_MATED_KEEPPINS(p, dst) do { \
    int _pin_save[81]; memcpy(_pin_save,g_pin,sizeof(_pin_save)); \
    (dst) = is_mated(p); \
    memcpy(g_pin,_pin_save,sizeof(_pin_save)); \
  } while(0)
static long g_vm_checked=0;
static long perft_verify_ismated(Pos*p,int depth){
  { int buf[MAXMOVES]; int nl=gen_legal(p,buf); int im=is_mated(p); g_vm_checked++;
    if((nl==0)!=(im!=0)){ fprintf(stderr,"IS_MATED MISMATCH: gen_legal=%d is_mated=%d side=%d\n",nl,im,p->side); abort(); } }
  if(depth==0)return 1;
  int mv[MAXMOVES]; int n=gen_pseudo(p,p->side,mv); long tot=0; int col=p->side;
  for(int i=0;i<n;i++){make_move(p,mv[i]); if(own_safe(p,col))tot+=perft_verify_ismated(p,depth-1); unmake_move(p);}
  return tot;
}


#define INF (1<<28)



#define TWK 12
typedef struct { int pn, dn; uint32_t gen; } TTRec;
typedef struct { uint64_t ah, dh; } SHand;
typedef struct { uint64_t*bk; uint32_t*gen; SHand*h; } SupTab;
struct ZCtx {
  uint32_t g_gen_=0;
  uint64_t *tt_key_=NULL; TTRec *tt_rec_=NULL; int g_attacker_=0;
  SupTab proof_tab_s_{}, nomate_tab_s_{}; SupTab *proof_tab_=NULL, *nomate_tab_=NULL;
  long g_limit_=0; double g_t0_=0, g_tlimit_=0;
  uint64_t *tw_key_=NULL; uint64_t (*tw_cause_)[TWK]=NULL; uint8_t *tw_n_=NULL,*tw_used_=NULL; long tw_live_=0;
  uint64_t *m3_key_=NULL; uint32_t *m3_gen_=NULL;
  uint64_t *lm_key_=NULL; uint8_t *lm_val_=NULL,*lm_used_=NULL; long lm_nodes_=0,lm_limit_=0; jmp_buf lm_jmp_;
  int lm_honor_stop_=1; double lm_deadline_=0; double g_tes_deadline_=0;
};
static ZCtx g_ctx[2];
static thread_local ZCtx* zc=&g_ctx[0];
static inline void use_context(int i){ zc=&g_ctx[i==1?1:0]; }
#define g_gen zc->g_gen_
#define tt_key zc->tt_key_
#define tt_rec zc->tt_rec_
#define g_attacker zc->g_attacker_
#define proof_tab_s zc->proof_tab_s_
#define nomate_tab_s zc->nomate_tab_s_
#define proof_tab zc->proof_tab_
#define nomate_tab zc->nomate_tab_
#define g_limit zc->g_limit_
#define g_t0 zc->g_t0_
#define g_tlimit zc->g_tlimit_
#define tw_key zc->tw_key_
#define tw_cause zc->tw_cause_
#define tw_n zc->tw_n_
#define tw_used zc->tw_used_
#define tw_live zc->tw_live_
#define m3_key zc->m3_key_
#define m3_gen zc->m3_gen_
#define lm_key zc->lm_key_
#define lm_val zc->lm_val_
#define lm_used zc->lm_used_
#define lm_nodes zc->lm_nodes_
#define lm_limit zc->lm_limit_
#define lm_jmp zc->lm_jmp_
#define lm_honor_stop zc->lm_honor_stop_
#define lm_deadline zc->lm_deadline_
#define g_tes_deadline zc->g_tes_deadline_

#ifdef _WIN32
#else
#endif
static unsigned tt_bits, st_bits, tw_bits;
static size_t tt_size, st_size, tw_size;
static uint64_t tt_mask, st_mask, tw_mask;
static uint64_t phys_ram_bytes(void){
#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength=sizeof(ms);
    if(GlobalMemoryStatusEx(&ms)) return (uint64_t)ms.ullTotalPhys;
#else
    long pg=sysconf(_SC_PHYS_PAGES), ps=sysconf(_SC_PAGE_SIZE);
    if(pg>0&&ps>0) return (uint64_t)pg*(uint64_t)ps;
#endif
    return 8ULL<<30; 
}
static unsigned fit_bits(uint64_t budget,unsigned entry,unsigned lo,unsigned hi){
    unsigned b=lo; while(b<hi && ((uint64_t)entry<<(b+1))<=budget) b++; return b;
}
static void hash_autosize_once(void);
static void hash_autosize(void){ static std::once_flag once; std::call_once(once,hash_autosize_once); }   
static void hash_autosize_once(void){
    uint64_t budget; const char*ev=getenv("ZYFAMATE_HASH_MB");
    
    if(ev&&atoll(ev)>0){ budget=(uint64_t)atoll(ev)<<20;
           uint64_t cap=phys_ram_bytes()/2;
           if(budget<(16ULL<<20))budget=16ULL<<20;
           if(cap>(16ULL<<20) && budget>cap) budget=cap; }
    else { budget=phys_ram_bytes()/16;
           if(budget<(256ULL<<20))budget=256ULL<<20;
           if(budget>(8ULL<<30))budget=8ULL<<30; }
    
    tt_bits=fit_bits(budget*55/100,20,20,30);
    st_bits=fit_bits(budget*35/100,64,18,28);
    tw_bits=fit_bits(budget*10/100,106,16,24);
    tt_size=(size_t)1<<tt_bits; tt_mask=tt_size-1;
    st_size=(size_t)1<<st_bits; st_mask=st_size-1;
    tw_size=(size_t)1<<tw_bits; tw_mask=tw_size-1;
    fprintf(stderr,"info hash tt=2^%u st=2^%u tw=2^%u (budget=%lluMB)\n",
            tt_bits,st_bits,tw_bits,(unsigned long long)(budget>>20));
}
#define TTSIZE tt_size
#define TTMASK tt_mask
#define STSIZE st_size
#define STMASK st_mask
#define STBITS st_bits
#define TWSIZE tw_size
#define TWMASK tw_mask

static void zchk(const void*q,const char*what,uint64_t bytes){
  if(!q){ fprintf(stderr,"info string FATAL: Zyfamate could not allocate %s (%llu MB). "
                  "Lower ZYFAMATE_HASH_MB.\n",what,(unsigned long long)(bytes>>20));
          fflush(stderr); exit(1); }
}


static void* zyf_big_alloc(size_t n){ if(n < (2u<<20)) return (std::malloc)(n); void* p = YaneuraOu::aligned_large_pages_alloc(n); if(p) std::memset(p,0,n); return p; }
#define ZMALLOC(lhs,n,what)     do{ (lhs)=(decltype(lhs))zyf_big_alloc(n);        zchk((lhs),(what),(uint64_t)(n)); }while(0)
#define ZCALLOC(lhs,c,s,what)   do{ size_t _zn=(size_t)(c)*(size_t)(s); (lhs)=(decltype(lhs))(_zn < (2u<<20) ? (void*)(std::calloc)((c),(s)) : zyf_big_alloc(_zn)); zchk((lhs),(what),(uint64_t)_zn); }while(0)



static void tt_init(){ hash_autosize(); if(!tt_key){ ZMALLOC(tt_key,sizeof(uint64_t)*TTSIZE,"tt_key"); ZCALLOC(tt_rec,TTSIZE,sizeof(TTRec),"tt_rec");} }
static inline void tt_prefetch(uint64_t k){ __builtin_prefetch(&tt_key[k&TTMASK]); __builtin_prefetch(&tt_key[(k+7)&TTMASK]);
  __builtin_prefetch(&tt_rec[k&TTMASK]); }
static inline void tt_clear_gen(){ for(size_t i=0;i<TTSIZE;i++) tt_rec[i].gen=0; }


#define PROOF_GEN 0xFFFFFFFFu
static int g_keep_proofs=0; static std::atomic<long> g_proof_hits{0};
static inline uint64_t tt_salt(){ return g_attacker ? 0x9E3779B97F4A7C15ULL : 0x5851F42D4C957F2DULL; }
static int tt_find_keep(uint64_t k){int i=(int)(k&TTMASK); for(int j=0;j<8;j++){int x=(i+j)&TTMASK;
  if(tt_key[x]==k){ if(tt_rec[x].gen==g_gen)return x; if(tt_rec[x].gen==PROOF_GEN){++g_proof_hits;return x;} } } return -1;}
static void tt_store_keep(uint64_t k,int pn,int dn){int i=(int)(k&TTMASK); int slot=-1,fr=-1;
  for(int j=0;j<8;j++){int x=(i+j)&TTMASK; if(tt_rec[x].gen==g_gen||tt_rec[x].gen==PROOF_GEN){ if(tt_key[x]==k){slot=x;break;} } else if(fr<0)fr=x; }
  if(slot<0)slot=(fr>=0?fr:i);
  tt_rec[slot].gen=(pn==0&&dn>=INF)?PROOF_GEN:g_gen;tt_key[slot]=k;tt_rec[slot].pn=pn;tt_rec[slot].dn=dn;}
static int tt_find(uint64_t k){ if(g_keep_proofs) return tt_find_keep(k^tt_salt());
  int i=(int)(k&TTMASK); for(int j=0;j<8;j++){int x=(i+j)&TTMASK;
  if(tt_key[x]==k && tt_rec[x].gen==g_gen)return x;} return -1;}
static void tt_read(uint64_t k,int*pn,int*dn){int x=tt_find(k); if(x<0){*pn=1;*dn=1;}else{*pn=tt_rec[x].pn;*dn=tt_rec[x].dn;}}
static void tt_store(uint64_t k,int pn,int dn){ if(g_keep_proofs){ tt_store_keep(k^tt_salt(),pn,dn); return; }
  int i=(int)(k&TTMASK); int slot=-1,fr=-1;
  for(int j=0;j<8;j++){int x=(i+j)&TTMASK; if(tt_rec[x].gen==g_gen){ if(tt_key[x]==k){slot=x;break;} } else if(fr<0)fr=x; }
  if(slot<0)slot=(fr>=0?fr:i);
  tt_rec[slot].gen=g_gen;tt_key[slot]=k;tt_rec[slot].pn=pn;tt_rec[slot].dn=dn;}

static unsigned g_order_variant=0; 

#define STPROBE 16


static int use_superior=1;
static void sup_alloc(SupTab*t,const char*what){ ZCALLOC(t->bk,STSIZE,sizeof(uint64_t),what); ZCALLOC(t->gen,STSIZE,sizeof(uint32_t),what);
  ZCALLOC(t->h,STSIZE,sizeof(SHand),what); }
static void sup_clear(SupTab*t){ memset(t->bk,0,STSIZE*sizeof(uint64_t)); memset(t->gen,0,STSIZE*sizeof(uint32_t)); memset(t->h,0,STSIZE*sizeof(SHand)); }
static void sup_init(){ hash_autosize(); if(!proof_tab){ sup_alloc(&proof_tab_s,"proof_tab"); sup_alloc(&nomate_tab_s,"nomate_tab");
  proof_tab=&proof_tab_s; nomate_tab=&nomate_tab_s; } }
static inline int sup_idx(uint64_t bk){ return (int)((bk*0x9E3779B97F4A7C15ULL)>>(64-STBITS)); }
static inline uint64_t hand_pack(Pos*p,int col){ uint64_t h=0; for(int t=1;t<=7;t++) h|=(uint64_t)(uint8_t)p->hand[col][t]<<(8*(t-1)); return h; }

static inline int subseteq(uint64_t a,uint64_t b){ const uint64_t H=0x8080808080808080ULL; return (((b|H)-a)&H)==H; }

static inline void sup_masks(const SupTab*t,int i0,uint64_t bk,unsigned*live,unsigned*hit){
  unsigned g=0,k=0; for(int j=0;j<STPROBE;j++){ int x=(i0+j)&STMASK; if(t->gen[x]==g_gen){ g|=1u<<j; if(t->bk[x]==bk) k|=1u<<j; } }
  *live=g; *hit=k;
}

static int sup_mate(uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(proof_tab,i0,bk,&live,&hit);
  for(;hit;hit&=hit-1){ const SHand*e=&proof_tab->h[(i0+__builtin_ctz(hit))&STMASK]; if(subseteq(e->ah,A)&&subseteq(D,e->dh))return 1; } return 0; }

static int sup_nomate(uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(nomate_tab,i0,bk,&live,&hit);
  for(;hit;hit&=hit-1){ const SHand*e=&nomate_tab->h[(i0+__builtin_ctz(hit))&STMASK]; if(subseteq(A,e->ah)&&subseteq(e->dh,D))return 1; } return 0; }

static void sup_rec(SupTab*tab,uint64_t bk,uint64_t A,uint64_t D){ int i0=sup_idx(bk); unsigned live,hit; sup_masks(tab,i0,bk,&live,&hit);
  if(hit){ SHand*e=&tab->h[(i0+__builtin_ctz(hit))&STMASK]; e->ah=A; e->dh=D; return; }
  unsigned fr=~live&((1u<<STPROBE)-1); int s=fr?(i0+__builtin_ctz(fr))&STMASK:(i0&STMASK);
  tab->gen[s]=g_gen; tab->bk[s]=bk; tab->h[s].ah=A; tab->h[s].dh=D; }

static __thread long g_nodes; static __thread jmp_buf g_jmp;   

static __thread int g_stop_reason=0;

static __thread unsigned g_poll_mask=8191;
static std::atomic<int> g_stop{0}; static int g_parallel=0;
static volatile int g_usi_mode=0; static double g_usi_last_info=0;
static __thread uint64_t g_path[4096]; static __thread int g_pathlen;
static int on_path(uint64_t k){for(int i=0;i<g_pathlen;i++)if(g_path[i]==k)return 1;return 0;}





static void twin_init(){ hash_autosize(); if(!tw_key){ ZMALLOC(tw_key,sizeof(uint64_t)*TWSIZE,"tw_key"); ZMALLOC(tw_cause,sizeof(uint64_t)*TWK*TWSIZE,"tw_cause");
    ZMALLOC(tw_n,TWSIZE,"tw_n"); ZMALLOC(tw_used,TWSIZE,"tw_used");} memset(tw_used,0,TWSIZE); tw_live=0; }

static int twin_find(uint64_t k){ if(!tw_used || tw_live==0) return -1;
  int i=(int)(k&TWMASK); for(int j=0;j<8;j++){int x=(i+j)&TWMASK;
  if(tw_used[x]==0)return -1; if(tw_used[x]==1&&tw_key[x]==k)return x;} return -1; }
static void twin_remove(uint64_t k){ int x=twin_find(k); if(x>=0){ tw_used[x]=2; --tw_live; } }
static void twin_put(uint64_t k,const uint64_t*causes,int n){ if(n>TWK)n=TWK;
  int i=(int)(k&TWMASK),slot=-1; for(int j=0;j<8;j++){int x=(i+j)&TWMASK;
    if(tw_used[x]==1&&tw_key[x]==k){slot=x;break;} if(tw_used[x]!=1&&slot<0)slot=x; if(tw_used[x]==0)break;}
  if(slot<0)slot=i&TWMASK;
  
  if(tw_used[slot]==1 && tw_key[slot]!=k) tt_store(tw_key[slot],1,1);
  if(tw_used[slot]!=1) ++tw_live; tw_used[slot]=1; tw_key[slot]=k; tw_n[slot]=n;
  for(int t=0;t<n;t++)tw_cause[slot][t]=causes[t]; }
static int twin_cause_on_path(int x){ for(int t=0;t<tw_n[x];t++) if(on_path(tw_cause[x][t]))return 1; return 0; }
static void tt_read_ghi(uint64_t k,int*pn,int*dn){ int x=tt_find(k); if(x<0){*pn=1;*dn=1;return;}
  int tw=twin_find(k); if(tw>=0){ if(twin_cause_on_path(tw)){*pn=INF;*dn=0;}else{*pn=1;*dn=1;} return; }
  *pn=tt_rec[x].pn; *dn=tt_rec[x].dn; }
static int finalize_ghi(uint64_t key,int pn,int dn,const uint64_t*rt,int rtn,uint64_t*out){
  if(pn==0){ tt_store(key,0,INF); twin_remove(key); return 0; }
  if(dn==0){ int no=0;
    for(int i=0;i<rtn;i++){ uint64_t k=rt[i]; if(k!=key && on_path(k)){ int dup=0;
      for(int j=0;j<no;j++)if(out[j]==k){dup=1;break;} if(!dup&&no<TWK)out[no++]=k; } }
    tt_store(key,INF,0);
    if(no>0){ twin_put(key,out,no); return no; }
    twin_remove(key); return 0; }
  tt_store(key,pn,dn); twin_remove(key); return 0; }

static double now(){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec*1e-9; }

static const int RD8[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{-1,1},{1,-1},{1,1}};
static __thread int g_pin[81];

static int g_verify=0;
static int isPinner(int Qpc,int d){ int t=ptype(Qpc);
  if(d<4){ if(t==ROOK||t==DRAGON)return 1;
    if(t==LANCE){ int fr=(pcolor(Qpc)==BLACK)?-1:1; return RD8[d][1]==0 && fr==-RD8[d][0]; }
    return 0; }
  return t==BISHOP||t==HORSE; }
static void compute_pins(Pos*p,int me){
  for(int i=0;i<81;i++)g_pin[i]=0;
  int K=p->ksq[me]; if(K<0)return; int*b=p->b; int en=me^1; const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[K][d]; if(!m)continue; int cand=bb_first(m,d);
    if(pcolor(b[cand])!=me)continue; m&=~SQBB[cand]; if(!m)continue; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==en && isPinner(pc,d))g_pin[cand]=d+1; } }
static inline int colinearK(Pos*p,int K,int to,int pinidx){
  (void)p; int d=pinidx-1; int tr=RNK(to)-RNK(K), tc=COL(to)-COL(K);
  return tr*RD8[d][1]==tc*RD8[d][0]; }
static __thread int g_disc[81];
static void compute_disc(Pos*p,int att){      
  for(int i=0;i<81;i++)g_disc[i]=0;
  int dk=p->ksq[att^1]; if(dk<0)return; int*b=p->b; const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[dk][d]; if(!m)continue; int cand=bb_first(m,d);
    if(pcolor(b[cand])!=att)continue; m&=~SQBB[cand]; if(!m)continue; int pc=b[bb_first(m,d)];
    if(pcolor(pc)==att && isPinner(pc,d))g_disc[cand]=d+1; }
}
static int attacks_sq(Pos*p,int to,int T,int att,int dk,int from){ 
  
  if(MVSTBB[att][T][to]&SQBB[dk]) return 1;
  int sdn=MV_SDN[att][T]; if(!sdn) return 0;
  ZBB occ=p->occc[0]|p->occc[1]; if(from<81) occ&=~SQBB[from];
  for(int di=0;di<sdn;di++){ int dir=MV_SDIR[att][T][di]; ZBB m=occ&RAYBB[to][dir]; if(m && bb_first(m,dir)==dk) return 1; }
  return 0;
}
static int gives_check_fast(Pos*p,int m,int att,int dk){
  int to=M_TO(m), from=M_FROM(m), T;
  if(from==DROP_FROM)T=M_DROP(m); else {int mt=ptype(p->b[from]); T=M_PROMO(m)?PROMO[mt]:mt;}
  if(attacks_sq(p,to,T,att,dk,from)) return 1;                 
  if(from!=DROP_FROM && g_disc[from] && !colinearK(p,dk,to,g_disc[from])) return 1; 
  return 0;
}

typedef struct { int from,to,moved,cap,kfrom; } TB;
static inline TB tb_apply(Pos*p,int m){
  int*b=p->b; int col=p->side; int to=M_TO(m); TB t; t.to=to; t.kfrom=-1;
  if(M_FROM(m)==DROP_FROM){ t.from=-1; t.moved=0; t.cap=0; b[to]=mkpc(M_DROP(m),col); p->occc[col]|=SQBB[to];
    p->slo[col]|=SQBB[to]&SLOM[b[to]]; p->sld[col]|=SQBB[to]&SLDM[b[to]]; return t; }
  int from=M_FROM(m); int moved=b[from]; int cap=b[to];
  t.from=from; t.moved=moved; t.cap=cap; b[from]=EMPTY; p->occc[col]=(p->occc[col]&~SQBB[from])|SQBB[to]; if(cap)p->occc[col^1]&=~SQBB[to];
  int mt=ptype(moved); b[to]=mkpc(M_PROMO(m)?PROMO[mt]:mt,col);
  p->slo[col]=(p->slo[col]&~SQBB[from])|(SQBB[to]&SLOM[b[to]]); p->sld[col]=(p->sld[col]&~SQBB[from])|(SQBB[to]&SLDM[b[to]]);
  p->slo[col^1]&=~SQBB[to]; p->sld[col^1]&=~SQBB[to];
  if(mt==KING){ t.kfrom=from; p->ksq[col]=to; }
  return t;
}
static inline void tb_revert(Pos*p,TB t){
  int*b=p->b; if(t.from==-1){ b[t.to]=EMPTY; p->occc[p->side]&=~SQBB[t.to]; p->slo[p->side]&=~SQBB[t.to]; p->sld[p->side]&=~SQBB[t.to]; return; }
  b[t.from]=t.moved; b[t.to]=t.cap; if(t.kfrom>=0)p->ksq[p->side]=t.kfrom;
  p->occc[p->side]=(p->occc[p->side]&~SQBB[t.to])|SQBB[t.from]; if(t.cap)p->occc[p->side^1]|=SQBB[t.to];
  { int c=p->side; p->slo[c]=(p->slo[c]&~SQBB[t.to])|(SQBB[t.from]&SLOM[t.moved]); p->sld[c]=(p->sld[c]&~SQBB[t.to])|(SQBB[t.from]&SLDM[t.moved]);
    p->slo[c^1]|=SQBB[t.to]&SLOM[t.cap]; p->sld[c^1]|=SQBB[t.to]&SLDM[t.cap]; }
}

static int gen_checks(Pos*p,int*out){
  int att=p->side, def=att^1; int K=p->ksq[att];
  int incheck=(K>=0 && is_attacked(p,K,def));
  compute_pins(p,att); compute_disc(p,att); int dk=p->ksq[def];
  int*b=p->b; int m=0;
  if(incheck || dk<0){                       
    int pm[MAXMOVES]; int n=gen_pseudo(p,att,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv); TB t=tb_apply(p,mv); int legal=own_safe(p,att); tb_revert(p,t);
      if(!legal)continue; if(!(dk>=0&&gives_check_fast(p,mv,att,dk)))continue;
      
      if(from==DROP_FROM&&M_DROP(mv)==PAWN){ make_move(p,mv); int mated; IS_MATED_KEEPPINS(p,mated); unmake_move(p); if(mated)continue; }
      out[m++]=mv; }
  } else {
    ZBB zone=STEPBB[att][dk]; for(int d=0;d<8;d++) zone|=RAYBB[dk][d];   
    int bm[MAXMOVES]; int bn=gen_board_z(p,att,bm,zone,g_disc);                 
    for(int i=0;i<bn;i++){ int mv=bm[i],from=M_FROM(mv),to=M_TO(mv);
      if(!(zone&SQBB[to]) && !g_disc[from]) continue;              
      int mt=ptype(b[from]); int legal;
      if(mt==KING){ legal=king_move_safe(p,from,to,att); }
      else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]);
      if(!legal)continue; if(!gives_check_fast(p,mv,att,dk))continue; out[m++]=mv; }
    
    const StepAtk*sa=STEP_ATK[att][dk]; int sn=STEP_ATK_N[att][dk];
    for(int i=0;i<sn;i++){ int q=sa[i].q; if(b[q])continue; uint64_t mask=sa[i].mask;
      if((mask&(1ULL<<mkpc(PAWN,att)))&&p->hand[att][PAWN]&&droppable_at(p,att,PAWN,q)){
        
        int mv=MV(DROP_FROM,q,0,PAWN); make_move(p,mv); int mated; IS_MATED_KEEPPINS(p,mated); unmake_move(p);
        if(!mated) out[m++]=mv; }
      if((mask&(1ULL<<mkpc(KNIGHT,att)))&&p->hand[att][KNIGHT]&&droppable_at(p,att,KNIGHT,q)) out[m++]=MV(DROP_FROM,q,0,KNIGHT);
      if((mask&(1ULL<<mkpc(SILVER,att)))&&p->hand[att][SILVER]) out[m++]=MV(DROP_FROM,q,0,SILVER);
      if((mask&(1ULL<<mkpc(GOLD,att)))&&p->hand[att][GOLD]) out[m++]=MV(DROP_FROM,q,0,GOLD);
    }
    if(p->hand[att][LANCE]){ int d=LANCE_INDIR[att]; const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; if(droppable_at(p,att,LANCE,q)) out[m++]=MV(DROP_FROM,q,0,LANCE); } }
    if(p->hand[att][ROOK]){ for(int d=0;d<4;d++){ const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; out[m++]=MV(DROP_FROM,q,0,ROOK); } } }
    if(p->hand[att][BISHOP]){ for(int d=4;d<8;d++){ const int*ray=RAY[dk][d]; int len=RAYN[dk][d];
      for(int k=0;k<len;k++){ int q=ray[k]; if(b[q])break; out[m++]=MV(DROP_FROM,q,0,BISHOP); } } }
  }
  if(g_verify){ int pm[MAXMOVES]; int n=gen_pseudo(p,att,pm); int ref[MAXMOVES],rn=0;
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv); TB t=tb_apply(p,mv); int legal=own_safe(p,att); tb_revert(p,t);
      if(!legal)continue; if(!(dk>=0&&gives_check_fast(p,mv,att,dk)))continue;
      if(from==DROP_FROM&&M_DROP(mv)==PAWN){ make_move(p,mv); int any=0;int dm[MAXMOVES];int dn=gen_pseudo(p,def,dm);
        for(int j=0;j<dn;j++){TB t2=tb_apply(p,dm[j]);if(own_safe(p,def))any=1;tb_revert(p,t2);if(any)break;} unmake_move(p); if(!any)continue; }
      ref[rn++]=mv; }
    if(rn!=m){fprintf(stderr,"CHECK COUNT MISMATCH new=%d old=%d incheck=%d\n",m,rn,incheck);abort();}
    for(int i=0;i<rn;i++){int f=0;for(int j=0;j<m;j++)if(out[j]==ref[i]){f=1;break;}if(!f){fprintf(stderr,"CHECK SET MISMATCH mv=%d\n",ref[i]);abort();}} }
  if(g_order_variant && m>1){
    const unsigned offset=unsigned((p->key ^ (uint64_t(g_order_variant)*0x9e3779b97f4a7c15ULL))%m);
    std::rotate(out,out+offset,out+m);
  }
  assert(m<=MAXMOVES); return m;
}

static int find_checkers(Pos*p,int side,int K,int*chk){
  int by=side^1,nc=0;
  const StepAtk*sa=STEP_ATK[by][K]; int n=STEP_ATK_N[by][K];
  (void)sa; (void)n;
  for(ZBB c=p->occc[by]&STEPBB[by][K]; c; c&=c-1){ int q=bb_lsb(c); if((1ULL<<p->b[q])&STEP_REL[by][q-K+19]){ if(nc<2)chk[nc]=q; nc++; } }
  const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ if(!((d<4?p->slo[by]:p->sld[by])&RAYBB[K][d]))continue; ZBB m=occ&RAYBB[K][d]; int sq=bb_first(m,d); int pc=p->b[sq];
    if(pcolor(pc)==by){ int t=ptype(pc),hit=0;
      if(d<4){ if(t==ROOK||t==DRAGON)hit=1; else if(t==LANCE&&d==LANCE_INDIR[by])hit=1; }
      else { if(t==BISHOP||t==HORSE)hit=1; }
      if(hit){ if(nc<2)chk[nc]=sq; nc++; } } }
  return nc;
}

static void build_targets(int K,int C,char*tgt,int*btw,int*nb){
  tgt[C]=1; *nb=0; int kr=K/9,kc=K%9,cr=C/9,cc=C%9; int dr=cr-kr,dc=cc-kc;
  if(dr==0||dc==0||dr==dc||dr==-dc){ int sr=(dr>0)-(dr<0),sc=(dc>0)-(dc<0);
    int r=kr+sr,c=kc+sc; while(r!=cr||c!=cc){ int s=r*9+c; tgt[s]=1; btw[(*nb)++]=s; r+=sr; c+=sc; } }
}


static void gen_moves_to(Pos*p,int side,int T,int*out,int*pm,int K){
  int tp=p->b[T]; if(tp && (pcolor(tp)==side||ptype(tp)==KING)) return;
  for(ZBB c=p->occc[side]&STEPBB[side][T]; c; c&=c-1){ int s=bb_lsb(c); int pc=p->b[s];
    int t=ptype(pc); if(t==KING)continue; if(!(STEP_REL[side][s-T+19]&(1ULL<<pc)))continue;
    if(g_pin[s] && !colinearK(p,K,T,g_pin[s]))continue; emit(s,T,t,side,out,pm); }
  const ZBB occ=p->occc[0]|p->occc[1];
  for(int d=0;d<8;d++){ ZBB m=occ&RAYBB[T][d]; if(!m)continue; int s=bb_first(m,d); int pc=p->b[s];
    if(pcolor(pc)==side){ int t=ptype(pc),hit=0;
      if(d<4){ if(t==ROOK||t==DRAGON)hit=1; else if(t==LANCE&&d==LANCE_INDIR[side])hit=1; }
      else { if(t==BISHOP||t==HORSE)hit=1; }
      if(hit && !(g_pin[s] && !colinearK(p,K,T,g_pin[s]))) emit(s,T,t,side,out,pm); } }
}
static int gen_legal(Pos*p,int*out){
  int side=p->side, K=p->ksq[side]; int m=0;
  
  int chk[2]; int nc=K>=0?find_checkers(p,side,K,chk):0; int incheck=(nc>0);
  if(!incheck){
    compute_pins(p,side);
    int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv),legal;
      if(from==DROP_FROM) legal=1;
      else { int mt=ptype(p->b[from]);
        if(mt==KING){ legal=king_move_safe(p,from,M_TO(mv),side); }
        else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]); }
      if(legal) out[m++]=mv; }
  } else {
    if(nc==0){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
      for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); if(own_safe(p,side))out[m++]=pm[i]; tb_revert(p,t);} }
    else { char tgt[81]; memset(tgt,0,81); int btw[16],nb=0; if(nc==1) build_targets(K,chk[0],tgt,btw,&nb);
      compute_pins(p,side);
      { int*st=MV_ST[side][KING][K]; int sn=MV_STN[side][KING][K];          
        for(int k=0;k<sn;k++){ int to=st[k]; int tp=p->b[to]; if(tp&&(pcolor(tp)==side||ptype(tp)==KING))continue;
          int mv=MV(K,to,0,0); if(king_move_safe(p,K,to,side))out[m++]=mv; } }
      if(nc==1){ gen_moves_to(p,side,chk[0],out,&m,K);                       
        for(int j=0;j<nb;j++) gen_moves_to(p,side,btw[j],out,&m,K);          
        static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};  
        for(int j=0;j<nb;j++){ int s=btw[j];
          for(int hi=0;hi<7;hi++){ int t=HT[hi]; if(p->hand[side][t]==0)continue;
            if(droppable_at(p,side,t,s)) out[m++]=MV(DROP_FROM,s,0,t); } } }
    }
  }
  if(g_verify){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm); int ref[MAXMOVES],rn=0;
    for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); if(own_safe(p,side))ref[rn++]=pm[i]; tb_revert(p,t); }
    if(rn!=m){ fprintf(stderr,"LEGAL COUNT MISMATCH new=%d old=%d incheck=%d\n",m,rn,incheck); abort(); }
    for(int i=0;i<rn;i++){ int f=0; for(int j=0;j<m;j++)if(out[j]==ref[i]){f=1;break;} if(!f){ fprintf(stderr,"LEGAL SET MISMATCH mv=%d\n",ref[i]); abort(); } } }
  assert(m<=MAXMOVES); return m;
}
static int has_legal(Pos*p,int side){int pm[MAXMOVES];int n=gen_pseudo(p,side,pm);
  for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); int ok=own_safe(p,side); tb_revert(p,t); if(ok)return 1;} return 0;}


static int is_mated(Pos*p){
  int side=p->side, K=p->ksq[side];
  
  int chk[2]; int nc=K>=0?find_checkers(p,side,K,chk):0; int incheck=(nc>0);
  if(!incheck){
    compute_pins(p,side);
    int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);
    for(int i=0;i<n;i++){ int mv=pm[i],from=M_FROM(mv),legal;
      if(from==DROP_FROM) legal=1;
      else { int mt=ptype(p->b[from]);
        if(mt==KING){ legal=king_move_safe(p,from,M_TO(mv),side); }
        else legal=!g_pin[from]||colinearK(p,K,M_TO(mv),g_pin[from]); }
      if(legal) return 0; }
    return 1;
  }
  if(nc==0){ int pm[MAXMOVES]; int n=gen_pseudo(p,side,pm);          
    for(int i=0;i<n;i++){ TB t=tb_apply(p,pm[i]); int ok=own_safe(p,side); tb_revert(p,t); if(ok)return 0; }
    return 1; }
  { int*st=MV_ST[side][KING][K]; int sn=MV_STN[side][KING][K];        
    for(int k=0;k<sn;k++){ int to=st[k]; int tp=p->b[to]; if(tp&&(pcolor(tp)==side||ptype(tp)==KING))continue;
      if(king_move_safe(p,K,to,side))return 0; } }
  if(nc>=2) return 1;                                                 
  { char tgt[81]; memset(tgt,0,81); int btw[16],nb=0; build_targets(K,chk[0],tgt,btw,&nb);
    compute_pins(p,side);
    int tmp[MAXMOVES]; int m;
    m=0; gen_moves_to(p,side,chk[0],tmp,&m,K); if(m) return 0;        
    for(int j=0;j<nb;j++){ m=0; gen_moves_to(p,side,btw[j],tmp,&m,K); if(m) return 0; }  
    static const int HT[7]={PAWN,LANCE,KNIGHT,SILVER,GOLD,BISHOP,ROOK};
    for(int j=0;j<nb;j++){ int s=btw[j];                              
      for(int hi=0;hi<7;hi++){ int t=HT[hi]; if(p->hand[side][t]==0)continue;
        if(droppable_at(p,side,t,s)) return 0; } }
  }
  return 1;
}
static int use_plus=1;

static int g_order=0;
static int g_sort=0;    

#define ZY_SEED_MAX 64
static const int ZY_PVAL[15]={0,1,2,2,3,4,5,6,0,3,3,3,3,7,8};  
static void estimate_mv(Pos*p,int mv,int*epn,int*edn){
  int def=g_attacker^1; int ks=p->ksq[def]; if(ks<0){*epn=1;*edn=1;return;}
  int r=RNK(ks),c=COL(ks),libs=0;
  for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ if(!dr&&!dc)continue; int rr=r+dr,cc=c+dc;
    if(!inb(rr,cc))continue; int t=SQ(rr,cc); int pc=p->b[t];
    if(pc==0||pcolor(pc)==g_attacker){ if(!is_attacked(p,t,g_attacker))libs++; } }
  int e=libs+1;                                   
  if(g_order && mv>=0){
    
    int s=libs*6;                                 
    int to=M_TO(mv);
    
    if(!is_attacked(p,to,g_attacker)) s+=3;
    
    { int cap=ustack[usp-1].captured;
      if(cap) { int v=ZY_PVAL[ptype(cap)]; s-= (v>2?2:v); } }
    
    { int dr2=RNK(to)-RNK(ks), dc2=COL(to)-COL(ks);
      if(dr2<0)dr2=-dr2; if(dc2<0)dc2=-dc2; int d=dr2>dc2?dr2:dc2;
      s += (d>3?3:d); }
    if(s<0)s=0;
    e=s+1;                                        
    if(e>ZY_SEED_MAX)e=ZY_SEED_MAX;               
  }
  if(p->side==g_attacker){*epn=1;*edn=e;} else {*epn=e;*edn=1;}
}

static int move_quality(Pos*p,int mv){
  int def=g_attacker^1; int ks=p->ksq[def];
  if(ks<0) return 0;
  int r=RNK(ks),c=COL(ks),libs=0;
  for(int dr=-1;dr<=1;dr++)for(int dc=-1;dc<=1;dc++){ if(!dr&&!dc)continue; int rr=r+dr,cc=c+dc;
    if(!inb(rr,cc))continue; int t=SQ(rr,cc); int pc=p->b[t];
    if(pc==0||pcolor(pc)==g_attacker){ if(!is_attacked(p,t,g_attacker))libs++; } }
  int s=libs*6;
  int to=M_TO(mv);
  if(!is_attacked(p,to,g_attacker)) s+=3;
  { int cap=ustack[usp-1].captured;
    if(cap){ int v=ZY_PVAL[ptype(cap)]; s-=(v>2?2:v); } }
  { int dr2=RNK(to)-RNK(ks),dc2=COL(to)-COL(ks);
    if(dr2<0)dr2=-dr2; if(dc2<0)dc2=-dc2; int d=dr2>dc2?dr2:dc2;
    s+=(d>3?3:d); }
  return s;
}



#ifndef MFD
#define MFD 3
#endif
static int g_mfd=MFD; static int g_nodelay=0; static int g_snda=1;   
static int mate_and(Pos*p,int depth);

static int mate_search_with(Pos*p,int depth,const int*cm,int n){   
  for(int i=0;i<n;i++){ make_move(p,cm[i]); int r;
    if(on_path(p->key)) r=0;                   
    else { if(is_mated(p)) r=1;                 
      else if(depth<=1) r=0;
      else { g_path[g_pathlen++]=p->key; r=mate_and(p,depth-1); g_pathlen--; } }
    
    if(r){ tt_store(p->key,0,INF); twin_remove(p->key); }   
    unmake_move(p); if(r) return 1; }
  return 0; }

static int mate_search(Pos*p,int depth){
  int cm[MAXMOVES]; int n=gen_checks(p,cm); return mate_search_with(p,depth,cm,n); }
static int mate_and(Pos*p,int depth){          
  int dm[MAXMOVES]; int nd=gen_legal(p,dm);
  
  if(nd==0){ tt_store(p->key,0,INF); twin_remove(p->key); return 1; }  
  if(depth<=0) return 0;
  for(int j=0;j<nd;j++){ make_move(p,dm[j]); int r;
    if(on_path(p->key)) r=0;                    
    else { g_path[g_pathlen++]=p->key; r=mate_search(p,depth-1); g_pathlen--; }
    
    if(r){ tt_store(p->key,0,INF); twin_remove(p->key); }   
    unmake_move(p); if(!r) return 0; }          
  return 1; }


#define M3BITS 22
#define M3SIZE (1<<M3BITS)
#define M3MASK (M3SIZE-1)

static void m3_init(){ if(!m3_key){ ZMALLOC(m3_key,sizeof(uint64_t)*M3SIZE,"m3_key"); ZCALLOC(m3_gen,M3SIZE,sizeof(uint32_t),"m3_gen");} }
static int m3_seen(uint64_t k){ int i=k&M3MASK; for(int j=0;j<6;j++){int x=(i+j)&M3MASK; if(m3_gen[x]==g_gen&&m3_key[x]==k)return 1;} return 0; }
static void m3_add(uint64_t k){ int i=k&M3MASK,slot=-1,fr=-1; for(int j=0;j<6;j++){int x=(i+j)&M3MASK; if(m3_gen[x]==g_gen){if(m3_key[x]==k){slot=x;break;}} else if(fr<0)fr=x;} if(slot<0)slot=(fr>=0?fr:i&M3MASK); m3_gen[slot]=g_gen;m3_key[slot]=k; }

#define ZYF_MAX_DEPTH 3900

static __thread std::vector<std::pair<uint64_t,uint16_t>>* g_yexport=nullptr; static __thread size_t g_yexport_cap=0;

static __thread void (*g_ylive)(uint64_t,uint16_t)=nullptr;
static void move_usi(int m,char*buf);
static inline uint16_t yo_move16(int m){ char buf[8]={0}; move_usi(m,buf); return YaneuraOu::USIEngine::to_move16(buf).to_u16(); }
static int mid(Pos*p,int thpn,int thdn,uint64_t*rtout){
  
  if((++g_nodes & g_poll_mask)==0){ if(g_stop||g_nodes>g_limit||now()-g_t0>g_tlimit){ g_stop_reason=g_stop?3:(g_nodes>g_limit?1:2); longjmp(g_jmp,1); }
    if(g_usi_mode){ double t=now()-g_t0; if(t-g_usi_last_info>=1.0){
      g_usi_last_info=t; long nps=t>0?(long)(g_nodes/t):0;
      printf("info nodes %ld nps %ld\n",g_nodes,nps); fflush(stdout); } } }
  if(g_pathlen>=ZYF_MAX_DEPTH){ g_stop_reason=4; longjmp(g_jmp,1); }
  uint64_t key=p->key;
  uint64_t A=0,D=0; if(use_superior){ A=hand_pack(p,g_attacker); D=hand_pack(p,g_attacker^1);
    if(sup_mate(p->bkey,A,D)){ tt_store(key,0,INF); twin_remove(key); return 0; }
    if(sup_nomate(p->bkey,A,D)){ tt_store(key,INF,0); twin_remove(key); return 0; } }
  int is_or=(p->side==g_attacker);
  int moves[MAXMOVES]; int nm;
  if(is_or){ nm=gen_checks(p,moves); if(nm==0){tt_store(key,INF,0); twin_remove(key); if(use_superior)sup_rec(nomate_tab,p->bkey,A,D); return 0;}
    
    if(!m3_seen(key)){ m3_add(key); if(mate_search_with(p,g_mfd,moves,nm)){ tt_store(key,0,INF); twin_remove(key); if(use_superior)sup_rec(proof_tab,p->bkey,A,D); return 0; } } }
  else { nm=gen_legal(p,moves); if(nm==0){tt_store(key,0,INF); twin_remove(key); if(use_superior)sup_rec(proof_tab,p->bkey,A,D); return 0;} }

  uint64_t ck[MAXMOVES]; uint8_t repf[MAXMOVES];
  
  for(int i=0;i<nm;i++){ ck[i]=child_key(p,moves[i]); tt_prefetch(ck[i]); }
  for(int i=0;i<nm;i++){ make_move(p,moves[i]); assert(ck[i]==p->key); ck[i]=p->key;
    if(use_plus && tt_find(p->key)<0 && twin_find(p->key)<0){int ep,ed;estimate_mv(p,moves[i],&ep,&ed);tt_store(p->key,ep,ed);}
    repf[i]=0; unmake_move(p); }

  uint8_t dly[MAXMOVES]; int gnx[MAXMOVES];                 
  for(int i=0;i<nm;i++){ dly[i]=0; gnx[i]=-1; }
  if(!is_or && !g_nodelay){                          
    int head=-1,tail=-1;
    for(int i=0;i<nm;i++){
      if(IS_DROP(moves[i])){
        if(head>=0 && M_TO(moves[i])==M_TO(moves[head])){ gnx[tail]=i; dly[i]=1; tail=i; }
        else { head=i; tail=i; }
      } else head=-1; } }
  uint64_t rep_targets[64]; int rtn=0;   

  for(;;){
    int pn,dn,best=-1,best1=INF,best2=INF;
    
    if(is_or){ pn=INF; int64_t dnsum=0; int dnmax=0; int best_d=99;
      int kr=-1,kc=-1; if(g_sort){int ks=p->ksq[g_attacker^1]; if(ks>=0){kr=RNK(ks);kc=COL(ks);}}
      for(int i=0;i<nm;i++){int cpn,cdn; if(repf[i]){cpn=INF;cdn=0;}else tt_read_ghi(ck[i],&cpn,&cdn);
        if(cpn<pn)pn=cpn;
        if(g_snda && cdn>(INF/16)){ if(cdn>dnmax)dnmax=cdn; } else dnsum+=cdn;
        if(cpn<best1){best2=best1;best1=cpn;best=i;
          if(kr>=0){int to=M_TO(moves[i]),dr=RNK(to)-kr,dc=COL(to)-kc;if(dr<0)dr=-dr;if(dc<0)dc=-dc;best_d=dr>dc?dr:dc;}}
        else if(cpn==best1&&kr>=0){int to=M_TO(moves[i]),dr=RNK(to)-kr,dc=COL(to)-kc;if(dr<0)dr=-dr;if(dc<0)dc=-dc;int d=dr>dc?dr:dc;
          if(d<best_d){best=i;best_d=d;} if(cpn<best2)best2=cpn;}
        else if(cpn<best2)best2=cpn; }
      { int64_t dd=dnsum+dnmax; dn=dd>INF?INF:(int)dd; }
    } else { int act=1;
      int64_t pnsum=0; int pnmax=0;
      while(act){ act=0; pnsum=0;pnmax=0;dn=INF;best=-1;best1=INF;best2=INF;
        for(int i=0;i<nm;i++){ if(dly[i])continue; int cpn,cdn; if(repf[i]){cpn=INF;cdn=0;}else tt_read_ghi(ck[i],&cpn,&cdn);
          if(g_snda && cpn>(INF/16)){ if(cpn>pnmax)pnmax=cpn; } else pnsum+=cpn;
          if(cdn<dn)dn=cdn;
          if(cdn<best1){best2=best1;best1=cdn;best=i;} else if(cdn<best2)best2=cdn;
          if(gnx[i]>=0 && cpn==0 && dly[gnx[i]]){ dly[gnx[i]]=0; act=1; } } }
      { int64_t pp=pnsum+pnmax; pn=pp>INF?INF:(int)pp; }
      int anyd=0; for(int i=0;i<nm;i++) if(dly[i]){anyd=1;break;}
      if(anyd && pn<INF) pn+=1;                       
    }
    if(pn==0 && is_or && g_yexport && best>=0 && g_yexport->size()<g_yexport_cap){
      g_yexport->emplace_back(yo_key(p),yo_move16(moves[best]));
      if(g_ylive) g_ylive(g_yexport->back().first,g_yexport->back().second); }
    if(pn>=thpn||dn>=thdn){ uint64_t out[TWK]; int no=finalize_ghi(key,pn,dn,rep_targets,rtn,out);
      if(use_superior){ if(pn==0)sup_rec(proof_tab,p->bkey,A,D);
        else if(dn==0&&no==0)sup_rec(nomate_tab,p->bkey,A,D); }
      for(int i=0;i<no;i++)rtout[i]=out[i]; return no; }
    int ctp,ctd,cpn,cdn;
    if(repf[best]){cpn=INF;cdn=0;}else tt_read_ghi(ck[best],&cpn,&cdn);
    if(is_or){ ctp=(thpn<best2+1?thpn:best2+1); int64_t v=(int64_t)thdn-(dn-cdn); ctd=v>INF?INF:(int)v; }
    else { ctd=(thdn<best2+1?thdn:best2+1); int64_t v=(int64_t)thpn-(pn-cpn); ctp=v>INF?INF:(int)v; }
    make_move(p,moves[best]);
    if(on_path(p->key)){ repf[best]=1; uint64_t a=p->key; int dup=0;
      for(int j=0;j<rtn;j++)if(rep_targets[j]==a){dup=1;break;} if(!dup&&rtn<64)rep_targets[rtn++]=a;
      unmake_move(p); continue; }
    g_path[g_pathlen++]=p->key; uint64_t crt[TWK]; int crn=mid(p,ctp,ctd,crt); g_pathlen--; unmake_move(p);
    for(int i=0;i<crn;i++){ uint64_t a=crt[i]; int dup=0;
      for(int j=0;j<rtn;j++)if(rep_targets[j]==a){dup=1;break;} if(!dup&&rtn<64)rep_targets[rtn++]=a; }
  }
}


static pthread_mutex_t g_mtx=PTHREAD_MUTEX_INITIALIZER;
static void ttp_read(uint64_t k,int*pn,int*dn){ pthread_mutex_lock(&g_mtx); tt_read(k,pn,dn); pthread_mutex_unlock(&g_mtx); }
static void ttp_store(uint64_t k,int pn,int dn){ pthread_mutex_lock(&g_mtx); tt_store(k,pn,dn); pthread_mutex_unlock(&g_mtx); }
static int  ttp_has(uint64_t k){ pthread_mutex_lock(&g_mtx); int r=tt_find(k)>=0; pthread_mutex_unlock(&g_mtx); return r; }

#define WKSIZE (1<<21)
#define WKMASK (WKSIZE-1)
static uint64_t *wk_key=NULL; static int *wk_cnt; static uint8_t *wk_used;
static void wk_init(){ if(!wk_key){ ZMALLOC(wk_key,8*WKSIZE,"wk_key"); ZMALLOC(wk_cnt,4*WKSIZE,"wk_cnt"); ZMALLOC(wk_used,WKSIZE,"wk_used");} memset(wk_used,0,WKSIZE); }
static void wk_inc(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x]){wk_used[x]=1;wk_key[x]=k;wk_cnt[x]=1;break;} if(wk_key[x]==k){wk_cnt[x]++;break;}} pthread_mutex_unlock(&g_mtx); }
static void wk_dec(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x])break; if(wk_key[x]==k){ if(wk_cnt[x]>0)wk_cnt[x]--; break;}} pthread_mutex_unlock(&g_mtx); }
static int  wk_get(uint64_t k){ pthread_mutex_lock(&g_mtx); int i=k&WKMASK; int r=0; for(int j=0;j<16;j++){int x=(i+j)&WKMASK;
  if(!wk_used[x])break; if(wk_key[x]==k){r=wk_cnt[x];break;}} pthread_mutex_unlock(&g_mtx); return r; }


static void mid_par(Pos*p,int thpn,int thdn,int*outrep){
  if((++g_nodes & std::min(g_poll_mask,4095u))==0){ if(g_stop||g_nodes>g_limit||now()-g_t0>g_tlimit){ g_stop_reason=g_stop?3:(g_nodes>g_limit?1:2); longjmp(g_jmp,1); } }
  if(g_pathlen>=ZYF_MAX_DEPTH){ g_stop_reason=4; longjmp(g_jmp,1); }   
  uint64_t key=p->key; int rep_here=0;
  int is_or=(p->side==g_attacker);
  int moves[MAXMOVES]; int nm;
  if(is_or){ nm=gen_checks(p,moves); if(nm==0){ttp_store(key,INF,0); *outrep=0; return;} }
  else { nm=gen_legal(p,moves); if(nm==0){ttp_store(key,0,INF); *outrep=0; return;} }
  wk_inc(key);
  uint64_t ck[MAXMOVES]; uint8_t repf[MAXMOVES];
  for(int i=0;i<nm;i++){ make_move(p,moves[i]); ck[i]=p->key;
    if(use_plus && !ttp_has(p->key)){int ep,ed;estimate_mv(p,moves[i],&ep,&ed);ttp_store(p->key,ep,ed);}
    repf[i]=0; unmake_move(p); }
  for(;;){
    int best=-1;
    if(is_or){
      int real_pn=INF,real_dn=0,rp1=INF,rp2=INF,argmin=-1,effbest=INF,bestdn=0;
      for(int i=0;i<nm;i++){ int cpn,cdn,w; if(repf[i]){cpn=INF;cdn=0;w=0;} else {ttp_read(ck[i],&cpn,&cdn); w=wk_get(ck[i]);}
        real_dn+=cdn; if(real_dn>INF)real_dn=INF;
        if(cpn<rp1){rp2=rp1;rp1=cpn;argmin=i;} else if(cpn<rp2)rp2=cpn;
        int eff=cpn+w; if(eff>INF)eff=INF; if(eff<effbest){effbest=eff;best=i;bestdn=cdn;} }
      real_pn=rp1;
      if(real_pn>=thpn||real_dn>=thdn){ ttp_store(key,real_pn,real_dn); wk_dec(key); *outrep=rep_here; return; }
      int om=(best==argmin)?rp2:rp1; int ctp=thpn<om+1?thpn:om+1;
      long v=(long)thdn-(real_dn-bestdn); int ctd=v>INF?INF:(int)v;
      make_move(p,moves[best]);
      if(on_path(p->key)){ repf[best]=1; rep_here=1; unmake_move(p); continue; }
      g_path[g_pathlen++]=p->key; int cr=0; mid_par(p,ctp,ctd,&cr); g_pathlen--; unmake_move(p); rep_here|=cr;
    } else {
      int real_pn=0,real_dn=INF,rd1=INF,rd2=INF,argmin=-1,effbest=INF,bestpn=0;
      for(int i=0;i<nm;i++){ int cpn,cdn,w; if(repf[i]){cpn=INF;cdn=0;w=0;} else {ttp_read(ck[i],&cpn,&cdn); w=wk_get(ck[i]);}
        real_pn+=cpn; if(real_pn>INF)real_pn=INF;
        if(cdn<rd1){rd2=rd1;rd1=cdn;argmin=i;} else if(cdn<rd2)rd2=cdn;
        int eff=cdn+w; if(eff>INF)eff=INF; if(eff<effbest){effbest=eff;best=i;bestpn=cpn;} }
      real_dn=rd1;
      if(real_pn>=thpn||real_dn>=thdn){ ttp_store(key,real_pn,real_dn); wk_dec(key); *outrep=rep_here; return; }
      int om=(best==argmin)?rd2:rd1; int ctd=thdn<om+1?thdn:om+1;
      long v=(long)thpn-(real_pn-bestpn); int ctp=v>INF?INF:(int)v;
      make_move(p,moves[best]);
      if(on_path(p->key)){ repf[best]=1; rep_here=1; unmake_move(p); continue; }
      g_path[g_pathlen++]=p->key; int cr=0; mid_par(p,ctp,ctd,&cr); g_pathlen--; unmake_move(p); rep_here|=cr;
    }
  }
}

static int solve(Pos*p,long nodelimit,double tlimit,long*nodes_out){
  g_attacker=p->side; tt_init(); twin_init(); m3_init(); if(use_superior)sup_init(); g_stop_reason=0;
  g_poll_mask = tlimit <= 0.02 ? 63u : tlimit <= 0.05 ? 255u : tlimit <= 0.25 ? 1023u : 8191u;   
  if(++g_gen==0 || g_gen==PROOF_GEN){ tt_clear_gen(); memset(m3_gen,0,sizeof(uint32_t)*M3SIZE);
    if(proof_tab){sup_clear(proof_tab);sup_clear(nomate_tab);} g_gen=1; } g_nodes=0; g_limit=nodelimit; g_t0=now(); g_tlimit=tlimit; g_pathlen=0;
  int usp_save=usp;
  if(setjmp(g_jmp)){ usp=usp_save; if(nodes_out)*nodes_out=g_nodes; return 2; } 
  g_path[g_pathlen++]=p->key;
  { uint64_t rt[16]; mid(p,INF,INF,rt); }
  g_pathlen=0;                 
  if(nodes_out)*nodes_out=g_nodes;
  int pn,dn; tt_read_ghi(p->key,&pn,&dn);
  if(pn==0)return 1; if(dn==0)return 0; return 2;
}

typedef struct { const char*sfen; long nodes; } PArg;

static void* pworker(void*arg){ PArg*a=(PArg*)arg; Pos p; parse_sfen(&p,a->sfen); g_nodes=0;
  if(setjmp(g_jmp)){ a->nodes=g_nodes; return NULL; }
  g_pathlen=0; g_path[g_pathlen++]=p.key; int rep=0;
  mid_par(&p,INF,INF,&rep);
  g_stop=1; a->nodes=g_nodes; return NULL; }
static int solve_parallel(const char*sfen,int nthreads,long nl,double tl,long*nodes_out){
  Pos p; if(!parse_sfen(&p,sfen)){ if(nodes_out)*nodes_out=0; return 2; }
  
  if(nthreads<=1 || !getenv("ZYFAMATE_ALLOW_PARALLEL")){
    if(nthreads>1) fprintf(stderr,"info string Zyfamate: parallel df-pn is a measured net loss "
        "(0.60x at 2T) and mid_par lacks mate_search/superiority/GHI; using the serial solver. "
        "Set ZYFAMATE_ALLOW_PARALLEL=1 to force the old behaviour.\n");
    return solve(&p,nl,tl,nodes_out);
  }
  g_attacker=p.side; g_limit=nl; g_t0=now(); g_tlimit=tl; g_stop=0;
  
  int save_sup=use_superior; use_superior=0; g_parallel=1; tt_init(); twin_init(); wk_init();
  if(++g_gen==0 || g_gen==PROOF_GEN){ tt_clear_gen(); g_gen=1; }
  if(nthreads<1)nthreads=1; if(nthreads>64)nthreads=64;
  pthread_t th[64]; PArg a[64];
  for(int t=0;t<nthreads;t++){ a[t].sfen=sfen; a[t].nodes=0; pthread_create(&th[t],NULL,pworker,&a[t]); }
  long tot=0; for(int t=0;t<nthreads;t++){ pthread_join(th[t],NULL); tot+=a[t].nodes; }
  g_parallel=0; use_superior=save_sup; if(nodes_out)*nodes_out=tot;
  int pn,dn; tt_read(p.key,&pn,&dn); return pn==0?1:(dn==0?0:2); }


#define LMBITS 22
#define LMSIZE (1<<LMBITS)
#define LMMASK (LMSIZE-1)




static void lm_init(){ if(!lm_key){ ZMALLOC(lm_key,sizeof(uint64_t)*LMSIZE,"lm_key"); ZMALLOC(lm_val,LMSIZE,"lm_val"); ZMALLOC(lm_used,LMSIZE,"lm_used");} memset(lm_used,0,LMSIZE); }
static int lm_get(uint64_t h,int*v){int i=h&LMMASK;for(int j=0;j<6;j++){int x=(i+j)&LMMASK;
  if(!lm_used[x])return 0; if(lm_key[x]==h){*v=lm_val[x];return 1;}}return 0;}
static void lm_put(uint64_t h,int v){int i=h&LMMASK;int slot=-1;for(int j=0;j<6;j++){int x=(i+j)&LMMASK;
  if(!lm_used[x]||lm_key[x]==h){slot=x;break;}} if(slot<0)slot=i; lm_used[slot]=1;lm_key[slot]=h;lm_val[slot]=v;}


#define PW_UNKNOWN 255
static int mate_len_tt(Pos*p,int cap){
  CYF_ZYF_TEST_STAGE(2);
  
  if(g_stop || (lm_deadline>0 && now()>=lm_deadline)) return -1;
  if(cap<0) return -1;
  
  uint64_t h=p->key ^ 0xD6E8FEB86659FD93ULL;
  int v; if(lm_get(h,&v)) return v==PW_UNKNOWN?-1:v;
  int is_or=(p->side==g_attacker); int res;
  if(!is_or){
    int mv[MAXMOVES]; int n=gen_legal(p,mv);
    if(n==0){ lm_put(h,0); return 0; }              
    if(cap==0) return -1;   
    int worst=-1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int pn,dn; tt_read(p->key,&pn,&dn);
      int r=(pn==0)?mate_len_tt(p,cap-1):-1; unmake_move(p);
      if(r<0){ lm_put(h,PW_UNKNOWN); return -1; }   
      if(r>worst)worst=r; }                          
    res=1+worst;
  } else {
    if(cap==0) return -1;   
    int mv[MAXMOVES]; int n=gen_checks(p,mv); res=-1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int pn,dn; tt_read(p->key,&pn,&dn);
      int r=(pn==0)?mate_len_tt(p,cap-1):-1; unmake_move(p);
      if(r>=0){ res=1+r; break; } }                  
    if(res<0){ lm_put(h,PW_UNKNOWN); return -1; }
  }
  if(res>=0 && res<PW_UNKNOWN) lm_put(h,res);
  return res;
}

static int mate_length(Pos*p,int maxd,double budget_s);
static int mate_len_fast(Pos*p,int maxd,double budget_s){
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=1;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen;
  if(!setjmp(lm_jmp)){
    int L=mate_len_tt(p, maxd<99?maxd:99);
    g_pathlen=saved;
    if(L>0) return L;
  } else g_pathlen=saved;
  return mate_length(p,maxd,budget_s);   
}

static int mate_len_proof(Pos*p,int cap,double budget_s=0){
  lm_init(); lm_nodes=0; lm_limit=20000000; lm_honor_stop=1;
  lm_deadline=budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen;
  if(setjmp(lm_jmp)){ g_pathlen=saved; return -1; }
  int L=mate_len_tt(p, cap<99?cap:99);
  g_pathlen=saved;
  return L>0?L:-1;
}


static int can_mate(Pos*p,int rem,int*rep_out){
  CYF_ZYF_TEST_STAGE(4);
  if(lm_honor_stop && (g_stop || (lm_deadline>0 && now()>=lm_deadline))) longjmp(lm_jmp,1);
  *rep_out=0;
  
  if((++lm_nodes & 16383)==0){
    if((lm_honor_stop&&g_stop)||lm_nodes>lm_limit||(lm_deadline>0&&now()>lm_deadline)) longjmp(lm_jmp,1); }
  uint64_t h=p->key ^ (0x9E3779B97F4A7C15ULL*(uint64_t)(rem+1));
  int v; if(lm_get(h,&v))return v;              
  int is_or=(p->side==g_attacker); int res, subrep=0;
  if(is_or){
    if(rem<=0){lm_put(h,0);return 0;}
    int pn,dn; tt_read(p->key,&pn,&dn); if(dn==0&&pn>=INF&&twin_find(p->key)<0){lm_put(h,0);return 0;}
    int mv[MAXMOVES]; int n=gen_checks(p,mv); res=0;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int r;
      if(on_path(p->key)){ r=0; *rep_out=1; }    
      else { int cr=0; g_path[g_pathlen++]=p->key; r=can_mate(p,rem-1,&cr); g_pathlen--; subrep|=cr; }
      unmake_move(p); if(r){res=1;break;} }
  } else {
    int mv[MAXMOVES]; int n=gen_legal(p,mv); if(n==0){lm_put(h,1);return 1;}
    if(rem<=0){lm_put(h,0);return 0;}
    int pn,dn; tt_read(p->key,&pn,&dn); if(dn==0&&pn>=INF&&twin_find(p->key)<0){lm_put(h,0);return 0;}
    res=1;
    for(int i=0;i<n;i++){ make_move(p,mv[i]); int r;
      if(on_path(p->key)){ r=0; *rep_out=1; }    
      else { int cr=0; g_path[g_pathlen++]=p->key; r=can_mate(p,rem-1,&cr); g_pathlen--; subrep|=cr; }
      unmake_move(p); if(!r){res=0;break;} }
  }
  int rep_involved = (*rep_out) | subrep; *rep_out = rep_involved;
  if(!rep_involved) lm_put(h,res);              
  return res;
}

static int mate_length(Pos*p,int maxd,double budget_s){
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=1;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int saved=g_pathlen, savedu=usp; const Pos original=*p;
  if(setjmp(lm_jmp)){ g_pathlen=saved; usp=savedu; *p=original; return -1; }
  for(int d=1;d<=maxd;d+=2){ g_pathlen=0; g_path[g_pathlen++]=p->key; int rep=0;
    if(can_mate(p,d,&rep)){ g_pathlen=saved; return d; } }
  g_pathlen=saved; return -1;
}
static int node_dist(Pos*p,int cap){
  int saved=g_pathlen, r=INF;
  for(int d=0;d<=cap;d++){ g_pathlen=0; g_path[g_pathlen++]=p->key; int rep=0;
    if(can_mate(p,d,&rep)){ r=d; break; } }
  g_pathlen=saved; return r;
}


static const char* CSAN[15]={"","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","UM","RY"};
static void move_csa(Pos*p,int m,int col,char*buf){
  int to=M_TO(m),tf=9-COL(to),tr=RNK(to)+1; char sg=col==BLACK?'+':'-';
  if(IS_DROP(m)) sprintf(buf,"%c00%d%d%s",sg,tf,tr,CSAN[M_DROP(m)]);
  else{ int f=M_FROM(m),ff=9-COL(f),fr=RNK(f)+1,mt=ptype(p->b[f]),nt=M_PROMO(m)?PROMO[mt]:mt;
    sprintf(buf,"%c%d%d%d%d%s",sg,ff,fr,tf,tr,CSAN[nt]); } }

static void move_usi(int m,char*buf){
  int to=M_TO(m); char tf='1'+(8-COL(to)),tr='a'+RNK(to);
  if(IS_DROP(m)){ static const char DP[]="?PLNSGBR";
    int dt=M_DROP(m); buf[0]=(dt>=1&&dt<=7)?DP[dt]:'?'; buf[1]='*'; buf[2]=tf; buf[3]=tr; buf[4]=0; }
  else{ int f=M_FROM(m); buf[0]='1'+(8-COL(f)); buf[1]='a'+RNK(f);
    buf[2]=tf; buf[3]=tr; buf[4]=M_PROMO(m)?'+':0; buf[5]=0; } }
static int parse_usi_move(Pos*p,const char*s){
  if(!s||strlen(s)<4) return -1;
  int mv[MAXMOVES],nm=gen_legal(p,mv);
  for(int i=0;i<nm;i++){ char buf[8]; move_usi(mv[i],buf); if(!strcmp(buf,s)) return mv[i]; }
  return -1; }


static void print_pv(Pos*p,int D,char*out,double budget_s,int honor_stop=0){
  CYF_ZYF_TEST_STAGE(3);
  out[0]=0; if(honor_stop && g_stop) return; int cur=D,guard=0; char mb[16];
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=honor_stop; 
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int savedp=g_pathlen, savedu=usp; const Pos original=*p;
  if(setjmp(lm_jmp)){ g_pathlen=savedp; usp=savedu; *p=original; return; }   
  while(cur>0 && guard++<2000){
    if((honor_stop && g_stop) || (lm_deadline>0 && now()>=lm_deadline)) break;
    int best=-1,bd;
    if(p->side==g_attacker){ int mv[MAXMOVES];int n=gen_checks(p,mv); bd=INF;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<bd){bd=d;best=i;}}
      if(best<0)break; move_csa(p,mv[best],p->side,mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    } else { int mv[MAXMOVES];int n=gen_legal(p,mv); bd=-1;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<INF&&d>bd){bd=d;best=i;}}
      if(best<0)break; move_csa(p,mv[best],p->side,mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    }
  }
  g_pathlen=savedp; usp=savedu; *p=original;
}

static int csa_type(const char*s){
  static const char*N[15]={"","FU","KY","KE","GI","KI","KA","HI","OU","TO","NY","NK","NG","UM","RY"};
  for(int t=1;t<15;t++) if(s[0]==N[t][0]&&s[1]==N[t][1]) return t; return 0; }
static int parse_csa(Pos*p,const char*text){
  memset(p,0,sizeof *p); p->ksq[0]=p->ksq[1]=-1; p->side=BLACK; int al=-1;
  const char*s=text; char line[256];
  while(*s){ int n=0; while(*s&&*s!='\n'&&n<255)line[n++]=*s++; line[n]=0; if(*s=='\n')s++;
    while(n>0&&line[n-1]=='\r')line[--n]=0;
    if(n>=2&&line[0]=='P'&&line[1]>='1'&&line[1]<='9'){ int r=line[1]-'1';
      for(int i=0;i<9;i++){ int b=2+3*i; if(b+2>=n)break; char c0=line[b];
        if(c0=='+'||c0=='-'){ int col=c0=='+'?BLACK:WHITE; int t=csa_type(&line[b+1]); if(t)p->b[r*9+i]=mkpc(t,col); } } }
    else if(n>=2&&line[0]=='P'&&(line[1]=='+'||line[1]=='-')){ int col=line[1]=='+'?BLACK:WHITE; int b=2;
      while(b+3<n){ if(line[b+2]=='A'&&line[b+3]=='L'){al=col;break;} int t=csa_type(&line[b+2]); if(t>=1&&t<=7)p->hand[col][t]++; b+=4; } }
    else if(n==1&&(line[0]=='+'||line[0]=='-')) p->side=line[0]=='+'?BLACK:WHITE;
  }
  if(al>=0){ int full[8]={0,18,4,4,4,4,2,2},used[8]={0};
    for(int sq=0;sq<81;sq++){int pc=p->b[sq]; if(pc){int t=ptype(pc),base=DEMO[t]; if(base>=1&&base<=7)used[base]++;}}
    for(int c=0;c<2;c++)for(int t=1;t<=7;t++)used[t]+=p->hand[c][t];
    for(int t=1;t<=7;t++){int rest=full[t]-used[t]; if(rest>0)p->hand[al][t]+=rest;} }
  refresh_kings(p); p->key=compute_key(p); p->bkey=compute_bkey(p); ykey_refresh(p); return 1; }


#define ZYF_RH_MAX 2048
static uint64_t g_rh_key[ZYF_RH_MAX+1]; static unsigned char g_rh_chk[ZYF_RH_MAX]; static int g_rh_n=-1;
static void zyf_set_root_history(const uint64_t*keys,const unsigned char*checks,int nmoves){
  if(!keys||nmoves<0||nmoves>ZYF_RH_MAX){ g_rh_n=-1; return; }
  for(int i=0;i<=nmoves;i++) g_rh_key[i]=keys[i];
  for(int i=0;i<nmoves;i++) g_rh_chk[i]=checks?checks[i]:0;
  g_rh_n=nmoves; }
static void zyf_clear_root_history(void){ g_rh_n=-1; }
static int root_move_perpetual(Pos*p,int mv){
  if(g_rh_n<0 || g_rh_key[g_rh_n]!=p->key) return 0;
  make_move(p,mv); const uint64_t k=p->key; const int chk=in_check(p,p->side); unmake_move(p);
  if(!chk) return 0;
  int occ=1, i0=g_rh_n+1;
  for(int i=0;i<=g_rh_n;i++) if(g_rh_key[i]==k){ occ++; if(i<i0) i0=i; }
  if(occ<4) return 0;
  for(int j=i0;j<g_rh_n;j++) if(((g_rh_n-j)&1)==0 && !g_rh_chk[j]) return 0;
  return 1; }

static int first_mate_move(Pos*p,int honor_stop=0){
  if(honor_stop && g_stop) return -1;
  int mv[MAXMOVES]; int n=gen_checks(p,mv); int first=-1;
  for(int i=0;i<n;i++){ if(honor_stop && g_stop) return -1; if(root_move_perpetual(p,mv[i])) continue;
    int pn,dn; make_move(p,mv[i]); tt_read(p->key,&pn,&dn);
    
    int reply[MAXMOVES]; const int mated=gen_legal(p,reply)==0; unmake_move(p);
    if(mated) return mv[i];
    if(pn==0 && first<0) first=mv[i]; }
  if(first>=0) return first;   
  return -1; }

static void report_solve(Pos p,long nl,double tl,int wantpv){
  Pos p0=p; long nodes; double t=now(),deadline=t+tl;
  int r=solve(&p,nl,tl,&nodes); double dt=now()-t;
  if(r==1){
    double rem=deadline-now(); if(rem<0.05)rem=0.05;
    Pos q=p0; g_attacker=q.side; int L=mate_len_fast(&q,399,rem*0.7);
    if(L>0){ printf("mate %d nodes %ld time %.3f\n",L,nodes,dt);
      if(wantpv){ char pv[8192]; Pos q2=p0; g_attacker=q2.side;
        double rem2=deadline-now(); if(rem2<0.05)rem2=0.05;
        print_pv(&q2,L,pv,rem2); printf("%s\n",pv); } }
    else { printf("mate ? nodes %ld time %.3f\n",nodes,dt);   
      if(wantpv){ Pos q3=p0; g_attacker=q3.side; int fm=first_mate_move(&q3);
        if(fm>=0){ char mb[16]; move_csa(&q3,fm,q3.side,mb); printf("%s\n",mb); } } } }
  else if(r==0) printf("nomate nodes %ld time %.3f\n",nodes,dt);
  else printf("unknown nodes %ld time %.3f\n",nodes,dt); }
static void report_tsumero(Pos p,long nl,double tl){
  int def=p.side;
  if(p.ksq[def]>=0 && is_attacked(&p,p.ksq[def],def^1)){ printf("False (oute/王手)\n"); return; }
  Pos n=p; n.side^=1; n.key^=Z_SIDE; n.bkey^=Z_SIDE; n.ybk^=Y_SIDE;
  double t0=now(),deadline=t0+tl;
  Pos work=n; long nd; int r=solve(&work,nl,tl,&nd);
  if(r==1){ double rem=deadline-now(); if(rem<0.05)rem=0.05;
    Pos q=n; g_attacker=q.side; int L=mate_len_fast(&q,399,rem*0.7);
    if(L>0){ char pv[8192]; Pos q2=n; g_attacker=q2.side;
      double rem2=deadline-now(); if(rem2<0.05)rem2=0.05;
      print_pv(&q2,L,pv,rem2); printf("True (threat %d)\n%s\n",L,pv); }
    else printf("True (threat ?)\n"); }
  else if(r==0) printf("False\n"); else printf("Unknown\n"); }



static int is_tsumero_p(Pos*p,long nl,double tl){ int d=p->side;
  if(p->ksq[d]>=0 && is_attacked(p,p->ksq[d],d^1)) return 0;
  Pos w=*p; w.side^=1; w.key^=Z_SIDE; w.bkey^=Z_SIDE; w.ybk^=Y_SIDE;
  { double left=g_tes_deadline-now(); if(tl>left) tl=left<0.001?0.001:left; }   
  long nd; int r=solve(&w,nl,tl,&nd); return r==1; }
static int defmated(Pos*p){ int d=p->side; return p->ksq[d]>=0 && is_attacked(p,p->ksq[d],d^1) && !has_legal(p,d); }
static int tesuki(Pos*p,int maxn,long nl,double tl){
  if(now()>g_tes_deadline) return -2;
  if(defmated(p)) return 0;
  if(is_tsumero_p(p,nl,tl)) return 1;
  if(maxn<=1) return -1;
  p->side^=1;p->key^=Z_SIDE;p->bkey^=Z_SIDE;p->ybk^=Y_SIDE;
  int mv[MAXMOVES]; int nm=gen_legal(p,mv); int best=-1,unk=0;
  for(int i=0;i<nm;i++){ if(now()>g_tes_deadline){unk=1;break;}   
    make_move(p,mv[i]); int t=tesuki(p,maxn-1,nl,tl); unmake_move(p);
    if(t==-2){unk=1;continue;} if(t>=1){int v=1+t; if(best<0||v<best)best=v; if(best==2)break;} }
  p->side^=1;p->key^=Z_SIDE;p->bkey^=Z_SIDE;p->ybk^=Y_SIDE;
  return best>=1?best:(unk?-2:-1); }
static void report_tesuki(Pos p,int maxn,long nl,double tl,double overall){
  int d=p.side;
  if(p.ksq[d]>=0 && is_attacked(&p,p.ksq[d],d^1)){
    if(!has_legal(&p,d)) printf("tesuki 0 (mated)\n"); else printf("tesuki - (in check)\n"); return; }
  g_tes_deadline=now()+overall; int r=tesuki(&p,maxn,nl,tl);
  if(r==0) printf("tesuki 0 (mated)\n");
  else if(r>=1) printf("tesuki %d\n",r);
  else if(r==-1) printf("tesuki >%d (no threat within cap)\n",maxn);
  else printf("tesuki unknown (budget)\n"); }

static double tes_each(double overall){ double e=overall/4.0; if(e<0.05)e=0.05; return e; }


static void serve_loop(void){
  char line[16384];
  while(fgets(line,sizeof line,stdin)){
    size_t L=strlen(line); while(L&&(line[L-1]=='\n'||line[L-1]=='\r'))line[--L]=0;
    if(!L)continue;
    if(!strcmp(line,"quit"))break;
    usp=0; g_pathlen=0;      
    if(!strcmp(line,"ping")){ printf("pong\n=done\n"); fflush(stdout); continue; }
    long nl; double tl; int off=-1, maxn;
    if(sscanf(line,"solve %ld %lf %n",&nl,&tl,&off)==2&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_solve(p,nl,tl,1); }
    else if(sscanf(line,"tsumero %ld %lf %n",&nl,&tl,&off)==2&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_tsumero(p,nl,tl); }
    else if(sscanf(line,"tesuki %d %ld %lf %n",&maxn,&nl,&tl,&off)==3&&off>0&&line[off]){
      Pos p; if(!parse_sfen(&p,line+off))printf("parse error\n"); else report_tesuki(p,maxn,nl,tes_each(tl),tl); }
    else printf("error unknown command\n");
    printf("=done\n"); fflush(stdout);
  }
}


static Pos g_usi_pos;
static int g_usi_has_pos=0;
static volatile int g_usi_searching=0;
static volatile int g_usi_done=0;
static int g_usi_gomate=0;
static pthread_t g_usi_thread;

typedef struct { long nodes; int result; int mate_len; int bestmove; } UsiResult;
static UsiResult g_usi_result;


static void print_pv_usi(Pos*p,int D,char*out,double budget_s){
  out[0]=0; int cur=D,guard=0; char mb[8];
  lm_init(); lm_nodes=0; lm_limit=200000000; lm_honor_stop=0;
  lm_deadline = budget_s>0 ? now()+budget_s : 0;
  int savedp=g_pathlen;
  if(setjmp(lm_jmp)){ g_pathlen=savedp; return; }
  while(cur>0 && guard++<2000){ int best=-1,bd;
    if(p->side==g_attacker){ int mv[MAXMOVES];int n=gen_checks(p,mv); bd=INF;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<bd){bd=d;best=i;}}
      if(best<0)break; move_usi(mv[best],mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    } else { int mv[MAXMOVES];int n=gen_legal(p,mv); bd=-1;
      for(int i=0;i<n;i++){make_move(p,mv[i]);int d=node_dist(p,cur-1);unmake_move(p);if(d<INF&&d>bd){bd=d;best=i;}}
      if(best<0)break; move_usi(mv[best],mb); if(out[0])strcat(out," "); strcat(out,mb); make_move(p,mv[best]); cur=bd;
    }
  }
  g_pathlen=savedp;
}

static void usi_emit_checkmate(void){
  if(g_usi_result.result==1 && g_usi_result.bestmove>=0){
    int L=g_usi_result.mate_len;
    if(L>0){ char pv[8192]; Pos q=g_usi_pos; g_attacker=q.side;
      print_pv_usi(&q,L,pv,5.0);
      
      char fmb[8]; move_usi(g_usi_result.bestmove,fmb); size_t fl=strlen(fmb);
      if(pv[0] && (strncmp(pv,fmb,fl)!=0 || (pv[fl]!=' ' && pv[fl]!=0))) strcpy(pv,fmb);
      if(pv[0]) printf("checkmate %s\n",pv);
      else { char mb[8]; move_usi(g_usi_result.bestmove,mb); printf("checkmate %s\n",mb); }
    } else { char mb[8]; move_usi(g_usi_result.bestmove,mb); printf("checkmate %s\n",mb); }
  } else if(g_usi_result.result==0){
    printf("checkmate nomate\n");
  } else {
    printf("checkmate timeout\n");
  }
  fflush(stdout);
}

static double g_usi_tlimit=1e9;
static void* usi_solver(void*arg){
  (void)arg;
  Pos p=g_usi_pos;
  long nodes; double tl=g_usi_tlimit;   
  long nl=1000000000L;
  g_usi_result.result=solve(&p,nl,tl,&nodes);
  g_usi_result.nodes=nodes;
  if(g_usi_result.result==1){
    Pos q=g_usi_pos; g_attacker=q.side;
    g_usi_result.mate_len=mate_len_fast(&q,399,5.0);
    Pos q2=g_usi_pos; g_attacker=q2.side;
    g_usi_result.bestmove=first_mate_move(&q2);
  } else { g_usi_result.mate_len=-1; g_usi_result.bestmove=-1; }
  g_usi_done=1;
  if(g_usi_gomate) usi_emit_checkmate();
  g_usi_searching=0;
  return NULL;
}

static void usi_loop(void){
  g_usi_mode=1;
  char line[16384];
  while(fgets(line,sizeof line,stdin)){
    size_t L=strlen(line); while(L&&(line[L-1]=='\n'||line[L-1]=='\r'))line[--L]=0;
    if(!L)continue;

    if(!strcmp(line,"usi")){
      printf("id name Zyfamate\n");
      printf("id author yfamate\n");
      printf("option name USI_Hash type spin default 256 min 16 max 4096\n");
      printf("usiok\n"); fflush(stdout); continue; }

    if(!strcmp(line,"isready")){
      printf("readyok\n"); fflush(stdout); continue; }

    if(!strcmp(line,"usinewgame")){ continue; }

    if(!strncmp(line,"setoption ",10)){
      char name[256]="",val[256]="";
      if(sscanf(line,"setoption name %255s value %255s",name,val)==2){
        if(!strcmp(name,"USI_Hash")){
          int mb=atoi(val); if(mb>=16){
#ifdef _WIN32
            char ev[64]; snprintf(ev,sizeof ev,"ZYFAMATE_HASH_MB=%s",val); _putenv(ev);
#else
            setenv("ZYFAMATE_HASH_MB",val,1);
#endif
          }
        }
      }
      continue; }

    if(!strncmp(line,"position",8)){
      const char*s=line+8; while(*s==' ')s++;
      if(!strncmp(s,"startpos",8)){
        parse_sfen(&g_usi_pos,"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        s+=8; while(*s==' ')s++;
      } else if(!strncmp(s,"sfen ",5)){
        s+=5;
        char sfen_buf[1024]; int si=0;
        int fields=0;
        while(*s && fields<4){ if(*s==' '){ fields++; if(fields<4){sfen_buf[si++]=*s++;continue;} else break; }
          sfen_buf[si++]=*s++; }
        sfen_buf[si]=0;
        if(!parse_sfen(&g_usi_pos,sfen_buf)){ g_usi_has_pos=0; continue; }
        while(*s==' ')s++;
      }
      static uint64_t rh_k[ZYF_RH_MAX+1]; static unsigned char rh_c[ZYF_RH_MAX]; int rh_n=0, rh_ok=1;   
      g_usi_pos.key=compute_key(&g_usi_pos); rh_k[0]=g_usi_pos.key;
      if(!strncmp(s,"moves",5)){
        s+=5; while(*s==' ')s++;
        char tok[16]; int ti;
        while(*s){
          ti=0; while(*s&&*s!=' '&&ti<15)tok[ti++]=*s++; tok[ti]=0;
          while(*s==' ')s++;
          if(!ti)continue;
          int m=parse_usi_move(&g_usi_pos,tok);
          if(m<0){ rh_ok=0; break; }
          make_move(&g_usi_pos,m);
          g_usi_pos.key=compute_key(&g_usi_pos);
          g_usi_pos.bkey=compute_bkey(&g_usi_pos); ykey_refresh(&g_usi_pos);
          if(rh_n<ZYF_RH_MAX){ rh_c[rh_n]=(unsigned char)in_check(&g_usi_pos,g_usi_pos.side); rh_k[++rh_n]=g_usi_pos.key; }
          else rh_ok=0;
        }
      }
      if(rh_ok) zyf_set_root_history(rh_k,rh_c,rh_n); else zyf_clear_root_history();
      g_usi_has_pos=1;
      continue; }

    if(!strncmp(line,"go",2)){
      if(!g_usi_has_pos){ printf("bestmove resign\n"); fflush(stdout); continue; }
      g_usi_gomate=!!strstr(line,"mate");
      g_usi_tlimit=1e9;   
      if(g_usi_gomate){ const char*mp=strstr(line,"mate")+4; while(*mp==' ')mp++;
        if(*mp>='0'&&*mp<='9'){ double ms=atof(mp); if(ms>0) g_usi_tlimit=ms/1000.0; } }
      g_stop=0; g_usi_searching=1; g_usi_done=0;
      usp=0; g_pathlen=0;
      pthread_create(&g_usi_thread,NULL,usi_solver,NULL);
      continue; }

    if(!strcmp(line,"stop")){
      if(g_usi_searching){ g_stop=1; pthread_join(g_usi_thread,NULL); g_usi_searching=0; }
      if(g_usi_gomate){
        if(!g_usi_done) usi_emit_checkmate();
      } else {
        if(g_usi_result.result==1 && g_usi_result.bestmove>=0){
          char mb[8]; move_usi(g_usi_result.bestmove,mb);
          if(g_usi_result.mate_len>0)
            printf("info score mate %d\n",g_usi_result.mate_len);
          printf("bestmove %s\n",mb);
        } else { printf("bestmove resign\n"); }
      }
      fflush(stdout); continue; }

    if(!strcmp(line,"quit")){
      if(g_usi_searching){ g_stop=1; pthread_join(g_usi_thread,NULL); }
      break; }

    if(!strcmp(line,"gameover")||!strncmp(line,"gameover ",9)){ continue; }
  }
}
#undef malloc
#undef calloc
} 



#undef ADD
#undef BLACK
#undef DROP_FROM
#undef FILL
#undef INF
#undef IS_DROP
#undef IS_MATED_KEEPPINS
#undef LMBITS
#undef LMMASK
#undef LMSIZE
#undef M3BITS
#undef M3MASK
#undef M3SIZE
#undef MAXMOVES
#undef MFD
#undef MV
#undef M_DROP
#undef M_FROM
#undef M_PROMO
#undef M_TO
#undef PW_UNKNOWN
#undef STBITS
#undef STMASK
#undef STPROBE
#undef STSIZE
#undef TTMASK
#undef TTSIZE
#undef TWK
#undef TWMASK
#undef TWSIZE
#undef WHITE
#undef WKMASK
#undef WKSIZE
#undef WOFF
#undef ZCALLOC
#undef ZMALLOC
#undef ZYF_MAX_DEPTH
#undef ZY_SEED_MAX
#pragma pop_macro("SQ")
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
static bool _zyfamate_inited = (Zyfamate::init_tables(), true);  
static int cyf_zyf_child_entry(int argc,char* argv[]){
    if(argc!=7)return 2;
    using namespace Zyfamate;
    const int variant=std::atoi(argv[2]),hash=std::atoi(argv[3]);
    const long nodes=std::atol(argv[4]);const double seconds=std::atof(argv[5]);
    if(variant<0||variant>63||hash<16||hash>65536||nodes<1||seconds<=0||seconds>120)return 2;
    char hb[32];std::snprintf(hb,sizeof(hb),"%d",hash);_putenv_s("ZYFAMATE_HASH_MB",hb);
    Pos p;if(!parse_sfen(&p,argv[6]))return 2;
    g_order_variant=unsigned(variant);g_stop=0;long visited=0;
    const int result=solve(&p,nodes,seconds,&visited);
    int len=0;char move[16]="-";
    if(result==1){
        
        int first=first_mate_move(&p);
        if(first>=0)move_csa(&p,first,p.side,move);
        else {char pv[20000]{};
            if(pv[0])std::sscanf(pv,"%15s",move);}
    }
    std::printf("CYF_ZYF_RESULT %d %d %s\n",result,len,move);std::fflush(stdout);return 0;
}


#ifndef CYFAMATE_H_INCLUDED
#define CYFAMATE_H_INCLUDED



























#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace YaneuraOu {
namespace Cyfamate {













struct GamePhaseParams {
	
	double hand_val[7]  = {1.0, 2.775113, 2.066253, 3.120539, 3.911216, 4.574237, 7.863081};
	double camp_val[13] = {1.396302, 1.177588, 1.248085, 1.297911, 1.444452, 1.066618, 1.40945,
	                       0.563566, 1.186525, 0.91197, 1.020019, 0.592588, 1.458039};
	
	double w_hand=0.334803, w_promo=0.0,      w_camp=0.312461, w_king=0.540983;
	double ref_hand=2.035839, ref_promo=5.443070, ref_camp=1.173523, ref_king=0.121706;
	double w_expo=0.0, ref_expo=12.212311;
	double w_dev=0.0, ref_dev=15.0, w_contact=0.0, ref_contact=4.0;
	double scale=1.285300, nyugyoku_onset=0.514070, nyugyoku_full=4.757159;
};
struct GamePhaseFeatures {
	double whand=0, promo=0, wcamp=0, king=0, expo=0, dev=0, contact=0;
};
inline int gp_piece_idx(PieceType pt){   
	switch (pt) {
	case PAWN: return 0; case LANCE: return 1; case KNIGHT: return 2; case SILVER: return 3;
	case GOLD: return 4; case BISHOP: return 5; case ROOK: return 6;
	case PRO_PAWN: return 7; case PRO_LANCE: return 8; case PRO_KNIGHT: return 9;
	case PRO_SILVER: return 10; case HORSE: return 11; case DRAGON: return 12;
	default: return -1;
	}
}

inline const std::array<Piece, SQ_NB>& gp_initial_squares(){
	static const std::array<Piece, SQ_NB> t = []{
		std::array<Piece, SQ_NB> a; a.fill(NO_PIECE);
		const PieceType back[9] = {LANCE, KNIGHT, SILVER, GOLD, YaneuraOu::KING, GOLD, SILVER, KNIGHT, LANCE};
		for (int f = 0; f < 9; ++f) {
			a[(File)f | RANK_3] = make_piece(WHITE, PAWN);
			a[(File)f | RANK_7] = make_piece(BLACK, PAWN);
			a[(File)f | RANK_1] = make_piece(WHITE, back[f]);
			a[(File)f | RANK_9] = make_piece(BLACK, back[f]);
		}
		a[FILE_8 | RANK_2] = make_piece(WHITE, ROOK);   a[FILE_2 | RANK_2] = make_piece(WHITE, BISHOP);
		a[FILE_8 | RANK_8] = make_piece(BLACK, BISHOP); a[FILE_2 | RANK_8] = make_piece(BLACK, ROOK);
		return a;
	}();
	return t;
}

inline const std::array<std::array<Bitboard, SQ_NB>, COLOR_NB>& gp_king_support_masks(){
	static const auto t = []{
		std::array<std::array<Bitboard, SQ_NB>, COLOR_NB> m;
		for (Color c : {BLACK, WHITE})
			for (Square k = SQ_ZERO; k < SQ_NB; ++k) {
				Bitboard bb(ZERO);
				const int kf = (int)file_of(k), kr = (int)rank_of(k) + 1;
				for (Square s = SQ_ZERO; s < SQ_NB; ++s) {
					const int sf = (int)file_of(s), sr = (int)rank_of(s) + 1;
					if (s != k && std::max(std::abs(sf - kf), std::abs(sr - kr)) <= 2
					    && (c == BLACK ? sr <= kr + 1 : sr >= kr - 1)) bb |= s;
				}
				m[c][k] = bb;
			}
		return m;
	}();
	return t;
}

inline double gp_king_term(const Position& pos, int& ring1){
	const Bitboard kings = pos.pieces(YaneuraOu::KING);
	const auto& sup_mask = gp_king_support_masks();
	double king = 0.0;
	ring1 = 0;
	for (Color c : {BLACK, WHITE}) {
		const Square ksq = pos.square<YaneuraOu::KING>(c);
		if (ksq >= SQ_NB) continue;
		const Bitboard own = pos.pieces(c) & ~kings;
		ring1 += (kingEffect(ksq) & own).pop_count();
		const int kr = (int)rank_of(ksq) + 1;   
		const int raw = (c == BLACK) ? (7 - kr) : (kr - 3);
		if (raw < 0) continue;
		const int sup = std::min(3, (own & sup_mask[c][ksq]).pop_count());
		king += (raw + 0.5) * (0.25 + 0.25 * sup);
	}
	return king;
}



inline GamePhaseFeatures extract_features(const Position& pos, const GamePhaseParams& p){
	GamePhaseFeatures F;
	static const PieceType hand_pts[7] = {PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP, ROOK};
	static const PieceType camp_pts[13] = {PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP, ROOK,
	                                       PRO_PAWN, PRO_LANCE, PRO_KNIGHT, PRO_SILVER, HORSE, DRAGON};
	const Bitboard kings = pos.pieces(YaneuraOu::KING);
	for (Color c : {BLACK, WHITE}) {
		const Hand h = pos.hand_of(c);
		for (int i = 0; i < 7; ++i) F.whand += hand_count(h, hand_pts[i]) * p.hand_val[i];
		const Bitboard ef = enemy_field(c);
		for (int i = 0; i < 13; ++i) F.wcamp += (pos.pieces(c, camp_pts[i]) & ef).pop_count() * p.camp_val[i];
	}
	F.promo = (double)pos.pieces(PRO_PAWN, PRO_LANCE, PRO_KNIGHT, PRO_SILVER, HORSE, DRAGON).pop_count();
	int ring1 = 0;
	F.king = gp_king_term(pos, ring1);
	F.expo = 16.0 - ring1;
	const bool need_dev = p.w_dev != 0.0;
	if (need_dev || p.w_contact != 0.0) {
		const auto& init = gp_initial_squares();
		const Bitboard occ = pos.pieces();
		for (Square sq = SQ_ZERO; sq < SQ_NB; ++sq) {
			const Piece pc = pos.piece_on(sq);
			if (need_dev && init[sq] != NO_PIECE && pc != init[sq]) F.dev += 1;
			if (p.w_contact != 0.0 && pc != NO_PIECE && type_of(pc) != YaneuraOu::KING)
				F.contact += (effects_from(pc, sq, occ) & pos.pieces(~color_of(pc)) & ~kings).pop_count();
		}
	}
	return F;
}
inline double game_phase_state(const GamePhaseFeatures& F, const GamePhaseParams& p){
	const double S = p.w_hand*F.whand/p.ref_hand + p.w_promo*F.promo/p.ref_promo
	               + p.w_camp*F.wcamp/p.ref_camp + p.w_king*F.king/p.ref_king
	               + p.w_expo*F.expo/p.ref_expo
	               + p.w_dev*F.dev/p.ref_dev + p.w_contact*F.contact/p.ref_contact;
	const double neg = -S/p.scale;
	const double core = (neg < -10.0) ? 1.0 : (1.0 - std::exp(neg));
	double bonus = 0.0;
	if (F.king > p.nyugyoku_onset) {
		double f = (F.king - p.nyugyoku_onset)/(p.nyugyoku_full - p.nyugyoku_onset);
		if (f > 1.0) f = 1.0;
		bonus = 0.5 * f;
	}
	return std::max(0.0, std::min(1.5, core + bonus));
}
inline double game_phase_for_phase(double gp){ return gp<0.0?0.0:(gp>1.0?1.0:gp); }


inline double dl_policy_scale(double gp,double onset=0.6,double floor=0.3){
	const double g = game_phase_for_phase(gp);
	if (g <= onset) return 1.0;
	const double t = (g - onset)/(1.0 - onset);
	const double s = 1.0 - (1.0 - floor)*t;
	return s<floor?floor:(s>1.0?1.0:s);
}
inline double game_phase(const Position& pos,const GamePhaseParams& p){
	return game_phase_state(extract_features(pos, p), p);
}
#if defined(CYFAMATE_NNUE_5GP)









inline const GamePhaseParams kGpParams{};   

inline const bool gp_inc_tables_built = []{
	const double a_hand = kGpParams.w_hand / kGpParams.ref_hand;
	const double a_camp = kGpParams.w_camp / kGpParams.ref_camp;
	for (int pt = 0; pt < PIECE_TYPE_NB; ++pt) {
		const int i = gp_piece_idx((PieceType)pt);
		gp_s_camp_by_pt[pt] = i >= 0 ? a_camp * kGpParams.camp_val[i] : 0.0;
		gp_s_hand_by_pt[pt] = (i >= 0 && i < 7) ? a_hand * kGpParams.hand_val[i] : 0.0;
	}
	gp_s_promo = kGpParams.w_promo / kGpParams.ref_promo;
	return true;
}();
void gp_init_parts(const Position& pos, StateInfo* st){
	const GamePhaseFeatures F = extract_features(pos, kGpParams);
	const GamePhaseParams& p = kGpParams;
    st->gp_s = p.w_hand * F.whand / p.ref_hand + p.w_promo * F.promo / p.ref_promo + p.w_camp * F.wcamp / p.ref_camp;
    st->gp_king_code = st->gp_bucket = 255;
    st->gp_value_valid = false;
}

inline bool gp_inc_usable(){
	const GamePhaseParams& p = kGpParams;
	return p.w_expo == 0.0 && p.w_dev == 0.0 && p.w_contact == 0.0;
}

inline bool gp_king_in_zone(const Position& pos){
	const Square kb = pos.square<YaneuraOu::KING>(BLACK), kw = pos.square<YaneuraOu::KING>(WHITE);
	return (kb < SQ_NB && (int)rank_of(kb) <= 6) || (kw < SQ_NB && (int)rank_of(kw) >= 2);
}
#if defined(CYF_GP8)


constexpr int kGpTopStack = Gp8::kMaxStacks - 1;
inline int gp_top_stack() { return Gp8::stacks - 1; }
#else
constexpr int kGpTopStack = Eval::NNUE::LayerStacks - 1;
#endif 
inline int gp_bucket_of(double gp){
	int b = 0;
#if defined(CYF_GP8)
	const int top = gp_top_stack();
	while (b < top && gp >= Gp8::bounds[b]) ++b;
#else
	while (b < kGpTopStack && gp >= Eval::NNUE::kGpStackBounds[b]) ++b;
#endif 
	return b;
}

#if defined(CYF_GP8)
inline std::array<double, kGpTopStack> gp_stack_s_bounds{};
#else
inline const auto kGpStackSBounds = []{
	std::array<double, Eval::NNUE::LayerStacks - 1> sb{};
	for (int i = 0; i < kGpTopStack; ++i) {
		const double b = Eval::NNUE::kGpStackBounds[i];
		sb[i] = b <= 0.0 ? -1e300 : (b >= 1.0 ? 1e300 : -kGpParams.scale * std::log(1.0 - b));
	}
	return sb;
}();
#endif 
struct GpProgressTerms { double sum, bonus; };
inline double gp_progress_value(GpProgressTerms terms){
    const double neg = -terms.sum / kGpParams.scale;
    const double core = neg < -10.0 ? 1.0 : 1.0 - std::exp(neg);
    return std::clamp(core + terms.bonus, 0.0, 1.5);
}




inline unsigned gp_king_code(const Position& pos) {
    auto* st = pos.state();
    if (st->gp_king_code != 255) return st->gp_king_code;
    unsigned code = 0;
    if (gp_king_in_zone(pos)) {
        const auto& masks = gp_king_support_masks();
        for (Color c : {BLACK, WHITE}) {
            const Square k = pos.square<YaneuraOu::KING>(c);
            if (k >= SQ_NB) continue;
            const int rank = int(rank_of(k));
            const int advance = c == BLACK ? 6-rank : rank-2;
            if (advance < 0) continue;
            const int support = std::min(3, (pos.pieces(c) & masks[c][k]).pop_count());
            code += (2*advance+1)*(support+1);
        }
    }
    st->gp_king_code = std::uint8_t(code);
    return code;
}

struct alignas(64) GpKingRow {
    double boundary[kGpTopStack];
    double offset, bonus;
};



inline int gp_bucket_reference(double sum, double bonus) {
    if (bonus == 0.0 && sum < 10.0*kGpParams.scale) {
        int b=0;
#if defined(CYF_GP8)
        const int top = gp_top_stack();
        while (b<top && sum>=gp_stack_s_bounds[b]) ++b;
#else
        while (b<kGpTopStack && sum>=kGpStackSBounds[b]) ++b;
#endif 
        return b;
    }
    return gp_bucket_of(gp_progress_value({sum,bonus}));
}
inline std::uint64_t gp_ordered_double(double x) {
    std::uint64_t bits;
    std::memcpy(&bits,&x,sizeof(bits));
    return bits & (std::uint64_t(1)<<63) ? ~bits : bits ^ (std::uint64_t(1)<<63);
}
inline double gp_from_ordered_double(std::uint64_t key) {
    std::uint64_t bits = key & (std::uint64_t(1)<<63) ? key ^ (std::uint64_t(1)<<63) : ~key;
    double x;
    std::memcpy(&x,&bits,sizeof(x));
    return x;
}
#if defined(CYF_GP8)
inline std::array<GpKingRow,105> gp_king_rows{};


inline void gp8_rebuild_tables() {
#else
inline const auto kGpKingRows = [] {
#endif 
    static_assert(sizeof(double)==sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559);
#if defined(CYF_GP8)
    const int top = gp_top_stack();
    for (int i = 0; i < kGpTopStack; ++i) {
        const double b = i < top ? Gp8::bounds[i] : 1e300;
        gp_stack_s_bounds[i] = b <= 0.0 ? -1e300 : (b >= 1.0 ? 1e300 : -kGpParams.scale * std::log(1.0 - b));
    }
    for (unsigned code=0; code<gp_king_rows.size(); ++code) {
        auto& row=gp_king_rows[code];
#else
    std::array<GpKingRow,105> rows{};
    for (unsigned code=0; code<rows.size(); ++code) {
        auto& row=rows[code];
#endif 
        const double king=double(code)/8.0;
        row.offset=kGpParams.w_king*king/kGpParams.ref_king;
        row.bonus=king>kGpParams.nyugyoku_onset
            ? 0.5*std::min(1.0,(king-kGpParams.nyugyoku_onset)
                                     /(kGpParams.nyugyoku_full-kGpParams.nyugyoku_onset)) : 0.0;
        for (int b=0;b<kGpTopStack;++b) {
#if defined(CYF_GP8)
            if (b >= top) { row.boundary[b] = std::numeric_limits<double>::infinity(); continue; }
#endif 
            
            
            
            auto low=gp_ordered_double(-std::numeric_limits<double>::infinity());
            auto high=gp_ordered_double(std::numeric_limits<double>::infinity());
            while (high-low>1) {
                const auto mid=low+(high-low)/2;
                if (gp_bucket_reference(gp_from_ordered_double(mid)+row.offset,row.bonus)>b)
                    high=mid;
                else low=mid;
            }
            row.boundary[b]=gp_from_ordered_double(high);
        }
    }
#if defined(CYF_GP8)
}
inline const bool gp8_tables_initialised = [] { gp8_rebuild_tables(); return true; }();
#else
    return rows;
}();
#endif 
inline GpProgressTerms gp_progress_terms(const Position& pos) {
#if defined(CYF_GP8)
    const auto& row=gp_king_rows[gp_king_code(pos)];
#else
    const auto& row=kGpKingRows[gp_king_code(pos)];
#endif 
    return {pos.state()->gp_s+row.offset,row.bonus};
}
inline double game_phase(const Position& pos) {
    if (!gp_inc_usable()) return game_phase(pos,kGpParams);
    auto* st=pos.state();
    if (!st->gp_value_valid) {
        st->gp_value=gp_progress_value(gp_progress_terms(pos));
        st->gp_value_valid=true;
    }
    return st->gp_value;
}
int gp_stack_bucket(const Position& pos) {
    if (!gp_inc_usable()) return gp_bucket_of(game_phase(pos,kGpParams));
    auto* st=pos.state();
    if (st->gp_bucket!=255) return st->gp_bucket;
#if defined(CYF_GP8)
    const auto& row=gp_king_rows[gp_king_code(pos)];
#else
    const auto& row=kGpKingRows[gp_king_code(pos)];
#endif 
    const double s=st->gp_s;
#if defined(CYF_GP8)
    
    int b = 0;
    const int top = gp_top_stack();
    while (b < top && s >= row.boundary[b]) ++b;
#else
    
    const int b=s<row.boundary[1]
        ? int(s>=row.boundary[0])
        : (s<row.boundary[2] ? 2 : 3+int(s>=row.boundary[3]));
#endif 
    st->gp_bucket=std::uint8_t(b);
    return b;
}
#else
inline double game_phase(const Position& pos){ return game_phase(pos,GamePhaseParams{}); }
#endif 
inline const char* phase_label(double gp){
	if (gp > 1.0) return "nyugyoku";
	const double g = game_phase_for_phase(gp);
	if (g < 0.33) return "opening";
	if (g < 0.66) return "middlegame";
	return "endgame";
}




struct Options {
	bool   enable        = false;  
	bool   verbose       = true;   
	double gp_onset      = 0.6;    
	double gp_floor      = 0.3;    
	double dl_conf_min   = 0.30;   
	
	int    probe_min_depth   = 0;    
	int    probe_cap_k       = 4096; 
	
	
	
	
	
	
	int    multiponder_topk  = 0;    
	bool   mate_enable       = true; 
	bool   mt_obs            = false; 
	bool   mt_privhist       = true; 
	bool   vote_depth        = true; 
	int    mate_threads      = 0;    
	int    zyf_share         = 1;    
	
	bool   zyf_service       = true;  
    bool   zyf_play_hint     = false; 
                                      
	bool   zyf_keep_proofs   = false; 
	bool   zyf_tree_share    = false; 
	int    zyf_tree_max      = 1024;  
	bool   zyf_tree_extend   = false; 
	bool   zyf_tree_live     = false; 
	bool   zyf_tree_bound    = false; 
	                                  
	                                  
	                                  
	int    zyf_go_jobs       = 8;     
	
	
	bool   zyf_defense       = true;  
	int    zyf_defense_jobs  = 16;    
	int    zyf_defense_ms    = 200;   
	int    zyf_defense_nodes = 1000000; 
	int    zyf_tonshi_switch = 1;     
	                                  
	                                  
	                                  
	bool   zyf_safe_order    = true;  
	int    zyf_job_nodes     = 100000;
	int    zyf_job_ms        = 50;    
	int    zyf_root_nodes    = 0;     
	int    zyf_root_ms       = 0;     
	int    zyf_pv_plies      = 6;     
	int    zyf_cands         = 3;     
	int    zyf_min_depth     = 6;     
	int    mate_hash_mb      = 256;  
	int    mate_probe_depth  = 10;   
	int    mpon_node_limit   = 200000;  
	int    tonshi_ms         = 60;   
	
	
	int    nncache_mb        = 64;   
	
	bool   tm_enable         = false; 
	int    tm_agree          = 92;   
	int    tm_disagree       = 115;  
	
	
	int    tm_gp_early       = 96;   
	int    tm_gp_mid         = 112;  
	int    tm_score_start_cp = 1000; 
	int    tm_score_floor    = 75;   
	
	int    tm_gp_gamma       = 100;  
	int    tm_momentum_ext   = 25;   
	int    tm_momentum_acc   = 10;   
	int    tm_sharp_base     = 85;   
	bool   tm_sharp_current  = false; 
	int    tm_sharp_range    = 30;   
	int    tm_tesuki_ms      = 40;   
	int    sp_credit         = 0;    
	int    tm_ponder_bonus   = 125;  
};

constexpr double CYF_TM_CEIL  = 1.25;
constexpr double CYF_TM_FLOOR = 0.05;
inline Options& options(){ static Options o; return o; }



inline int& tm_optimum_hint(){ static int h = 0; return h; }










static const char CYF_DL_EMBEDDED_SRC[] = R"CYFDL(
#!/usr/bin/env python3
# =============================================================================
# cyfamate_dl.py  ―  cyfamate C++ エンジンの DL sidecar (v2 プロトコル)
#
# cyfamate.cpp の DlSidecar が  `python cyfamate_dl.py <model.onnx>`  で常駐起動する。
# 標準入出力の行プロトコル:
#   起動:  READY v2
#   C→S: POLICY <id> <sfen>   S→C: POL <id> [usi:prob ...]
#   C→S: VALUE  <id> <sfen>   S→C: VAL <id> <wr 0..1> [usi:prob ...]
#   C→S: BATCHHINT <n>        (無視: 本実装は逐次)
#   C→S: sfen <sfen>          S→C: OK [usi:prob ...]   (旧プロトコル)
#   stdin EOF で終了。応答は必ず flush。
#
# ★本ファイルの正は cyfamate.cpp の CYF_DL_EMBEDDED_SRC (逐語埋め込み)。直接編集するな。
#
# エンコーダ/前処理は Pyfamate.py から **逐語移植** (二重実装のズレ=silent poison を避ける):
#   定数群 / sv_sq / SvBoard.set_sfen / sv_geom / sv_img_to_np / _sv_find_cjk_font /
#   sv_render_board / sv_encode_planes  ―  すべて Pyfamate.py (2026-07-16) と同一コード。
#
# 入力形状で経路を自動判定 (Pyfamate _SvInfer._planes と同じ分岐):
#   [B,3,H,W]  → 画像モデル (現 model.onnx)。sv_IMG=H で盤描画 → sv_img_to_np。
#   [B,43,9,9] → 平面モデル (将来 arbiter.onnx)。sv_encode_planes。
#
# value: ONNX は raw logit を出す契約 (Pyfamate L37209-37215)。wr = sigmoid(clamp(logit,-60,60))
#   = **手番側勝率**。_SvInfer.infer の `if side==1: v=1-v` は表示用の先手正規化であり、
#   C++ 側が期待するのは手番側勝率なので **flip は持ち込まない** (符号事故防止)。
#
# policy: named output "policy" (13689 raw logit) を持つモデル (arbiter.onnx) で復号する。
#   復号規約は Pyfamate _compute_trunk (L36887) と同一: 全 13689 次元の max-shift softmax。
#   **top-K 送出でも再正規化しない** — C++ 側の dl_conf_min は絶対確信度の
#   閾値であり、再正規化すると意味が壊れる。ラベル→USI は sv_label_to_usi (逐語移植) で復号。
#   合法手フィルタは持たない (C++ 側が自合法手と突合し非合法ラベルは自然脱落)。
#   policy ヘッド無しモデル (現 model.onnx) は従来どおり即・空 POL (P1/council no-op)。
#   出力は名前で解決 (value/gp/policy/sharp) — インデックス決め打ちは arbiter.onnx で事故る。
# =============================================================================
import os, sys, re, glob, io, math, random
import numpy as np

# ---------------------------------------------------------------------------
# 以下、Pyfamate.py からの逐語移植（改変禁止。改変は描画/エンコードのズレ=教師汚染に直結）
# ---------------------------------------------------------------------------
sv_PIECES = ["FU", "KY", "KE", "GI", "KI", "KA", "HI", "OU",
          "TO", "NY", "NK", "NG", "UM", "RY"]
sv_N_PIECE = len(sv_PIECES)               # 14
sv_PROMOTE = {"FU": "TO", "KY": "NY", "KE": "NK", "GI": "NG", "KA": "UM", "HI": "RY"}
sv_UNPROMOTE = {v: k for k, v in sv_PROMOTE.items()}
sv_HAND_ORDER = ["FU", "KY", "KE", "GI", "KI", "KA", "HI"]   # 持ち駒 7 種
sv_KANJI = {"FU": "歩", "KY": "香", "KE": "桂", "GI": "銀", "KI": "金", "KA": "角",
         "HI": "飛", "OU": "玉", "TO": "と", "NY": "杏", "NK": "圭", "NG": "全",
         "UM": "馬", "RY": "竜"}
sv_ASCII_PC = {"FU": "P", "KY": "L", "KE": "N", "GI": "S", "KI": "G", "KA": "B",
            "HI": "R", "OU": "K", "TO": "+P", "NY": "+L", "NK": "+N", "NG": "+S",
            "UM": "+B", "RY": "+R"}
sv_SFEN_PC = {"P": "FU", "L": "KY", "N": "KE", "S": "GI", "G": "KI", "B": "KA",
           "R": "HI", "K": "OU"}
sv_SFEN_PROM = {"+P": "TO", "+L": "NY", "+N": "NK", "+S": "NG", "+B": "UM", "+R": "RY"}
sv_PC_SFEN = {v: k for k, v in sv_SFEN_PC.items()}
sv_PC_SFEN.update({v: k for k, v in sv_SFEN_PROM.items()})

sv_IMG = 256          # ONNX 入力形状から実行時に上書き (144 等)
sv_GRAY = False       # モデルは 3ch。1ch なら True
sv_N_PLANES = 28 + 14 + 1   # 43


def sv_sq(file: int, rank: int) -> int:
    """(筋1-9, 段1-9) → 0..80。筋9段1が左上=index0 (画像座標と揃える)。"""
    return (rank - 1) * 9 + (9 - file)


class SvBoard:
    """局面。cells[81] は駒 id。set_sfen のみ移植 (推論に必要な範囲)。"""

    def __init__(self):
        self.cells = [0] * 81
        self.hand = [[0] * 7, [0] * 7]   # [先手, 後手] x sv_HAND_ORDER
        self.side = 0                    # 0=先手, 1=後手
        self.ply = 1
        self.last_to = None

    @staticmethod
    def pid(name: str, owner: int) -> int:
        return 1 + sv_PIECES.index(name) + owner * sv_N_PIECE

    @staticmethod
    def pname(pid: int):
        if pid == 0:
            return None, None
        pid -= 1
        return sv_PIECES[pid % sv_N_PIECE], pid // sv_N_PIECE

    def set_startpos(self):
        self.__init__()
        back = ["KY", "KE", "GI", "KI", "OU", "KI", "GI", "KE", "KY"]
        for i, n in enumerate(back):
            self.cells[sv_sq(9 - i, 1)] = self.pid(n, 1)
            self.cells[sv_sq(9 - i, 9)] = self.pid(n, 0)
        self.cells[sv_sq(2, 2)] = self.pid("KA", 1); self.cells[sv_sq(8, 2)] = self.pid("HI", 1)
        self.cells[sv_sq(8, 8)] = self.pid("KA", 0); self.cells[sv_sq(2, 8)] = self.pid("HI", 0)
        for f in range(1, 10):
            self.cells[sv_sq(f, 3)] = self.pid("FU", 1)
            self.cells[sv_sq(f, 7)] = self.pid("FU", 0)

    def set_sfen(self, sfen: str):
        parts = sfen.split()
        if parts and parts[0] in ("sfen", "position"):
            parts = parts[1:]
        if parts and parts[0] == "startpos":
            self.set_startpos()
            return
        board_s, turn = parts[0], (parts[1] if len(parts) > 1 else "b")
        hands = parts[2] if len(parts) > 2 else "-"
        self.__init__()
        r = 1
        for row in board_s.split("/"):
            f = 9
            i = 0
            while i < len(row):
                c = row[i]
                if c.isdigit():
                    f -= int(c)
                    i += 1
                    continue
                if c == "+":
                    c = row[i + 1]
                    i += 1
                    name = sv_SFEN_PROM["+" + c.upper()]
                else:
                    name = sv_SFEN_PC[c.upper()]
                self.cells[sv_sq(f, r)] = self.pid(name, 0 if c.isupper() else 1)
                f -= 1
                i += 1
            r += 1
        self.side = 0 if turn == "b" else 1
        if hands != "-":
            for m in re.finditer(r"(\d*)([PLNSGBRplnsgbr])", hands):
                n = int(m.group(1) or 1)
                ch = m.group(2)
                self.hand[0 if ch.isupper() else 1][sv_HAND_ORDER.index(sv_SFEN_PC[ch.upper()])] += n
        if len(parts) > 3 and parts[3].isdigit():
            self.ply = int(parts[3])


def sv_geom(rng=None, augment=False):
    cw = sv_IMG // 11
    ch = sv_IMG // 9
    ex = (sv_IMG - 11 * cw) // 2
    ey = (sv_IMG - 9 * ch) // 2
    px = [ex + cw * (1 + i) for i in range(10)]
    py = [ey + ch * i for i in range(10)]
    return cw, ch, px, py, ex, ex + 10 * cw


def sv_img_to_np(im):
    if not isinstance(im, np.ndarray):
        if sv_GRAY:
            arr = np.asarray(im.convert("L"), np.float32)[None, :, :]
        else:
            arr = np.asarray(im, np.float32).transpose(2, 0, 1)
    else:
        a3 = im.astype(np.float32)
        if sv_GRAY:
            arr = (a3 @ np.asarray([0.299, 0.587, 0.114], np.float32))[None, :, :]
        else:
            arr = a3.transpose(2, 0, 1)
    arr /= 255.0
    return np.ascontiguousarray(arr)


def _sv_find_cjk_font():
    cands = []
    for d in ("/usr/share/fonts", os.path.expanduser("~/.fonts"),
              "C:/Windows/Fonts", "/System/Library/Fonts",
              os.path.expanduser("~/Library/Fonts")):
        for pat in ("**/*CJK*.tt*", "**/*CJK*.otf", "**/ipag*.ttf", "**/meiryo*.ttc",
                    "**/YuGoth*.ttc", "**/msgothic*.ttc", "**/*Gothic*.tt*"):
            cands += glob.glob(os.path.join(d, pat), recursive=True)
    def _rank(p):
        n = os.path.basename(p).lower()
        return (0 if "meiryo" in n else 1 if "yugoth" in n
                else 2 if "msgothic" in n else 3)
    return sorted(dict.fromkeys(cands), key=_rank)


def sv_render_board(b, rng, augment=True, last_to=None, strength=0.5):
    from PIL import Image, ImageDraw, ImageFont, ImageFilter
    _g = lambda lo, hi: (lambda v: (v, v, v))(rng.randint(lo, hi))
    bg = _g(245, 255) if augment else (255, 255, 255)
    board_c = _g(245, 255) if augment else (255, 255, 255)
    line_c = _g(0, 40) if augment else (0, 0, 0)
    im = Image.new("RGB", (sv_IMG, sv_IMG), bg)
    d = ImageDraw.Draw(im)
    cw, ch, px, py, x_gote, x_sente = sv_geom(rng, augment)
    d.rectangle([px[0], py[0], px[9], py[9]], fill=board_c)
    lw = 1
    for i in range(10):
        d.line([px[i], py[0], px[i], py[9]], fill=line_c, width=lw)
        d.line([px[0], py[i], px[9], py[i]], fill=line_c, width=lw)
    _last_cell = None
    if last_to:
        f, r = last_to
        c_, r_ = 9 - f, r - 1
        _last_cell = (r_, c_)
        d.rectangle([px[c_] + lw, py[r_] + lw, px[c_ + 1] - lw, py[r_ + 1] - lw],
                    fill=line_c)
    use_kanji = bool(_sv_FONTS)
    fsz = int(min(cw, ch) * (rng.uniform(0.72, 0.86) if augment else 0.8))
    font = None
    _font_path = None
    if use_kanji:
        try:
            _font_path = rng.choice(_sv_FONTS) if augment else _sv_FONTS[0]
            font = ImageFont.truetype(_font_path, fsz)
        except Exception:
            use_kanji = False
    if not use_kanji:
        font = ImageFont.load_default()
    _jit = 1.0 * strength if augment else 0.0

    def _piece_tile(name, owner, col):
        txt = sv_KANJI[name] if use_kanji else sv_ASCII_PC[name]
        tile = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        td = ImageDraw.Draw(tile)
        td.text((cw / 2 + rng.uniform(-_jit, _jit), ch / 2 + rng.uniform(-_jit, _jit)),
                txt, font=font, fill=col, anchor="mm")
        if owner == 1:
            tile = tile.rotate(180)
        return tile

    for idx, pid in enumerate(b.cells):
        if pid == 0:
            continue
        name, owner = SvBoard.pname(pid)
        r_, c_ = divmod(idx, 9)
        col = (255, 255, 255) if (r_, c_) == _last_cell else line_c
        im.paste(t := _piece_tile(name, owner, col), (px[c_], py[r_]), t)

    try:
        _cnt_font = (ImageFont.truetype(_font_path, max(8, int(min(cw, ch) * 0.45)))
                     if use_kanji else ImageFont.load_default())
    except Exception:
        _cnt_font = ImageFont.load_default()
    for owner, x in ((1, x_gote), (0, x_sente)):
        items = [(sv_HAND_ORDER[i], n) for i, n in enumerate(b.hand[owner]) if n]
        for k, (name, n) in enumerate(items):
            y = (py[0] + k * ch) if owner == 1 else (py[9] - (k + 1) * ch)
            im.paste(t := _piece_tile(name, owner, line_c), (x, y), t)
            if n > 1:
                d.text((x + cw - 1, y + ch - 1), str(n),
                       font=_cnt_font, fill=(0, 0, 0), anchor="rs")
    if augment:
        if rng.random() < 0.3 * strength * 2:
            im = im.rotate(rng.uniform(-1.0, 1.0) * strength * 2,
                           resample=Image.BILINEAR, fillcolor=bg, expand=False)
        if rng.random() < 0.4:
            im = im.filter(ImageFilter.GaussianBlur(rng.uniform(0.2, 0.6 + 0.4 * strength)))
        if rng.random() < 0.5:
            buf = io.BytesIO()
            im.save(buf, "JPEG", quality=rng.randint(45, 90))
            im = Image.open(buf).convert("RGB")
    return im


_sv_HAND_MAX = None


def sv_encode_planes(cells, hand, side):
    """(B,81)/(B,14)/(B,) → (B,43,9,9) float32。視点反転しない。完全ベクトル化。"""
    global _sv_HAND_MAX
    if _sv_HAND_MAX is None:
        _sv_HAND_MAX = np.array([18, 4, 4, 4, 4, 2, 2] * 2, np.float32)
    cells = np.asarray(cells, np.int64).reshape(-1, 81)
    hand = np.asarray(hand, np.float32).reshape(-1, 14)
    side = np.asarray(side, np.float32).reshape(-1)
    B = cells.shape[0]
    x = np.zeros((B, sv_N_PLANES, 81), np.float32)
    bi = np.repeat(np.arange(B), 81)
    ci = cells.ravel()
    occ = ci > 0
    x[bi[occ], ci[occ] - 1, np.tile(np.arange(81), B)[occ]] = 1.0
    x[:, 28:42, :] = np.clip(hand / _sv_HAND_MAX, 0.0, 2.0)[:, :, None]
    x[:, 42, :] = side[:, None]
    return x.reshape(B, sv_N_PLANES, 9, 9)


# -- 方策ラベル: 盤上手 = (from*81+to)*2+promo (13122) / 打 = 13122 + type*81 + to --
# (Pyfamate sv_N_MOVE_LABELS / sv_label_to_usi の逐語移植。policy_head 512x13689 と同一空間)
sv_N_MOVE_LABELS = 81 * 81 * 2 + 7 * 81   # 13689


def sv_label_to_usi(lab: int) -> str:
    if lab >= 81 * 81 * 2:
        lab -= 81 * 81 * 2
        t, to = divmod(lab, 81)
        f, r = 9 - to % 9, to // 9 + 1
        return "%s*%d%s" % (sv_PC_SFEN[sv_HAND_ORDER[t]], f, chr(ord("a") + r - 1))
    promo, ft = lab % 2, lab // 2
    frm, to = divmod(ft, 81)
    def _s(i):
        return "%d%s" % (9 - i % 9, chr(ord("a") + i // 9))
    return _s(frm) + _s(to) + ("+" if promo else "")


_sv_FONTS = _sv_find_cjk_font()

# ---------------------------------------------------------------------------
# sidecar 本体
# ---------------------------------------------------------------------------
_sess = None
_in_name = None
_planes = False   # True: 平面モデル(arbiter) / False: 画像モデル
_val_idx = 0      # named output "value" の位置 (無ければ 0 = 従来挙動)
_pol_idx = None   # named output "policy" の位置 (無ければ None = policy 非対応)
_POL_TOPK = 64    # 送出上限 (softmax 質量の実質全量。再正規化はしない)
_POL_PMIN = 1e-4  # これ未満の確率は送らない


def _log(m):
    sys.stderr.write("cyfamate_dl: " + m + "\n"); sys.stderr.flush()


def _load(path):
    global _sess, _in_name, _planes, sv_IMG, _val_idx, _pol_idx
    import onnxruntime as ort
    _sess = ort.InferenceSession(
        path, providers=["CUDAExecutionProvider", "CPUExecutionProvider"])
    inp = _sess.get_inputs()[0]
    _in_name = inp.name
    shp = inp.shape                                   # [B, C, H, W]
    ch = int(shp[1]) if isinstance(shp[1], int) else 3
    if ch == sv_N_PLANES and (len(shp) < 4 or (isinstance(shp[2], int) and shp[2] == 9)):
        _planes = True
        _log("plane model (arbiter系): 43x9x9")
    else:
        _planes = False
        if len(shp) >= 4 and isinstance(shp[2], int):
            sv_IMG = int(shp[2])                       # 例 144
        _log("image model: %dch %dx%d (sv_IMG=%d)" % (ch, shp[-2], shp[-1], sv_IMG))
    # named outputs (arbiter.onnx: value/gp/policy/sharp)。無名/単出力モデルは従来挙動。
    _out_names = [o.name for o in _sess.get_outputs()]
    _val_idx = _out_names.index("value") if "value" in _out_names else 0
    _pol_idx = _out_names.index("policy") if "policy" in _out_names else None
    _log("outputs=%s value_idx=%d policy=%s"
         % (_out_names, _val_idx, "none" if _pol_idx is None else _pol_idx))


def _policy_pairs(praw):
    """raw logit (13689,) → "usi:prob" 文字列 list (top-K, prob 降順)。
    Pyfamate _compute_trunk と同一: 全次元 max-shift softmax。再正規化しない。"""
    praw = np.asarray(praw, np.float32).reshape(-1)
    if praw.shape[0] != sv_N_MOVE_LABELS:
        return []
    e = np.exp(praw - float(praw.max()))
    pol = e / (float(e.sum()) + 1e-9)
    k = min(_POL_TOPK, pol.shape[0])
    top = np.argpartition(pol, -k)[-k:]
    top = top[np.argsort(pol[top])[::-1]]
    out = []
    for lab in top:
        p = float(pol[lab])
        if p < _POL_PMIN:
            break
        try:
            out.append("%s:%.6f" % (sv_label_to_usi(int(lab)), p))
        except Exception:
            continue                                  # 不正ラベルは送らない
    return out


def _sfen_infer(sfen, want_policy):
    """sfen → (wr|None, ["usi:prob",...])。wr は手番側勝率。失敗は (None, [])。"""
    if _sess is None:
        return None, []
    try:
        sb = SvBoard(); sb.set_sfen(sfen)
        if _planes:
            x = sv_encode_planes(np.asarray(sb.cells, np.int64)[None],
                                 np.asarray(sb.hand, np.float32).reshape(1, 14),
                                 np.asarray([sb.side], np.float32))
        else:
            im = sv_render_board(sb, random.Random(0), augment=False, last_to=sb.last_to)
            x = sv_img_to_np(np.asarray(im, np.uint8))[None]
        outs = _sess.run(None, {_in_name: x})
        lg = max(-60.0, min(60.0, float(np.asarray(outs[_val_idx]).reshape(-1)[0])))
        wr = 1.0 / (1.0 + math.exp(-lg))              # 手番側勝率。★flip しない
        pairs = (_policy_pairs(outs[_pol_idx])
                 if (want_policy and _pol_idx is not None) else [])
        return wr, pairs
    except Exception as e:  # noqa: BLE001
        _log("infer error: " + repr(e)); return None, []


def main():
    model = sys.argv[1] if len(sys.argv) > 1 else "model.onnx"
    try:
        _load(model)
    except Exception as e:  # noqa: BLE001
        _log("model load failed (%r): VALUE は値なしで縮退" % e)

    sys.stdout.write("READY v2\n"); sys.stdout.flush()

    for raw in sys.stdin:
        line = raw.rstrip("\n").rstrip("\r")
        if not line:
            continue
        sp = line.split(" ", 2)
        cmd = sp[0]
        if cmd == "BATCHHINT":
            continue
        elif cmd == "POLICY":
            rid = sp[1] if len(sp) > 1 else "0"
            sfen = sp[2] if len(sp) > 2 else ""
            if _pol_idx is None:
                sys.stdout.write("POL " + rid + "\n")  # policy ヘッド無し → 即・空 (従来挙動)
            else:
                _, pairs = _sfen_infer(sfen, want_policy=True)
                sys.stdout.write("POL " + rid + ("".join(" " + t for t in pairs)) + "\n")
        elif cmd == "VALUE":
            rid = sp[1] if len(sp) > 1 else "0"
            sfen = sp[2] if len(sp) > 2 else ""
            wr, pairs = _sfen_infer(sfen, want_policy=True)
            if wr is None:
                sys.stdout.write("VAL %s\n" % rid)
            else:
                sys.stdout.write("VAL %s %.6f%s\n"
                                 % (rid, wr, "".join(" " + t for t in pairs)))
        elif cmd == "sfen":
            if _pol_idx is None:
                sys.stdout.write("OK\n")              # 旧プロトコル: 空 policy
            else:
                _, pairs = _sfen_infer(sp[1] + (" " + sp[2] if len(sp) > 2 else "")
                                       if len(sp) > 1 else "", want_policy=True)
                sys.stdout.write("OK" + ("".join(" " + t for t in pairs)) + "\n")
        else:
            continue
        sys.stdout.flush()


if __name__ == "__main__":
    main()
)CYFDL";



inline std::string cyf_dl_materialize_script(){
	const char* td = getenv("TEMP"); if (!td || !*td) td = getenv("TMP");
#if !defined(_WIN32)
	if (!td || !*td) td = "/tmp";
#endif
	if (!td || !*td) td = ".";
	std::string path = std::string(td) + "/cyfamate_dl_embedded.py";
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return std::string();
	const char* src = CYF_DL_EMBEDDED_SRC;
	size_t n = sizeof(CYF_DL_EMBEDDED_SRC) - 1;
	if (n && src[0] == '\n'){ ++src; --n; }           
	size_t w = fwrite(src, 1, n, f);
	fclose(f);
	return (w == n) ? path : std::string();
}
















struct CyfOrtEnv; struct CyfOrtStatus; struct CyfOrtSession; struct CyfOrtSessionOptions;
struct CyfOrtValue; struct CyfOrtTypeInfo; struct CyfOrtTensorTypeAndShapeInfo;
struct CyfOrtMemoryInfo; struct CyfOrtAllocator; struct CyfOrtRunOptions;
struct CyfOrtIoBinding; struct CyfOrtCUDAProviderOptionsV2; struct CyfOrtTensorRTProviderOptionsV2;   
#if defined(_WIN32)
#define CYF_ORT_CALL __stdcall
#else
#define CYF_ORT_CALL
#endif
typedef void(CYF_ORT_CALL* CyfOrtFn)(void);          
typedef CyfOrtStatus* CyfOrtStatusPtr;
struct CyfOrtApi {
	
	CyfOrtFn CreateStatus; CyfOrtFn GetErrorCode;
	const char*(CYF_ORT_CALL* GetErrorMessage)(const CyfOrtStatus*);
	
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateEnv)(int logging_level, const char* logid, CyfOrtEnv**);
	CyfOrtFn CreateEnvWithCustomLogger; CyfOrtFn EnableTelemetryEvents; CyfOrtFn DisableTelemetryEvents;
#if defined(_WIN32)
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateSession)(const CyfOrtEnv*, const wchar_t* model_path, const CyfOrtSessionOptions*, CyfOrtSession**);
#else
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateSession)(const CyfOrtEnv*, const char* model_path, const CyfOrtSessionOptions*, CyfOrtSession**);
#endif
	CyfOrtFn CreateSessionFromArray;
	CyfOrtStatusPtr(CYF_ORT_CALL* Run)(CyfOrtSession*, const CyfOrtRunOptions*,
		const char* const* input_names, const CyfOrtValue* const* inputs, size_t input_len,
		const char* const* output_names, size_t output_names_len, CyfOrtValue** outputs);
	
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateSessionOptions)(CyfOrtSessionOptions**);
	CyfOrtFn SetOptimizedModelFilePath; CyfOrtFn CloneSessionOptions;
	CyfOrtStatusPtr(CYF_ORT_CALL* SetSessionExecutionMode)(CyfOrtSessionOptions*, int);   
	CyfOrtFn EnableProfiling; CyfOrtFn DisableProfiling; CyfOrtFn EnableMemPattern;
	CyfOrtStatusPtr(CYF_ORT_CALL* DisableMemPattern)(CyfOrtSessionOptions*);               
	CyfOrtFn EnableCpuMemArena; CyfOrtFn DisableCpuMemArena; CyfOrtFn SetSessionLogId;
	CyfOrtFn SetSessionLogVerbosityLevel; CyfOrtFn SetSessionLogSeverityLevel;
	CyfOrtStatusPtr(CYF_ORT_CALL* SetSessionGraphOptimizationLevel)(CyfOrtSessionOptions*, int);
	CyfOrtStatusPtr(CYF_ORT_CALL* SetIntraOpNumThreads)(CyfOrtSessionOptions*, int);
	CyfOrtFn SetInterOpNumThreads; CyfOrtFn CreateCustomOpDomain; CyfOrtFn CustomOpDomain_Add;
	CyfOrtFn AddCustomOpDomain; CyfOrtFn RegisterCustomOpsLibrary;
	
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetInputCount)(const CyfOrtSession*, size_t*);
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetOutputCount)(const CyfOrtSession*, size_t*);
	CyfOrtFn SessionGetOverridableInitializerCount;
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetInputTypeInfo)(const CyfOrtSession*, size_t, CyfOrtTypeInfo**);
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetOutputTypeInfo)(const CyfOrtSession*, size_t, CyfOrtTypeInfo**);   
	CyfOrtFn SessionGetOverridableInitializerTypeInfo;
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetInputName)(const CyfOrtSession*, size_t, CyfOrtAllocator*, char**);
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionGetOutputName)(const CyfOrtSession*, size_t, CyfOrtAllocator*, char**);
	CyfOrtFn SessionGetOverridableInitializerName;
	
	CyfOrtFn CreateRunOptions; CyfOrtFn RunOptionsSetRunLogVerbosityLevel; CyfOrtFn RunOptionsSetRunLogSeverityLevel;
	CyfOrtFn RunOptionsSetRunTag; CyfOrtFn RunOptionsGetRunLogVerbosityLevel; CyfOrtFn RunOptionsGetRunLogSeverityLevel;
	CyfOrtFn RunOptionsGetRunTag; CyfOrtFn RunOptionsSetTerminate; CyfOrtFn RunOptionsUnsetTerminate;
	
	CyfOrtFn CreateTensorAsOrtValue;
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateTensorWithDataAsOrtValue)(const CyfOrtMemoryInfo*,
		void* p_data, size_t p_data_len, const int64_t* shape, size_t shape_len,
		int onnx_element_type, CyfOrtValue**);
	CyfOrtFn IsTensor;
	CyfOrtStatusPtr(CYF_ORT_CALL* GetTensorMutableData)(CyfOrtValue*, void**);
	CyfOrtFn FillStringTensor; CyfOrtFn GetStringTensorDataLength; CyfOrtFn GetStringTensorContent;
	
	CyfOrtStatusPtr(CYF_ORT_CALL* CastTypeInfoToTensorInfo)(const CyfOrtTypeInfo*, const CyfOrtTensorTypeAndShapeInfo**);
	CyfOrtFn GetOnnxTypeFromTypeInfo; CyfOrtFn CreateTensorTypeAndShapeInfo; CyfOrtFn SetTensorElementType;
	CyfOrtFn SetDimensions; CyfOrtStatusPtr(CYF_ORT_CALL* GetTensorElementType)(const CyfOrtTensorTypeAndShapeInfo*, int*);
	CyfOrtStatusPtr(CYF_ORT_CALL* GetDimensionsCount)(const CyfOrtTensorTypeAndShapeInfo*, size_t*);
	CyfOrtStatusPtr(CYF_ORT_CALL* GetDimensions)(const CyfOrtTensorTypeAndShapeInfo*, int64_t*, size_t);
	CyfOrtFn GetSymbolicDimensions; CyfOrtFn GetTensorShapeElementCount; CyfOrtFn GetTensorTypeAndShape;
	CyfOrtFn GetTypeInfo; CyfOrtFn GetValueType;
	
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateMemoryInfo)(const char* name, int allocator_type, int id, int mem_type, CyfOrtMemoryInfo**);   
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateCpuMemoryInfo)(int allocator_type, int mem_type, CyfOrtMemoryInfo**);
	CyfOrtFn CompareMemoryInfo; CyfOrtFn MemoryInfoGetName; CyfOrtFn MemoryInfoGetId;
	CyfOrtFn MemoryInfoGetMemType; CyfOrtFn MemoryInfoGetType;
	CyfOrtStatusPtr(CYF_ORT_CALL* AllocatorAlloc)(CyfOrtAllocator*, size_t, void**);                 
	CyfOrtStatusPtr(CYF_ORT_CALL* AllocatorFree)(CyfOrtAllocator*, void*);
	CyfOrtFn AllocatorGetInfo;
	CyfOrtStatusPtr(CYF_ORT_CALL* GetAllocatorWithDefaultOptions)(CyfOrtAllocator**);
	
	CyfOrtFn AddFreeDimensionOverride; CyfOrtFn GetValue; CyfOrtFn GetValueCount; CyfOrtFn CreateValue;
	CyfOrtFn CreateOpaqueValue; CyfOrtFn GetOpaqueValue; CyfOrtFn KernelInfoGetAttribute_float;
	CyfOrtFn KernelInfoGetAttribute_int64; CyfOrtFn KernelInfoGetAttribute_string;
	CyfOrtFn KernelContext_GetInputCount; CyfOrtFn KernelContext_GetOutputCount;
	CyfOrtFn KernelContext_GetInput; CyfOrtFn KernelContext_GetOutput;
	
	void(CYF_ORT_CALL* ReleaseEnv)(CyfOrtEnv*);
	void(CYF_ORT_CALL* ReleaseStatus)(CyfOrtStatus*);
	void(CYF_ORT_CALL* ReleaseMemoryInfo)(CyfOrtMemoryInfo*);
	void(CYF_ORT_CALL* ReleaseSession)(CyfOrtSession*);
	void(CYF_ORT_CALL* ReleaseValue)(CyfOrtValue*);
	void(CYF_ORT_CALL* ReleaseRunOptions)(CyfOrtRunOptions*);
	void(CYF_ORT_CALL* ReleaseTypeInfo)(CyfOrtTypeInfo*);
	void(CYF_ORT_CALL* ReleaseTensorTypeAndShapeInfo)(CyfOrtTensorTypeAndShapeInfo*);
	void(CYF_ORT_CALL* ReleaseSessionOptions)(CyfOrtSessionOptions*);
	
	
	
	CyfOrtFn _pad101[29];                                                                            
	CyfOrtStatusPtr(CYF_ORT_CALL* AddSessionConfigEntry)(CyfOrtSessionOptions*, const char*, const char*); 
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateAllocator)(const CyfOrtSession*, const CyfOrtMemoryInfo*, CyfOrtAllocator**); 
	void(CYF_ORT_CALL* ReleaseAllocator)(CyfOrtAllocator*);                                          
	CyfOrtStatusPtr(CYF_ORT_CALL* RunWithBinding)(CyfOrtSession*, const CyfOrtRunOptions*, const CyfOrtIoBinding*); 
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateIoBinding)(CyfOrtSession*, CyfOrtIoBinding**);               
	void(CYF_ORT_CALL* ReleaseIoBinding)(CyfOrtIoBinding*);                                          
	CyfOrtStatusPtr(CYF_ORT_CALL* BindInput)(CyfOrtIoBinding*, const char*, const CyfOrtValue*);     
	CyfOrtStatusPtr(CYF_ORT_CALL* BindOutput)(CyfOrtIoBinding*, const char*, const CyfOrtValue*);    
	CyfOrtStatusPtr(CYF_ORT_CALL* BindOutputToDevice)(CyfOrtIoBinding*, const char*, const CyfOrtMemoryInfo*); 
	CyfOrtFn _pad139[2];                                                                             
	void(CYF_ORT_CALL* ClearBoundInputs)(CyfOrtIoBinding*);                                          
	void(CYF_ORT_CALL* ClearBoundOutputs)(CyfOrtIoBinding*);                                         
	CyfOrtFn _pad143[27];                                                                            
	
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionOptionsAppendExecutionProvider_TensorRT_V2)(CyfOrtSessionOptions*, const CyfOrtTensorRTProviderOptionsV2*); 
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateTensorRTProviderOptions)(CyfOrtTensorRTProviderOptionsV2**);  
	CyfOrtStatusPtr(CYF_ORT_CALL* UpdateTensorRTProviderOptions)(CyfOrtTensorRTProviderOptionsV2*, const char* const*, const char* const*, size_t); 
	CyfOrtFn _pad173;                                                                                
	void(CYF_ORT_CALL* ReleaseTensorRTProviderOptions)(CyfOrtTensorRTProviderOptionsV2*);            
	CyfOrtFn _pad175[27];                                                                            
	CyfOrtStatusPtr(CYF_ORT_CALL* SynchronizeBoundInputs)(CyfOrtIoBinding*);                         
	CyfOrtStatusPtr(CYF_ORT_CALL* SynchronizeBoundOutputs)(CyfOrtIoBinding*);                        
	CyfOrtStatusPtr(CYF_ORT_CALL* SessionOptionsAppendExecutionProvider_CUDA_V2)(CyfOrtSessionOptions*, const CyfOrtCUDAProviderOptionsV2*); 
	CyfOrtStatusPtr(CYF_ORT_CALL* CreateCUDAProviderOptions)(CyfOrtCUDAProviderOptionsV2**);         
	CyfOrtStatusPtr(CYF_ORT_CALL* UpdateCUDAProviderOptions)(CyfOrtCUDAProviderOptionsV2*, const char* const*, const char* const*, size_t); 
	CyfOrtFn _pad207;                                                                                
	void(CYF_ORT_CALL* ReleaseCUDAProviderOptions)(CyfOrtCUDAProviderOptionsV2*);                    
};
static_assert(offsetof(CyfOrtApi, ReleaseSessionOptions)  == 100*sizeof(void*), "CyfOrtApi slot100");
static_assert(offsetof(CyfOrtApi, AddSessionConfigEntry)  == 130*sizeof(void*), "CyfOrtApi slot130");
static_assert(offsetof(CyfOrtApi, RunWithBinding)         == 133*sizeof(void*), "CyfOrtApi slot133");
static_assert(offsetof(CyfOrtApi, SessionOptionsAppendExecutionProvider_TensorRT_V2) == 170*sizeof(void*), "CyfOrtApi slot170");
static_assert(offsetof(CyfOrtApi, ReleaseTensorRTProviderOptions) == 174*sizeof(void*), "CyfOrtApi slot174");
static_assert(offsetof(CyfOrtApi, SynchronizeBoundInputs) == 202*sizeof(void*), "CyfOrtApi slot202");
static_assert(offsetof(CyfOrtApi, SessionOptionsAppendExecutionProvider_CUDA_V2) == 204*sizeof(void*), "CyfOrtApi slot204");
static_assert(sizeof(CyfOrtApi) == 209*sizeof(void*), "CyfOrtApi prefix size");
struct CyfOrtApiBase {
	const CyfOrtApi*(CYF_ORT_CALL* GetApi)(uint32_t version);
	const char*(CYF_ORT_CALL* GetVersionString)(void);
};


inline std::string cyf_dl_label_to_usi(int lab){
	static const char HANDPC[8] = {'P','L','N','S','G','B','R',0};   
	char buf[8];
	if (lab >= 81*81*2){
		lab -= 81*81*2;
		int t = lab / 81, to = lab % 81;
		snprintf(buf, sizeof(buf), "%c*%d%c", HANDPC[t], 9 - to % 9, (char)('a' + to / 9));
		return buf;
	}
	int promo = lab & 1, ft = lab >> 1, frm = ft / 81, to = ft % 81;
	snprintf(buf, sizeof(buf), "%d%c%d%c%s", 9 - frm % 9, (char)('a' + frm / 9),
	         9 - to % 9, (char)('a' + to / 9), promo ? "+" : "");
	return buf;
}




inline bool cyf_dl_encode_planes(const std::string& sfen_in, float* x){
	std::string s = sfen_in;
	{	
		size_t p = 0;
		auto tok = [&](const char* w){ size_t l=strlen(w);
			return s.compare(p, l, w)==0 && (s.size()==p+l || s[p+l]==' '); };
		if (tok("position")) p += 9;
		while (p < s.size() && s[p]==' ') ++p;
		if (tok("sfen")) { p += 5; while (p < s.size() && s[p]==' ') ++p; }
		if (tok("startpos"))
			s = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
		else s = s.substr(p);
	}
	
	std::string board, turn = "b", hands = "-";
	{	size_t a = s.find(' ');
		board = s.substr(0, a);
		if (a != std::string::npos){
			size_t b2 = s.find(' ', a+1);
			turn = s.substr(a+1, b2==std::string::npos ? std::string::npos : b2-a-1);
			if (b2 != std::string::npos){
				size_t c2 = s.find(' ', b2+1);
				hands = s.substr(b2+1, c2==std::string::npos ? std::string::npos : c2-b2-1);
			}
		}
	}
	auto base_idx = [](char u)->int{ switch(u){ case 'P':return 0; case 'L':return 1;
		case 'N':return 2; case 'S':return 3; case 'G':return 4; case 'B':return 5;
		case 'R':return 6; case 'K':return 7; default: return -1; } };
	int r = 1, f = 9;
	for (size_t i = 0; i < board.size(); ++i){
		char c = board[i];
		if (c == '/'){ ++r; f = 9; continue; }
		if (c >= '1' && c <= '9'){ f -= (c - '0'); continue; }
		bool prom = false;
		if (c == '+'){ prom = true; c = board[++i]; }
		int owner = (c >= 'a' && c <= 'z') ? 1 : 0;
		int bi = base_idx((char)toupper((unsigned char)c));
		if (bi < 0 || r > 9 || f < 1) return false;
		
		
		static const int PROM_IDX[8] = {8, 9, 10, 11, -1, 12, 13, -1};
		int idx = prom ? PROM_IDX[bi] : bi;
		if (idx < 0) return false;                    
		int sq = (r - 1) * 9 + (9 - f);
		x[(idx + owner * 14) * 81 + sq] = 1.0f;
		--f;
	}
	static const float HMAX[7] = {18.f, 4.f, 4.f, 4.f, 4.f, 2.f, 2.f};
	auto hand_idx = [](char u)->int{ switch(u){ case 'P':return 0; case 'L':return 1;
		case 'N':return 2; case 'S':return 3; case 'G':return 4; case 'B':return 5;
		case 'R':return 6; default: return -1; } };
	if (hands != "-"){
		int n = 0;
		for (char c : hands){
			if (c >= '0' && c <= '9'){ n = n * 10 + (c - '0'); continue; }
			int hi = hand_idx((char)toupper((unsigned char)c));
			if (hi < 0) return false;
			int owner = (c >= 'a' && c <= 'z') ? 1 : 0;
			float v = (float)(n ? n : 1) / HMAX[hi];
			if (v > 2.0f) v = 2.0f;
			float* pl = x + (28 + owner * 7 + hi) * 81;
			for (int q = 0; q < 81; ++q) pl[q] = v;
			n = 0;
		}
	}
	if (turn == "w"){ float* pl = x + 42 * 81; for (int q = 0; q < 81; ++q) pl[q] = 1.0f; }
	return true;
}

struct CyfOrtNative {
	const CyfOrtApi* api = nullptr;
	CyfOrtEnv* env = nullptr;
	CyfOrtSession* sess = nullptr;
	CyfOrtMemoryInfo* meminfo = nullptr;
	std::string in_name;
	std::vector<std::string> out_names;              
	int pol_pos = -1;                                
	bool ok = false;
	std::mutex mtx;                                  

	
	
	
	
	
	
	
	CyfOrtIoBinding* bind = nullptr;
	CyfOrtValue*     v_in = nullptr;
	CyfOrtValue*     v_out[2] = {nullptr, nullptr};
	float*           h_in  = nullptr;                
	float*           h_out[2] = {nullptr, nullptr};  
	size_t           out_elems[2] = {0, 0};
	int64_t          out_shape[2][8] = {{0}};        
	size_t           out_rank[2] = {0, 0};
	bool             bound  = false;                 
	bool             graphs = false;                 
	std::string      model_path;                     
	
	void*            d_in = nullptr; void* d_out[2] = {nullptr, nullptr};
	CyfOrtAllocator* gpu_alloc = nullptr; CyfOrtMemoryInfo* mi_cuda = nullptr;
	typedef int (*CudaMemcpyFn)(void*, const void*, size_t, int);   
	CudaMemcpyFn     cuda_memcpy = nullptr;
	bool             graphs_req = false;             
	void*            legacy_cuda_fn = nullptr;       
	int              cuda_dev = 0;
	std::thread             eval_th;                 
	std::atomic<bool>       eval_run{false};
	std::mutex              ev_m; std::condition_variable ev_cv;
	bool job_ready = false, job_done = false, job_ok = false;

	
	void eval_loop(){
		std::unique_lock<std::mutex> lk(ev_m);
		while (eval_run.load(std::memory_order_relaxed)){
			ev_cv.wait(lk, [&]{ return job_ready || !eval_run.load(std::memory_order_relaxed); });
			if (!eval_run.load(std::memory_order_relaxed)) break;
			job_ready = false;
			bool okj = true;
			okj = okj && cuda_memcpy(d_in, h_in, 43*81*sizeof(float), 1) == 0;
			if (okj){ CyfOrtStatus* st = api->SynchronizeBoundInputs(bind); if (st){ api->ReleaseStatus(st); } }
			if (okj){ CyfOrtStatus* st = api->RunWithBinding(sess, nullptr, bind); if (st){ okj=false; api->ReleaseStatus(st); } }
			for (int i = 0; okj && i < 2; ++i)
				if (h_out[i] && d_out[i])
					okj = cuda_memcpy(h_out[i], d_out[i], out_elems[i]*sizeof(float), 2) == 0;
			job_ok = okj; job_done = true;
			ev_cv.notify_all();
		}
	}
	
	bool run_graphs_job(){
		std::unique_lock<std::mutex> lk(ev_m);
		job_done = false; job_ready = true;
		ev_cv.notify_all();
		ev_cv.wait(lk, [&]{ return job_done; });
		return job_ok;
	}

	
	size_t out_elems_of(size_t i){
		CyfOrtTypeInfo* ti = nullptr;
		if (api->SessionGetOutputTypeInfo(sess, i, &ti)){ return 0; }
		const CyfOrtTensorTypeAndShapeInfo* tsi = nullptr;
		size_t n = 0; int64_t d[8] = {0}; size_t nd = 0;
		if (!api->CastTypeInfoToTensorInfo(ti, &tsi) && tsi
		    && !api->GetDimensionsCount(tsi, &nd) && nd >= 1 && nd <= 8
		    && !api->GetDimensions(tsi, d, nd)){
			n = 1;
			for (size_t k = 0; k < nd; ++k){
				if (d[k] <= 0) d[k] = 1;                      
				n *= (size_t)d[k];
				out_shape[i][k] = d[k];
			}
			out_rank[i] = nd;
		}
		api->ReleaseTypeInfo(ti);
		return n;
	}

	
	bool setup_binding(bool want_graphs){
		
		for (size_t i = 0; i < out_names.size(); ++i){
			out_elems[i] = out_elems_of(i);
			if (out_elems[i] == 0) return false;
		}
		if (pol_pos == 1 && out_elems[1] != 13689) return false;   
		h_in = (float*)calloc(43*81, sizeof(float));
		for (size_t i = 0; i < out_names.size(); ++i) h_out[i] = (float*)calloc(out_elems[i], sizeof(float));
		if (api->CreateIoBinding(sess, &bind)) return false;
		const int64_t ishape[4] = {1, 43, 9, 9};
		if (want_graphs){
			
			if (api->CreateMemoryInfo("Cuda", 1 , 0, 0 , &mi_cuda)) return false;
			if (api->CreateAllocator(sess, mi_cuda, &gpu_alloc)) return false;
			if (api->AllocatorAlloc(gpu_alloc, 43*81*sizeof(float), &d_in)) return false;
			if (api->CreateTensorWithDataAsOrtValue(mi_cuda, d_in, 43*81*sizeof(float), ishape, 4, 1, &v_in)) return false;
			if (api->BindInput(bind, in_name.c_str(), v_in)) return false;
			for (size_t i = 0; i < out_names.size(); ++i){
				if (api->AllocatorAlloc(gpu_alloc, out_elems[i]*sizeof(float), &d_out[i])) return false;
				if (api->CreateTensorWithDataAsOrtValue(mi_cuda, d_out[i], out_elems[i]*sizeof(float), out_shape[i], out_rank[i], 1, &v_out[i])) return false;
				if (api->BindOutput(bind, out_names[i].c_str(), v_out[i])) return false;
			}
			
			static const char* cudarts[] = {"cudart64_12.dll","cudart64_13.dll","cudart64_110.dll","cudart64_11.dll","cudart64_101.dll","cudart64_10.dll"};
#if defined(_WIN32)
			for (const char* nm : cudarts){
				if (HMODULE hc = GetModuleHandleA(nm)){
					cuda_memcpy = (CudaMemcpyFn)(void*)GetProcAddress(hc, "cudaMemcpy");
					if (cuda_memcpy) break;
				}
			}
#endif
			if (!cuda_memcpy) return false;
		} else {
			if (api->CreateTensorWithDataAsOrtValue(meminfo, h_in, 43*81*sizeof(float), ishape, 4, 1, &v_in)) return false;
			if (api->BindInput(bind, in_name.c_str(), v_in)) return false;
			for (size_t i = 0; i < out_names.size(); ++i){
				if (api->CreateTensorWithDataAsOrtValue(meminfo, h_out[i], out_elems[i]*sizeof(float), out_shape[i], out_rank[i], 1, &v_out[i])) return false;
				if (api->BindOutput(bind, out_names[i].c_str(), v_out[i])) return false;
			}
		}
		return true;
	}

	
	void teardown_binding(){
		if (bind){ api->ReleaseIoBinding(bind); bind = nullptr; }
		for (int i = 0; i < 2; ++i){ if (v_out[i]){ api->ReleaseValue(v_out[i]); v_out[i] = nullptr; } }
		if (v_in){ api->ReleaseValue(v_in); v_in = nullptr; }
		for (int i = 0; i < 2; ++i){ if (d_out[i] && gpu_alloc){ api->AllocatorFree(gpu_alloc, d_out[i]); } d_out[i] = nullptr; }
		if (d_in && gpu_alloc){ api->AllocatorFree(gpu_alloc, d_in); d_in = nullptr; }
		if (gpu_alloc){ api->ReleaseAllocator(gpu_alloc); gpu_alloc = nullptr; }
		if (mi_cuda){ api->ReleaseMemoryInfo(mi_cuda); mi_cuda = nullptr; }
		if (h_in){ free(h_in); h_in = nullptr; }
		for (int i = 0; i < 2; ++i){ if (h_out[i]){ free(h_out[i]); h_out[i] = nullptr; } }
		cuda_memcpy = nullptr; bound = false; graphs = false;
	}

	bool chk(CyfOrtStatus* st, const char* where){
		if (!st) return true;
		fprintf(stderr, "cyfamate: ORT %s: %s\n", where, api ? api->GetErrorMessage(st) : "?");
		if (api) api->ReleaseStatus(st);
		return false;
	}

	
	bool reinit_plain(){
#if defined(_WIN32)
		CyfOrtSessionOptions* so = nullptr;
		if (!chk(api->CreateSessionOptions(&so), "CreateSessionOptions2")) return ok = false;
		api->SetIntraOpNumThreads(so, 1);
		api->SetSessionGraphOptimizationLevel(so, 99);
		typedef CyfOrtStatus* (CYF_ORT_CALL *AppendCudaFn)(CyfOrtSessionOptions*, int);
		if (legacy_cuda_fn){ AppendCudaFn ac = (AppendCudaFn)legacy_cuda_fn; chk(ac(so, cuda_dev), "AppendCUDA2"); }
		wchar_t wpath[1024];
		size_t wl = mbstowcs(wpath, model_path.c_str(), 1023);
		if (wl == (size_t)-1){ api->ReleaseSessionOptions(so); return ok = false; }
		wpath[wl] = 0;
		CyfOrtStatus* st = api->CreateSession(env, wpath, so, &sess);
		api->ReleaseSessionOptions(so);
		if (!chk(st, "CreateSession2")) return ok = false;
		if (setup_binding(false)) bound = true;
		else teardown_binding();                       
		fprintf(stderr, "cyfamate: graphs 縮退再ビルド ok (bound=%d)\n", (int)bound);
		return true;
#else
		return ok = false;
#endif
	}

	
	bool init(const std::string& model){
		model_path = model;
		const char* kill = getenv("CYFAMATE_DL_NATIVE");
		if (kill && (kill[0]=='0'||kill[0]=='n'||kill[0]=='N'||kill[0]=='f'||kill[0]=='F')) return false;
#if defined(_WIN32)
		
		
		
		const char* dllp = getenv("CYFAMATE_ORT_DLL");
		HMODULE h = nullptr;
		if (dllp && *dllp) h = LoadLibraryExA(dllp, nullptr, 0x00000008 );
		if (!h) h = LoadLibraryA("onnxruntime.dll");
		if (!h){
			FILE* pp = _popen("python -c \"import onnxruntime,os,sys;"
				"sys.stdout.write(os.path.join(os.path.dirname(onnxruntime.__file__),'capi','onnxruntime.dll'))\" 2>NUL", "r");
			if (pp){
				char pbuf[1024];
				size_t n2 = fread(pbuf, 1, sizeof(pbuf) - 1, pp);
				_pclose(pp);
				pbuf[n2] = 0;
				if (n2 > 4) h = LoadLibraryExA(pbuf, nullptr, 0x00000008);
				if (h) fprintf(stderr, "cyfamate: onnxruntime.dll @ %s (pip 自動発見)\n", pbuf);
			}
		}
		if (!h) return false;
		auto getbase = (const CyfOrtApiBase*(CYF_ORT_CALL*)(void))(void*)GetProcAddress(h, "OrtGetApiBase");
#else
		return false;   
#endif
#if defined(_WIN32)
		if (!getbase) return false;
		api = getbase()->GetApi(16);                  
		if (!api){ fprintf(stderr, "cyfamate: ORT dll が API v16 未満 → sidecar 縮退\n"); return false; }
		if (!chk(api->CreateEnv(2 , "cyfamate", &env), "CreateEnv")) return false;
		CyfOrtSessionOptions* so = nullptr;
		if (!chk(api->CreateSessionOptions(&so), "CreateSessionOptions")) return false;
		api->SetIntraOpNumThreads(so, 1);             
		api->SetSessionGraphOptimizationLevel(so, 99 );
		
		
		
		
		
		{
			const char* uc = getenv("CYFAMATE_ORT_CUDA");
			const char* ug = getenv("CYFAMATE_ORT_CUDA_GRAPHS");
			const bool cuda_on   = uc && (uc[0]=='1'||uc[0]=='y'||uc[0]=='Y'||uc[0]=='t'||uc[0]=='T');
			graphs_req           = ug && (ug[0]=='1'||ug[0]=='y'||ug[0]=='Y'||ug[0]=='t'||ug[0]=='T');
			const char* dv = getenv("CYFAMATE_ORT_CUDA_DEVICE");
			if (dv && *dv) cuda_dev = atoi(dv);
			typedef CyfOrtStatus* (CYF_ORT_CALL *AppendCudaFn)(CyfOrtSessionOptions*, int);
			legacy_cuda_fn = (void*)GetProcAddress(h, "OrtSessionOptionsAppendExecutionProvider_CUDA");
			if (cuda_on || graphs_req){
				bool appended = false;
				if (graphs_req){
					CyfOrtCUDAProviderOptionsV2* co = nullptr;
					if (chk(api->CreateCUDAProviderOptions(&co), "CreateCUDAOpts") && co){
						char devs[16]; snprintf(devs, sizeof(devs), "%d", cuda_dev);
						const char* keys[2] = {"device_id", "enable_cuda_graph"};
						const char* vals[2] = {devs, "1"};
						appended = chk(api->UpdateCUDAProviderOptions(co, keys, vals, 2), "UpdateCUDAOpts")
						        && chk(api->SessionOptionsAppendExecutionProvider_CUDA_V2(so, co), "AppendCUDA_V2");
						api->ReleaseCUDAProviderOptions(co);
					}
					if (!appended){ graphs_req = false; fprintf(stderr, "cyfamate: CUDA V2/graphs 指定失敗 → legacy へ\n"); }
				}
				if (!appended){
					AppendCudaFn ac = (AppendCudaFn)legacy_cuda_fn;
					if (ac && chk(ac(so, cuda_dev), "AppendCUDA")) appended = true;
				}
				if (appended)
					fprintf(stderr, "cyfamate: native ORT CUDA EP enabled (device=%d%s)\n", cuda_dev, graphs_req ? ", graphs" : "");
				else
					fprintf(stderr, "cyfamate: CUDA EP unavailable -> CPU 続行 (GPU 版 onnxruntime.dll か確認)\n");
			}
		}
		wchar_t wpath[1024];
		size_t wl = mbstowcs(wpath, model.c_str(), 1023);
		if (wl == (size_t)-1){ api->ReleaseSessionOptions(so); return false; }
		wpath[wl] = 0;
		CyfOrtStatus* st = api->CreateSession(env, wpath, so, &sess);
		api->ReleaseSessionOptions(so);
		if (!chk(st, "CreateSession")) return false;
		
		size_t nin = 0;
		if (!chk(api->SessionGetInputCount(sess, &nin), "InCount") || nin < 1) return false;
		CyfOrtTypeInfo* ti = nullptr;
		if (!chk(api->SessionGetInputTypeInfo(sess, 0, &ti), "InTypeInfo")) return false;
		const CyfOrtTensorTypeAndShapeInfo* tsi = nullptr;
		bool shape_ok = false;
		if (chk(api->CastTypeInfoToTensorInfo(ti, &tsi), "CastTensorInfo") && tsi){
			size_t nd = 0; int64_t d[8] = {0};
			if (chk(api->GetDimensionsCount(tsi, &nd), "DimCount") && nd == 4
			    && chk(api->GetDimensions(tsi, d, 4), "Dims"))
				shape_ok = (d[1] == 43 && d[2] == 9 && d[3] == 9);
		}
		api->ReleaseTypeInfo(ti);
		if (!shape_ok){ api->ReleaseSession(sess); sess = nullptr; return false; }
		
		CyfOrtAllocator* al = nullptr;
		if (!chk(api->GetAllocatorWithDefaultOptions(&al), "DefAllocator")) return false;
		char* nm = nullptr;
		if (!chk(api->SessionGetInputName(sess, 0, al, &nm), "InName")) return false;
		in_name = nm; api->AllocatorFree(al, nm);
		size_t nout = 0;
		if (!chk(api->SessionGetOutputCount(sess, &nout), "OutCount")) return false;
		std::string val_name; std::string pol_name;
		for (size_t i = 0; i < nout; ++i){
			if (!chk(api->SessionGetOutputName(sess, i, al, &nm), "OutName")) return false;
			std::string onm = nm; api->AllocatorFree(al, nm);
			if (onm == "value") val_name = onm;
			if (onm == "policy") pol_name = onm;
			if (i == 0 && val_name.empty()) val_name = onm;   
		}
		out_names.clear(); out_names.push_back(val_name);
		pol_pos = -1;
		if (!pol_name.empty()){ out_names.push_back(pol_name); pol_pos = 1; }
		if (!chk(api->CreateCpuMemoryInfo(0 , -1 , &meminfo),
		         "CpuMemInfo")) return false;
		ok = true;
		fprintf(stderr, "cyfamate: native ORT backend ok (plane 43x9x9, outputs=%s%s)\n",
		        val_name.c_str(), pol_name.empty() ? "" : (std::string("+") + pol_name).c_str());
		
		if (graphs_req){
			if (setup_binding(true)){
				eval_run.store(true, std::memory_order_relaxed);
				eval_th = std::thread([this]{ eval_loop(); });
				memset(h_in, 0, 43*81*sizeof(float));          
				std::lock_guard<std::mutex> ilk(mtx);
				if (run_graphs_job()){                          
					bound = true; graphs = true;
					fprintf(stderr, "cyfamate: CUDA Graphs 常駐 ok (capture 済, 専用スレッド)\n");
				}
			}
			if (!graphs){
				if (eval_th.joinable()){
					{ std::lock_guard<std::mutex> l2(ev_m); eval_run.store(false, std::memory_order_relaxed); }
					ev_cv.notify_all(); eval_th.join();
				}
				teardown_binding();
				if (sess){ api->ReleaseSession(sess); sess = nullptr; }
				fprintf(stderr, "cyfamate: CUDA Graphs 失敗 → graphs 抜きで再ビルド\n");
				return reinit_plain();                          
			}
		} else if (setup_binding(false)){
			bound = true;
		} else {
			teardown_binding();                                 
		}
		return true;
#endif
	}

	
	void parse_policy(const float* lgts, std::vector<std::pair<std::string,float>>& pol){
		const int N = 13689;
		float mx = lgts[0];
		for (int i = 1; i < N; ++i) if (lgts[i] > mx) mx = lgts[i];
		double sum = 0.0;
		static thread_local std::vector<float> prob;
		prob.resize(N);
		for (int i = 0; i < N; ++i){ prob[i] = (float)exp((double)lgts[i] - mx); sum += prob[i]; }
		const float inv = (float)(1.0 / (sum + 1e-9));
		static thread_local std::vector<int> idx;   
		idx.resize(N);
		for (int i = 0; i < N; ++i) idx[i] = i;
		const int K = 64;
		std::partial_sort(idx.begin(), idx.begin() + K, idx.end(),
			[&](int a, int b){ return prob[a] > prob[b]; });
		for (int k = 0; k < K; ++k){
			float pv = prob[idx[k]] * inv;
			if (pv < 1e-4f) break;                       
			pol.emplace_back(cyf_dl_label_to_usi(idx[k]), pv);
		}
	}

	
	bool infer(const std::string& sfen, bool want_policy, float& wr, bool& has_value,
	           std::vector<std::pair<std::string,float>>& pol){
		if (!ok) return false;
		std::lock_guard<std::mutex> lk(mtx);
		
		if (bound){
			memset(h_in, 0, 43*81*sizeof(float));
			if (!cyf_dl_encode_planes(sfen, h_in)) return false;
			bool okr;
			if (graphs) okr = run_graphs_job();
			else        okr = chk(api->RunWithBinding(sess, nullptr, bind), "RunWithBinding");
			if (okr){
				double lg = (double)h_out[0][0];
				if (lg > 60.0) lg = 60.0; if (lg < -60.0) lg = -60.0;
				wr = (float)(1.0 / (1.0 + exp(-lg)));    
				has_value = true;
				if (want_policy && pol_pos == 1 && h_out[1])
					parse_policy(h_out[1], pol);
				return true;
			}
			
			if (graphs && eval_th.joinable()){
				{ std::lock_guard<std::mutex> l2(ev_m); eval_run.store(false, std::memory_order_relaxed); }
				ev_cv.notify_all(); eval_th.join();
			}
			teardown_binding();
			fprintf(stderr, "cyfamate: bound 経路失敗 → per-call 経路へ縮退\n");
		}
		float planes[43 * 81] = {0};
		if (!cyf_dl_encode_planes(sfen, planes)) return false;
		int64_t shape[4] = {1, 43, 9, 9};
		CyfOrtValue* in = nullptr;
		if (!chk(api->CreateTensorWithDataAsOrtValue(meminfo, planes, sizeof(planes),
		         shape, 4, 1 , &in), "CreateTensor")) return false;
		const char* innames[1] = { in_name.c_str() };
		const char* outnames[2] = { out_names[0].c_str(),
		                            out_names.size() > 1 ? out_names[1].c_str() : nullptr };
		CyfOrtValue* outs[2] = {nullptr, nullptr};
		const CyfOrtValue* ins[1] = { in };
		CyfOrtStatus* st = api->Run(sess, nullptr, innames, ins, 1,
		                            outnames, out_names.size(), outs);
		api->ReleaseValue(in);
		if (!chk(st, "Run")) return false;
		bool good = false;
		void* p = nullptr;
		if (chk(api->GetTensorMutableData(outs[0], &p), "TensorData(value)") && p){
			double lg = (double)*(float*)p;
			if (lg > 60.0) lg = 60.0; if (lg < -60.0) lg = -60.0;
			wr = (float)(1.0 / (1.0 + exp(-lg)));     
			has_value = true; good = true;
		}
		if (good && want_policy && pol_pos == 1 && outs[1]){
			void* pp = nullptr;
			if (chk(api->GetTensorMutableData(outs[1], &pp), "TensorData(policy)") && pp)
				parse_policy((const float*)pp, pol);         
		}
		for (int i = 0; i < 2; ++i) if (outs[i]) api->ReleaseValue(outs[i]);
		return good;
	}
};







struct DlSidecar {
	CyfOrtNative native;   
	
	struct Result {
		std::vector<std::pair<std::string,float>> policy;
		float value     = -1.0f;   
		bool  has_value = false;
		bool  ok        = false;
	};

	bool ok        = false;   
	bool server_v2 = false;   

	
	
	
	
	
	struct NNCache {
		
		
		struct CEnt {
			std::vector<std::pair<uint16_t,uint16_t>> pol;   
			float    value = -1.0f;
			uint16_t psum  = 0;                              
			bool     has_value = false;
			bool     ok = false;
		};
		
		static const std::unordered_map<std::string,int>& usi2label(){
			static const std::unordered_map<std::string,int> m = []{
				std::unordered_map<std::string,int> t; t.reserve(13689 * 2);
				for (int i = 0; i < 13689; ++i) t.emplace(cyf_dl_label_to_usi(i), i);
				return t;
			}();
			return m;
		}
		static void pack(const Result& r, CEnt& e){
			e.ok = r.ok; e.value = r.value; e.has_value = r.has_value;
			e.pol.clear(); e.pol.reserve(r.policy.size()); e.psum = 0;
			auto& u2l = usi2label();
			for (auto& pr : r.policy){
				auto it = u2l.find(pr.first);
				if (it == u2l.end()) continue;               
				float p = pr.second; if (p < 0.f) p = 0.f; if (p > 1.f) p = 1.f;
				uint16_t p16 = (uint16_t)(p * 65535.0f + 0.5f);
				e.pol.emplace_back((uint16_t)it->second, p16);
				e.psum = (uint16_t)(e.psum + (p16 >> 4));
			}
		}
		static bool unpack(const CEnt& e, Result& r){
			uint16_t s = 0;
			r.policy.clear(); r.policy.reserve(e.pol.size());
			for (auto& pr : e.pol){
				if (pr.first >= 13689) return false;         
				s = (uint16_t)(s + (pr.second >> 4));
				r.policy.emplace_back(cyf_dl_label_to_usi((int)pr.first), (float)pr.second / 65535.0f);
			}
			if (s != e.psum) return false;                   
			r.ok = e.ok; r.value = e.value; r.has_value = e.has_value;
			return true;
		}
		std::mutex mtx;
		std::unordered_map<std::string, CEnt> cur, prev;
		uint64_t hits = 0, misses = 0;
		static size_t cap_entries(){
			int mb = options().nncache_mb;
			return mb <= 0 ? 0 : (size_t)mb * (1024*1024 / 384);   
		}
		
		
		
		static std::string key_of(const std::string& sfen){
			size_t e = sfen.size();
			while (e && sfen[e-1] >= '0' && sfen[e-1] <= '9') --e;
			while (e && sfen[e-1] == ' ') --e;
			return sfen.substr(0, e);
		}
		
		bool get(const std::string& sfen_in, bool need_value, Result& out){
			if (cap_entries() == 0) return false;
			const std::string sfen = key_of(sfen_in);
			std::lock_guard<std::mutex> lk(mtx);
			auto hit = [&](const CEnt& e)->bool{
				if (!e.ok) return false;
				if (need_value && !e.has_value) return false;
				if (!need_value && e.pol.empty()) return false;
				if (!unpack(e, out)) return false;           
				++hits; return true;
			};
			auto it = cur.find(sfen);
			if (it != cur.end() && hit(it->second)) return true;
			auto ip = prev.find(sfen);
			if (ip != prev.end() && hit(ip->second)){ cur[sfen] = ip->second; return true; }  
			++misses; return false;
		}
		void put(const std::string& sfen_in, const Result& r){
			if (cap_entries() == 0 || !r.ok) return;
			const std::string sfen = key_of(sfen_in);
			std::lock_guard<std::mutex> lk(mtx);
			if (cur.size() >= cap_entries()){ prev = std::move(cur); cur.clear(); }
			auto& slot = cur[sfen];
			
			if (!slot.ok || (r.has_value && !slot.has_value) || slot.pol.empty())
				pack(r, slot);
		}
	};
	NNCache nncache;
	std::unordered_map<uint32_t, std::string> pending_sfen;   

	
#if defined(_WIN32)
	HANDLE h_to_child   = INVALID_HANDLE_VALUE;
	HANDLE h_from_child = INVALID_HANDLE_VALUE;
	HANDLE h_process    = INVALID_HANDLE_VALUE;
#else
	FILE* to_child   = nullptr;
	FILE* from_child = nullptr;
	pid_t pid        = -1;
#endif

	
	
	
	
	
	std::thread             reader;
	std::atomic<bool>       reader_run{false};
	std::atomic<uint32_t>   next_id{1};
	std::mutex              w_mtx;         
	std::mutex              io_mtx;        
	std::condition_variable cv;
	std::unordered_map<uint32_t, Result> ready;        
	
	std::unordered_set<uint32_t> abandoned;             
	std::atomic<bool>       async_active{false};        

	
	
	bool read_line_raw(std::string& line){
		line.clear();
#if defined(_WIN32)
		char ch; DWORD nr;
		while (ReadFile(h_from_child,&ch,1,&nr,nullptr) && nr==1){
			if (ch=='\n') return true;
			line.push_back(ch);
		}
		return false;   
#else
		int c;
		while ((c=fgetc(from_child))!=EOF){
			if (c=='\n') return true;
			line.push_back((char)c);
		}
		return false;   
#endif
	}
	
	bool write_raw(const std::string& s){
#if defined(_WIN32)
		if (h_to_child==INVALID_HANDLE_VALUE) return false;
		DWORD nw;
		if (!WriteFile(h_to_child, s.c_str(), (DWORD)s.size(), &nw, nullptr)) return false;
		FlushFileBuffers(h_to_child);
		return true;
#else
		if (!to_child) return false;
		if (fwrite(s.data(),1,s.size(),to_child)!=s.size()) return false;
		fflush(to_child);
		return true;
#endif
	}

	
	static bool next_token(const std::string& s, size_t& i, std::string& tok){
		while (i<s.size() && s[i]==' ') ++i;
		if (i>=s.size()) return false;
		size_t j=s.find(' ', i); if (j==std::string::npos) j=s.size();
		tok.assign(s, i, j-i); i=j+1; return true;
	}
	static void parse_pairs(const std::string& s, size_t i,
	                        std::vector<std::pair<std::string,float>>& out){
		std::string tok;
		while (next_token(s,i,tok)){
			size_t k=tok.find(':');
			if (k!=std::string::npos)
				out.emplace_back(tok.substr(0,k), (float)atof(tok.c_str()+k+1));
		}
	}

	
	void reader_loop(){
		std::string line;
		while (reader_run.load(std::memory_order_relaxed)){
			if (!read_line_raw(line)){
				
				std::lock_guard<std::mutex> lk(io_mtx);
				pending_sfen.clear();
				async_active = false;
				cv.notify_all();
				return;
			}
			
			
			size_t i=0; std::string head;
			if (!next_token(line,i,head)) continue;
			if (head=="POL" || head=="VAL"){
				std::string sid; if (!next_token(line,i,sid)) continue;
				uint32_t id=(uint32_t)strtoul(sid.c_str(),nullptr,10);
				Result r; r.ok=true;
				if (head=="VAL"){
					std::string wr;
					if (next_token(line,i,wr)){ r.value=(float)atof(wr.c_str()); r.has_value=true; }
				}
				parse_pairs(line,i,r.policy);
				std::lock_guard<std::mutex> lk(io_mtx);
				std::string psfen;
				{ auto ps=pending_sfen.find(id); if (ps!=pending_sfen.end()){ psfen=std::move(ps->second); pending_sfen.erase(ps); } }
				if (!psfen.empty()) nncache.put(psfen, r);  
				if (abandoned.erase(id)) continue;          
				ready[id]=std::move(r); cv.notify_all();
			}
			
		}
	}

	
	
	
	bool ready_from_cache(uint32_t id, const std::string& sfen, bool need_value){
		Result c;
		if (!nncache.get(sfen, need_value, c)) return false;
		std::lock_guard<std::mutex> lk(io_mtx);
		ready[id]=std::move(c); cv.notify_all(); return true;
	}
	uint32_t post_policy(const std::string& sfen){
		uint32_t id = next_id.fetch_add(1, std::memory_order_relaxed);
		if (ready_from_cache(id, sfen, false)) return id;   
		if (native.ok){                                      
			Result r; float wr=0; bool hv=false;
			r.ok = native.infer(sfen, true, wr, hv, r.policy);
			if (r.ok && hv){ r.value = wr; r.has_value = true; }   
			nncache.put(sfen, r);
			std::lock_guard<std::mutex> lk(io_mtx);
			ready[id]=std::move(r); cv.notify_all(); return id;
		}
		std::lock_guard<std::mutex> wlk(w_mtx);              
		std::string msg = "POLICY " + std::to_string(id) + " " + sfen + "\n";   
		{ std::lock_guard<std::mutex> lk(io_mtx); pending_sfen[id]=sfen; }      
		if (!write_raw(msg)){                                
			std::lock_guard<std::mutex> lk(io_mtx);
			pending_sfen.erase(id);
			Result r; r.ok=false; ready[id]=std::move(r); cv.notify_all();
		}
		return id;
	}
	uint32_t post_value(const std::string& sfen){
		uint32_t id = next_id.fetch_add(1, std::memory_order_relaxed);
		if (ready_from_cache(id, sfen, true)) return id;    
		if (native.ok){                                      
			Result r; float wr=0; bool hv=false;
			r.ok = native.infer(sfen, true, wr, hv, r.policy);
			if (r.ok && hv){ r.value = wr; r.has_value = true; }
			nncache.put(sfen, r);
			std::lock_guard<std::mutex> lk(io_mtx);
			ready[id]=std::move(r); cv.notify_all(); return id;
		}
		std::lock_guard<std::mutex> wlk(w_mtx);
		std::string msg = "VALUE " + std::to_string(id) + " " + sfen + "\n";   
		{ std::lock_guard<std::mutex> lk(io_mtx); pending_sfen[id]=sfen; }         
		if (!write_raw(msg)){
			std::lock_guard<std::mutex> lk(io_mtx);
			pending_sfen.erase(id);
			Result r; r.ok=false; ready[id]=std::move(r); cv.notify_all();
		}
		return id;
	}
	
	void batch_hint(int n){
		if (native.ok) return;                               
		std::lock_guard<std::mutex> wlk(w_mtx);
		write_raw("BATCHHINT " + std::to_string(n) + "\n");
	}
	
	bool try_get(uint32_t id, Result& out){
		std::lock_guard<std::mutex> lk(io_mtx);
		auto it=ready.find(id); if (it==ready.end()) return false;
		out=std::move(it->second); ready.erase(it); return true;
	}
	
	void abandon(uint32_t id){
		std::lock_guard<std::mutex> lk(io_mtx);
		ready.erase(id); abandoned.insert(id);
		
		
	}
	
	bool wait(uint32_t id, Result& out, int timeout_ms){
		std::unique_lock<std::mutex> lk(io_mtx);
		auto pred=[&]{ return ready.find(id)!=ready.end() || !async_active; };
		if (timeout_ms<0) cv.wait(lk, pred);
		else              cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), pred);
		auto it=ready.find(id);
		if (it==ready.end()){
			if (async_active) abandoned.insert(id);   
			return false;
		}
		out=std::move(it->second); ready.erase(it); return true;
	}

	
	bool start(const std::string& script, const std::string& model){
		
		
		
		if (native.init(model)){
			ok = true; server_v2 = true; async_active = true;
			return true;
		}
#if defined(_WIN32)
		SECURITY_ATTRIBUTES sa{}; sa.nLength=sizeof(sa); sa.bInheritHandle=TRUE;
		HANDLE rd_in,wr_in,rd_out,wr_out;
		if (!CreatePipe(&rd_in, &wr_in, &sa, 0)) return false;
		if (!CreatePipe(&rd_out,&wr_out,&sa, 0)){ CloseHandle(rd_in); CloseHandle(wr_in); return false; }
		SetHandleInformation(wr_in, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(rd_out,HANDLE_FLAG_INHERIT, 0);
		STARTUPINFOA si{}; si.cb=sizeof(si);
		si.dwFlags=STARTF_USESTDHANDLES; si.hStdInput=rd_in; si.hStdOutput=wr_out; si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
		PROCESS_INFORMATION pi{};
		std::string cmd = "python \"" + script + "\" \"" + model + "\"";
		if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)){
			CloseHandle(rd_in); CloseHandle(wr_in); CloseHandle(rd_out); CloseHandle(wr_out); return false;
		}
		CloseHandle(pi.hThread); CloseHandle(rd_in); CloseHandle(wr_out);
		h_to_child=wr_in; h_from_child=rd_out; h_process=pi.hProcess;
#else
		int in_pipe[2], out_pipe[2];
		if (pipe(in_pipe) != 0) return false;
		if (pipe(out_pipe) != 0){ close(in_pipe[0]); close(in_pipe[1]); return false; }
		pid = fork();
		if (pid < 0){ return false; }
		if (pid == 0){
			dup2(in_pipe[0], 0);
			dup2(out_pipe[1], 1);
			close(in_pipe[0]); close(in_pipe[1]);
			close(out_pipe[0]); close(out_pipe[1]);
			execlp("python3","python3", script.c_str(), model.c_str(), (char*)nullptr);
			_exit(127);
		}
		close(in_pipe[0]); close(out_pipe[1]);
		to_child   = fdopen(in_pipe[1], "w");
		from_child = fdopen(out_pipe[0], "r");
		if (!to_child || !from_child){ close_child(); return false; }
#endif
		
		std::string line;
		if (!read_line_raw(line)){ ok=false; close_child(); return false; }
		ok = (line.rfind("READY",0)==0);
		if (!ok){ close_child(); return false; }
		server_v2 = (line.find("v2")!=std::string::npos) || (line.find("V2")!=std::string::npos);
		
		
		if (!server_v2){
			fprintf(stderr, "cyfamate: sidecar が v2 でない (READY のみ) → 拒否。旧プロトコルは廃止済み\n");
			ok = false; close_child(); return false;
		}
		reader_run.store(true, std::memory_order_relaxed);
		async_active = true;
		reader = std::thread([this]{ reader_loop(); });
		return ok;
	}

	
	void close_child(){
#if defined(_WIN32)
		if (h_to_child!=INVALID_HANDLE_VALUE){ CloseHandle(h_to_child); h_to_child=INVALID_HANDLE_VALUE; }
		if (h_process!=INVALID_HANDLE_VALUE){ TerminateProcess(h_process,0); CloseHandle(h_process); h_process=INVALID_HANDLE_VALUE; }
		if (h_from_child!=INVALID_HANDLE_VALUE){ CloseHandle(h_from_child); h_from_child=INVALID_HANDLE_VALUE; }
#else
		if (to_child){ fclose(to_child); to_child=nullptr; }
		if (pid>0){ kill(pid, SIGKILL); while (waitpid(pid, nullptr, 0)<0 && errno==EINTR){} pid=-1; }
		if (from_child){ fclose(from_child); from_child=nullptr; }
#endif
	}

	
	
	
	void stop(){
		if (reader.joinable()){
			reader_run.store(false, std::memory_order_relaxed);
#if defined(_WIN32)
			if (h_to_child!=INVALID_HANDLE_VALUE){ CloseHandle(h_to_child); h_to_child=INVALID_HANDLE_VALUE; }
			if (h_process!=INVALID_HANDLE_VALUE){ TerminateProcess(h_process,0); }
#else
			if (to_child){ fclose(to_child); to_child=nullptr; }
			if (pid>0){ kill(pid, SIGKILL); }
#endif
			reader.join();
		}
		close_child();
	}
	~DlSidecar(){ stop(); }
};
inline DlSidecar& sidecar(){ static DlSidecar s; return s; }
inline bool& sidecar_tried(){ static bool t=false; return t; }

inline bool& bench_guard(){ static bool b=false; return b; }


inline std::vector<std::pair<std::string,float>>& last_dl_policy(){ static std::vector<std::pair<std::string,float>> v; return v; }
inline double& last_gp(){ static double g=0.0; return g; }
inline bool&   last_dl_valid(){ static bool b=false; return b; }


inline double& last_dl_entropy(){ static double e=0.5; return e; }
inline bool&   last_dl_entropy_valid(){ static bool b=false; return b; }








inline double& last_momentum(){ static double m=0.0; return m; }          
inline double& last_sharpness(){ static double s=0.5; return s; }         
inline int&    last_tesuki_n(){ static int n=-1; return n; }              
inline int&    prev_best_cp(){ static int c=0; return c; }
inline bool&   prev_best_cp_valid(){ static bool b=false; return b; }
inline double& prev_gp(){ static double g=-1.0; return g; }
inline int&    calm_streak(){ static int s=0; return s; }                 
inline std::string& prev_pred_usi(){ static std::string s; return s; }    
inline int&    csm_last_ply(){ static int p=-1; return p; }




inline double compute_sharpness(int best_cp, bool dl_ok, double dl_policy_max, bool agree, bool cp_ok = true){
	return CyfSharp::observe(best_cp, cp_ok, dl_ok, dl_policy_max, agree).scalar;
}


inline double compute_momentum(int best_cp, bool cp_ok, double gp, double gp_prev, int streak, int tesuki_n){
	auto cl = [](double v){ return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v); };
	const double score_trend = cp_ok ? cl((best_cp - prev_best_cp()) / 200.0) : 0.0;
	const double gp_push     = gp_prev >= 0.0 ? cl((gp - gp_prev) * 5.0) : 0.0;
	const double streak_calm = cl((streak - 2.5) / 2.5);
	double base = 0.45 * score_trend + 0.35 * gp_push + 0.20 * streak_calm;
	if (tesuki_n >= 0 && tesuki_n <= 2)   
		base -= std::max(0.0, 1.0 - tesuki_n / 3.0);
	return base < -1.0 ? -1.0 : (base > 1.0 ? 1.0 : base);
}



inline std::string last_dl_argmax(float& conf){
	conf = -1.f; std::string top;
	for (auto& pr : last_dl_policy()) if (pr.second > conf){ conf = pr.second; top = pr.first; }
	return top;
}





inline double current_sharpness(const std::string& root_best_usi, int best_cp, bool cp_ok){
    float pmax;
    const std::string top = last_dl_argmax(pmax);
    auto& sh = CyfSharp::state();
    const bool dl_ok = last_dl_valid() && sh.policy_current() && !top.empty() && !root_best_usi.empty();
    sh.current = CyfSharp::observe(best_cp, cp_ok, dl_ok, pmax, top == root_best_usi);
    ++sh.tm_uses;
    return sh.current.scalar;
}
inline double tm_dl_scale(const std::string& root_best_usi, int best_cp, bool mate_range, bool ponder_mode, bool cp_ok = true){
	Options& o = options();
	double s = 1.0;
	if (!o.enable || bench_guard()) return s;
	
	if (last_dl_entropy_valid())
		s *= 0.85 + 0.30 * last_dl_entropy();
	
	if (o.tm_enable && last_dl_valid() && !root_best_usi.empty()){
		float dbp; const std::string dtop = last_dl_argmax(dbp);
		const double dls = dl_policy_scale(last_gp(), o.gp_onset, o.gp_floor);
		if (!dtop.empty() && dls > 0.0){
			const double f = (root_best_usi == dtop) ? o.tm_agree / 100.0
			               : (dbp >= (float)o.dl_conf_min ? o.tm_disagree / 100.0 : 1.0);
			s *= 1.0 + (f - 1.0) * dls;
		}
	}
	
	
	
	
	{
		const double gpv = last_gp();
		const double e = o.tm_gp_early / 100.0, m = o.tm_gp_mid / 100.0;
		const double g = std::max(0.05, o.tm_gp_gamma / 100.0);
		if      (gpv <= 0.5) s *= e + (m - e) * std::pow(gpv / 0.5, g);
		else if (gpv <  1.0) s *= m + (1.0 - m) * std::pow((gpv - 0.5) / 0.5, g);
	}
	
	{
		const int acp = best_cp < 0 ? -best_cp : best_cp;
		const int start = o.tm_score_start_cp;
		if (!mate_range && acp > start){
			const double fl = o.tm_score_floor / 100.0;
			double t = (double)(acp - start) / (double)start;
			if (t > 1.0) t = 1.0;
			s *= 1.0 + (fl - 1.0) * t;
		}
	}
	
	
	{
		const double mm = last_momentum();
		if      (mm < 0.0) s *= 1.0 + (-mm) * (o.tm_momentum_ext / 100.0);
		else if (mm > 0.0) s *= 1.0 -   mm  * (o.tm_momentum_acc / 100.0);
	}
	
	
	const double sharp = o.tm_sharp_current
        ? current_sharpness(root_best_usi, best_cp, cp_ok && !mate_range) : last_sharpness();
    s *= (o.tm_sharp_base + o.tm_sharp_range * sharp) / 100.0;
	
	
	
	
	
	
	if (s < CYF_TM_FLOOR) s = CYF_TM_FLOOR;
	else if (s > CYF_TM_CEIL) s = CYF_TM_CEIL;
	return s;
}



inline void post_move_metrics(int ply, int best_cp, bool cp_ok, bool dl_ok,
                              double dl_gap_p, bool agree, int tesuki_n,
                              const std::string& best_usi, const std::string& pred_usi){
	const double gp = last_gp();
	
	if (csm_last_ply() >= 0 && (ply <= csm_last_ply() || ply - csm_last_ply() > 4)){
		calm_streak() = 0; prev_best_cp_valid() = false; prev_gp() = -1.0; prev_pred_usi().clear();
	}
	
	
	if (!prev_pred_usi().empty() && prev_pred_usi() == best_usi) ++calm_streak();
	else calm_streak() = 0;
	prev_pred_usi() = pred_usi;
	last_tesuki_n()  = tesuki_n;
	last_sharpness() = compute_sharpness(best_cp, dl_ok, dl_gap_p, agree, cp_ok);
	last_momentum()  = compute_momentum(best_cp, cp_ok && prev_best_cp_valid(), gp, prev_gp(),
	                                    calm_streak(), tesuki_n);
	if (cp_ok){ prev_best_cp() = best_cp; prev_best_cp_valid() = true; }
	prev_gp() = gp;
	csm_last_ply() = ply;
}








inline std::atomic<int64_t>& zyf_action_deadline(){ static std::atomic<int64_t> d{0}; return d; }
inline std::atomic<bool>& zyf_cancelled(){ static std::atomic<bool> c{false}; return c; }
inline std::atomic<int64_t>& zyf_ponderhit_ms(){ static std::atomic<int64_t> m{0}; return m; }   
inline std::atomic<int>& zyf_verify_quota(){ static std::atomic<int> q{0}; return q; }
inline bool zyf_action_live(int64_t expiry=0){
    const auto now = CyfZyf::now_us(), hard = zyf_action_deadline().load();
    return !zyf_cancelled().load() && (!hard || now < hard) && (!expiry || now < expiry);
}
struct MateTable {
	static constexpr int      BITS = 16;
	static constexpr size_t   SIZE = size_t(1) << BITS;
	static constexpr uint32_t MASK = uint32_t(SIZE - 1);
	struct Entry { std::atomic<uint32_t> seq{0}; std::atomic<uint64_t> key{0}; std::atomic<int16_t> len{0}; std::atomic<uint8_t> state{0}; std::atomic<uint64_t> gen{0}; std::atomic<int64_t> expiry{0}; std::atomic<uint16_t> move{0}; };
	std::vector<Entry> tbl;
	uint64_t gen = 0;
	bool    active = false;
	
	
	
	
	
	std::vector<std::atomic<uint64_t>> hint_bits;
	std::atomic<uint32_t> hint_count{0};
	std::mutex hint_mx;   
	MateTable(): tbl(SIZE), hint_bits(SIZE / 64) {}
	void begin(){   
		++gen; active = true;
		if (!options().zyf_tree_share || hint_count.load(std::memory_order_relaxed) > SIZE / 2) clear_hints(); }
	void disable(){ active = false; }
	
	void store(uint64_t k, uint8_t st, int16_t ln, int64_t expiry){
		
		
		if (st != 1) return;
		Entry& e = tbl[(uint32_t)k & MASK];
		const uint32_t s = e.seq.load(std::memory_order_relaxed) + 1;
		e.seq.store(s, std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_release);
		e.key = k; e.state = st; e.len = ln; e.gen = gen; e.move = 0;
        e.expiry = expiry;   
		std::atomic_thread_fence(std::memory_order_release);
		e.seq.store(s + 1, std::memory_order_relaxed);
	}
	
	
	
	void store_hint(uint64_t k, uint16_t mv, int64_t expiry){
		if (!mv) return;
		std::lock_guard<std::mutex> lk(hint_mx);   
		const uint32_t i = (uint32_t)k & MASK;
		Entry& e = tbl[i];
		const uint32_t s = e.seq.load(std::memory_order_relaxed) + 1;
		e.seq.store(s, std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_release);
		e.key = k; e.state = 1; e.len = 0; e.gen = gen; e.move = mv;
		e.expiry = expiry;
		std::atomic_thread_fence(std::memory_order_release);
		e.seq.store(s + 1, std::memory_order_relaxed);
		if (!(hint_bits[i >> 6].fetch_or(uint64_t(1) << (i & 63), std::memory_order_relaxed) >> (i & 63) & 1))
			hint_count.fetch_add(1, std::memory_order_relaxed);
	}
	
	uint16_t load_hint(uint64_t k) const {
		if (!active) return 0;
		const uint32_t i = (uint32_t)k & MASK;
		if (!(hint_bits[i >> 6].load(std::memory_order_relaxed) >> (i & 63) & 1)) return 0;
		const Entry& e = tbl[i];
		const uint32_t s1 = e.seq.load(std::memory_order_relaxed);
		if (s1 & 1u) return 0;
		std::atomic_thread_fence(std::memory_order_acquire);
		const uint64_t kk = e.key; const uint16_t mv = e.move;
		std::atomic_thread_fence(std::memory_order_acquire);
		if (e.seq.load(std::memory_order_relaxed) != s1 || kk != k) return 0;
		return mv;
	}
	
	void clear_hints(){
		if (!hint_count.load(std::memory_order_relaxed)) return;
		for (auto& w : hint_bits) w.store(0, std::memory_order_relaxed);
		hint_count.store(0, std::memory_order_relaxed);
	}
	
	bool load(uint64_t k, uint8_t& st, int16_t& ln, int64_t* until=nullptr) const {
		if (!active) return false;
		const Entry& e = tbl[(uint32_t)k & MASK];
		const uint32_t s1 = e.seq.load(std::memory_order_relaxed);
		if (s1 & 1u) return false;
		std::atomic_thread_fence(std::memory_order_acquire);
		const uint64_t kk = e.key, gg = e.gen; const uint8_t ss = e.state; const int16_t ll = e.len;
        const int64_t expiry = e.expiry;
		std::atomic_thread_fence(std::memory_order_acquire);
		const uint32_t s2 = e.seq.load(std::memory_order_relaxed);
		if (s1 != s2 || kk != k || gg != gen || ss == 0 || !zyf_action_live(expiry)) return false;
		st = ss; ln = ll; if (until) *until=expiry; return true;
	}
};




inline MateTable mate_table_instance;
inline MateTable& mate_table(){ return mate_table_instance; }

























struct HarvestProbe {
#pragma pack(push, 1)
	struct Ev {
		uint64_t key;    
		uint32_t t_us;   
		uint16_t rd;     
		uint8_t  d;      
		uint8_t  th;     
        uint16_t move;   
        uint8_t kind;    
        uint8_t flags;   
        int16_t baseline, actual;
	};
#pragma pack(pop)
	static_assert(sizeof(Ev) == 24, "HarvestProbe v2 event is 24 bytes");

	std::vector<Ev>     ev;
	std::atomic<size_t> n{0};
	std::chrono::steady_clock::time_point t0;
	int         minDepth = 0;      
	bool        on       = false;
	std::string outPath;

	
	void begin(int X, size_t cap){
		on = false;                       
		minDepth = X;
		n.store(0, std::memory_order_relaxed);
		if (X <= 0 || cap == 0) return;
		if (ev.size() != cap) ev.assign(cap, Ev{});
		n.store(0, std::memory_order_relaxed);
		minDepth = X;
		if (outPath.empty()){
			const char* p = getenv("CYFAMATE_PROBE_OUT");
			outPath = (p && *p) ? p : "cyf_probe.bin";
		}
		t0 = std::chrono::steady_clock::now();
		on = true;
	}
	void disable(){ on = false; }

	
	inline void log(uint64_t k, int d, int rd, size_t th, uint16_t move=0, uint8_t kind=0, uint8_t flags=0, int baseline=0, int actual=0){
		if (!on || d < minDepth) return;
		const size_t i = n.fetch_add(1, std::memory_order_relaxed);
		if (i >= ev.size()) return;       
		const auto dt = std::chrono::steady_clock::now() - t0;
		Ev& e  = ev[i];
		e.key  = k;
		e.t_us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(dt).count();
		e.rd   = (uint16_t)(rd < 0 ? 0 : (rd > 65535 ? 65535 : rd));
		e.d    = (uint8_t)(d > 255 ? 255 : (d < 0 ? 0 : d));
		e.th   = (uint8_t)(th > 255 ? 255 : th);
        e.move=move; e.kind=kind; e.flags=flags; e.baseline=int16_t(baseline); e.actual=int16_t(actual);
	}

	
	void dump(){
		if (minDepth <= 0) return;
		const size_t raw = n.load(std::memory_order_relaxed);
		const size_t cnt = raw < ev.size() ? raw : ev.size();
		if (!cnt) return;
		FILE* f = std::fopen(outPath.c_str(), "ab");
		if (f){
			
			
			Ev hdr{};
			hdr.key  = 0xC1FA0B0EC1FA0B02ULL;
			hdr.t_us = (uint32_t)cnt;            
			hdr.rd   = 0xFFFFu;
			hdr.d    = (uint8_t)(minDepth > 255 ? 255 : minDepth);
			hdr.th   = 0xFFu;
            hdr.flags = raw > ev.size() ? 1 : 0; 
			std::fwrite(&hdr, sizeof(Ev), 1, f);
			std::fwrite(ev.data(), sizeof(Ev), cnt, f);
			std::fclose(f);
		}
		sync_cout << "info string Cyfamate probe X=" << minDepth
		          << " events=" << cnt << (raw > ev.size() ? " (overflow)" : "")
		          << " out=" << outPath << sync_endl;
	}
};




inline HarvestProbe harvest_probe_instance;
inline HarvestProbe& harvest_probe(){ return harvest_probe_instance; }



inline bool& parent_info_enabled() { static bool v = false; return v; }
inline std::atomic<uint64_t>& go_received() { static std::atomic<uint64_t> v{0}; return v; }


inline bool& replaying_go() { static bool v = false; return v; }


inline std::atomic<int64_t>& sp_goponder_ms() { static std::atomic<int64_t> v{0}; return v; }
inline std::atomic<int64_t>& sp_credit_ms() { static std::atomic<int64_t> v{0}; return v; }

inline std::atomic<bool>& bestmove_muted() { static std::atomic<bool> v{false}; return v; }



inline std::string sp_go_with_ponderhit_clock(const std::string& go_cmd, std::istream& ph) {
    static const char* const keys[] = {"btime", "wtime", "binc", "winc", "byoyomi"};
    auto is_key = [](const std::string& k) { for (auto* x : keys) if (k == x) return true; return false; };
    auto numeric = [](const std::string& v) {
        return !v.empty() && v.size() <= 18 && std::all_of(v.begin(), v.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    std::vector<std::pair<std::string, std::string>> over;
    std::vector<std::string> ph_tok; std::string t;
    while (ph >> t) ph_tok.push_back(t);
    for (size_t i = 0; i + 1 < ph_tok.size(); ++i)
        if (is_key(ph_tok[i]) && numeric(ph_tok[i + 1])) { over.emplace_back(ph_tok[i], ph_tok[i + 1]); ++i; }
    if (over.empty()) return go_cmd;
    std::istringstream gs(go_cmd); std::vector<std::string> g;
    while (gs >> t) g.push_back(t);
    std::string out; std::vector<bool> used(over.size(), false);
    for (size_t i = 0; i < g.size(); ++i) {
        out += (out.empty() ? "" : " ") + g[i];
        for (size_t j = 0; j < over.size(); ++j)
            if (g[i] == over[j].first && i + 1 < g.size()) { out += " " + over[j].second; used[j] = true; ++i; break; }
    }
    for (size_t j = 0; j < over.size(); ++j) if (!used[j]) out += " " + over[j].first + " " + over[j].second;
    return out;
}


#include "cyf_fusion.h"
#include "cyf_auto_hint_engine.h"   


inline std::string auto_default_file(std::initializer_list<const char*> rels) {
	const std::string dir = fusion_exe_dir();
	for (const char* r : rels) { const std::string p = dir + r; if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p; }
	return "<empty>";
}
inline std::string auto_default_model() { return auto_default_file({"eval\\cnn8m.value.onnx", "runs\\arch\\blend\\cnn8m.value.onnx"}); }
inline std::string auto_default_ort() {
	return auto_default_file({"cyfamate-support\\work\\learning-lab\\.venv\\Lib\\site-packages\\onnxruntime\\capi\\onnxruntime.dll"}); }
inline void apply_auto_defaults() {   
	auto& c = auto_config(); c.model = auto_default_model(); c.ort = auto_default_ort();   
}
#if defined(CYF_REPLAY)
#include "cyf_replay.h"
#endif
#if defined(CYF_GPU_POOL)
#include "cyf_gpu_pool_search.h"
#endif



inline Value exact_nnue(const Position& pos) {
#if defined(CYF_GPU_POOL)
    if (Value v; gpu_pool_search().probe(pos, v)) return v;
#endif
    return Eval::evaluate(pos);
}
#if defined(CYF_BLEND)
#include "cyf_blend.h"
#endif
#if defined(CYF_BLEND)





struct Mode2Link {
	bool enabled = true;
	int mode = 0, batch = BlendConfig{}.batch, min_new = BlendConfig{}.min_new, rows = BlendConfig{}.new_rows_per_go;
	std::string model = "<empty>", ort = "<empty>";   
};
inline Mode2Link& mode2_link() { static Mode2Link l; return l; }
inline bool sh11_fully_on() {
	return auto_config().requested.load() && CyfPreInfer::state().enabled.load() && CyfPreInfer::state().root_ready_requested.load()
	    && CyfRootHint::state().enabled.load() && CyfRootHint::state().require_key.load();
}
inline void apply_mode2_link(size_t search_threads) {
	auto& l = mode2_link(); auto& bc = blend_config(); auto& f = fusion_config(); const BlendConfig d{};
	const bool sh11 = l.enabled && sh11_fully_on();
	const bool engage = l.enabled && l.mode != 1 && sh11 && search_threads == 1;
	const int effective = !l.enabled || l.mode == 1 ? l.mode : (engage ? 2 : 0);
	bc.mode = effective;
	bc.model = engage && l.model == "<empty>" ? auto_config().model : l.model;
	bc.batch = engage && l.batch == d.batch ? 8 : l.batch;
	bc.min_new = engage && l.min_new == d.min_new ? 1 : l.min_new;
	bc.new_rows_per_go = engage && l.rows == d.new_rows_per_go ? 128 : l.rows;
	f.ort_dll = engage && l.ort == "<empty>" ? auto_config().ort : l.ort;
	if (l.enabled && (engage || effective != l.mode || sh11))   
		sync_cout << "info string cyf mode2_link: link=1 sh11=" << sh11 << " search_threads=" << search_threads
		          << " requested_mode=" << l.mode << " effective_mode=" << effective
		          << " reason=" << (l.mode == 1 ? "mode1_unlinked" : !sh11 ? "sh11_off" : search_threads != 1 ? "search_threads" : "sh11_on")
		          << " model=" << bc.model << " batch=" << bc.batch << " min_new=" << bc.min_new
		          << " new_rows_per_go=" << bc.new_rows_per_go << sync_endl;
}
#endif
#if defined(CYF_EVAL_PROFILE)

inline uint64_t& eval_cycles() { static uint64_t c = 0; return c; }
inline uint64_t& eval_calls() { static uint64_t c = 0; return c; }
inline uint64_t& go_tsc0() { static uint64_t c = 0; return c; }
#endif



struct MateRootResult {
	std::atomic<int> state{0};   
	std::atomic<int> len{0};
    std::atomic<int64_t> expiry{0};
	std::atomic<bool> move_valid{false};
	char move_csa[16] = {0};     
	void reset(){ state.store(0); len.store(0); expiry.store(zyf_action_deadline().load()); move_valid.store(false); move_csa[0]=0; }
};
inline MateRootResult& mate_root(){ static MateRootResult r; return r; }
#include "cyf_mate_validate.h"
struct ZyfVerifyStats {   
    std::atomic<uint64_t> dead{0}, nolen{0}, quota{0}, tried{0}, ok{0}, fail{0}, late{0}, use_cut{0}, use_root{0};
    std::atomic<uint64_t> use_root_hint{0};
    void reset(){ for (auto* a : {&dead,&nolen,&quota,&tried,&ok,&fail,&late,&use_cut,&use_root,&use_root_hint}) a->store(0); }
};
inline ZyfVerifyStats& zyf_verify_stats(){ static ZyfVerifyStats s; return s; }
inline int verify_mate_bound(Position& pos, int len, int ply, int max_game_ply,
                             int64_t expiry, const std::atomic<bool>* stop, Move first=Move::none()){
    auto& vs = zyf_verify_stats();
    if (!zyf_action_live(expiry)){ ++vs.dead; return -1; }
    if (len <= 0 || len >= MAX_PLY){ ++vs.nolen; return -1; }
    if (zyf_verify_quota().fetch_sub(1) <= 0){ ++vs.quota; return -1; }
    ++vs.tried;
    NativeMateProof proof;
    proof.remaining = first == Move::none() ? 256 : 2048;
    proof.max_game_ply = max_game_ply; proof.cancelled = stop;
    proof.deadline = CyfZyf::now_us() + (first == Move::none() ? 200 : 1000);
    if (expiry) proof.deadline = std::min(proof.deadline, expiry);
    auto hard = zyf_action_deadline().load(); if (hard) proof.deadline = std::min(proof.deadline, hard);
    const int bound = proof.walk(pos, pos.side_to_move(), std::min(len,15), ply, first);
    if (!zyf_action_live(expiry) || proof.expired()){ ++vs.late; return -1; }
    ++(bound > 0 ? vs.ok : vs.fail);
    return bound;
}



Move csa_to_move(const Position& pos, const std::string& csa_in);
template<class RootMovesT>
inline bool play_zyf_root_hint(Position& p, RootMovesT& moves){
    auto& r=mate_root();
    if (!options().zyf_play_hint || !mate_table().active || r.state.load()!=1
        || !r.move_valid.load() || !zyf_action_live(r.expiry.load())) return false;
    
    
    
    if(!moves.empty() && moves[0].score!=VALUE_NONE && is_win(moves[0].score)) return false;
    const Move m=csa_to_move(p,std::string(r.move_csa));
    if(m==Move::none() || !p.pseudo_legal_s<true>(m) || !p.legal(m)) return false;
    for(size_t i=0;i<moves.size();++i) if(!moves[i].pv.empty() && moves[i].pv[0]==m){
        std::swap(moves[0],moves[i]); ++zyf_verify_stats().use_root_hint; return true;
    }
    return false;
}



inline bool& zyf_hash_latched(){ static bool b=false; return b; }









inline std::atomic<bool>& zyf_armed(){ static std::atomic<bool> b{false}; return b; }
inline bool& zyf_reserved(){ static bool b=false; return b; }      
constexpr double ZYF_GP_ONSET = 0.5;                               

#include "cyf_resources.h"

struct MateWorker {
	std::thread th;
    std::atomic<bool> cancelled{false};
    void run_portfolio(const std::string& sfen,uint64_t key,int count,long nodes,double seconds){
        
        
        const int64_t expiry=zyf_action_deadline().load();
        auto live=[&](){ return !cancelled && !Zyfamate::g_stop && zyf_action_live(expiry); };
        if(!live())return;
        ZyfCpuGuard _cpu(count);   
        std::vector<std::unique_ptr<ZyfProcess>> children;
        const int hash=std::max(16,options().mate_hash_mb/count);
        for(int i=0;i<count && live();++i){
            auto child=std::make_unique<ZyfProcess>();
            if(child->start(sfen,i,hash,nodes,seconds))children.push_back(std::move(child));
        }
        sync_cout<<"info string zyf_portfolio workers="<<children.size()<<" table_mb_each="<<hash<<sync_endl;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(int(seconds*1000)+1000);
        while(!children.empty()&&live() && std::chrono::steady_clock::now()<deadline){
            for(auto it=children.begin();it!=children.end();){
                if(!(*it)->poll()){++it;continue;}
                (*it)->poll();
                const auto at=(*it)->text.find("CYF_ZYF_RESULT ");
                int result=2,len=0;char move[16]{};
                if(at!=std::string::npos)std::sscanf((*it)->text.c_str()+at,"CYF_ZYF_RESULT %d %d %15s",&result,&len,move);
                if(result==1 && move[0] && std::string(move)!="-"){
                    CYF_ZYF_TEST_STAGE(5);
                    if(!live())return;
                    mate_table().store(key,1,int16_t(std::max(0,len)),expiry);
                    std::snprintf(mate_root().move_csa,sizeof(mate_root().move_csa),"%s",move);
                    mate_root().len.store(std::max(0,len));mate_root().move_valid.store(true);
                    mate_root().state.store(1);return;
                }
                
                it=children.erase(it);
            }
            if(!children.empty())std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
	
	
	void run(std::string sfen, uint64_t cyf_key, int nthreads, long nodelimit, double tlimit){
        const int64_t started = CyfZyf::now_us(), expiry = zyf_action_deadline().load();
        auto live = [&](){ return !cancelled && !Zyfamate::g_stop && zyf_action_live(expiry); };
        auto remaining = [&](){
            double secs = std::max(0.0, tlimit - (CyfZyf::now_us()-started)/1e6);
            const auto hard = zyf_action_deadline().load();
            if (hard) secs = std::min(secs, std::max(0.0,(hard-CyfZyf::now_us())/1e6));
            return secs;
        };
        if (!live() || remaining() <= 0) return;
        ZyfCpuGuard _cpu(std::max(1, nthreads));   
        Zyfamate::Pos p;
        if (!Zyfamate::parse_sfen(&p, sfen.c_str())) return;
        long nd = 0;
        const int r = nthreads > 1 ? Zyfamate::solve_parallel(sfen.c_str(),nthreads,nodelimit,remaining(),&nd)
                                  : Zyfamate::solve(&p,nodelimit,remaining(),&nd);
        CYF_ZYF_TEST_STAGE(1);
        if (!live()) return;
        if (r == 1){
            const double proof_budget = remaining();
            
            const int L = -1; 
            if (!live()) return;
            CyfZyf::SolveOut out{CyfZyf::kMate,int16_t(L > 0 ? L : 0),nd};
            if (remaining() > 0){
                Zyfamate::Pos q; Zyfamate::parse_sfen(&q,sfen.c_str()); Zyfamate::g_attacker=q.side;
                int fm=Zyfamate::first_mate_move(&q,1);
                if (fm >= 0) Zyfamate::move_csa(&q,fm,q.side,out.root_move_csa);
                else if (live() && remaining() > 0){
                    char pv[20000]{};
                    
                    int i=0;for(;pv[i] && pv[i]!=' ' && i<15;++i)out.root_move_csa[i]=pv[i];
                    out.root_move_csa[i]=0;
                }
            }
            CYF_ZYF_TEST_STAGE(5);
            if (!live()) return;
            mate_table().store(cyf_key,1,out.len,expiry);
            std::memcpy(mate_root().move_csa,out.root_move_csa,sizeof(out.root_move_csa));
            mate_root().move_valid.store(out.root_move_csa[0] != 0);
            mate_root().len.store(out.len);mate_root().state.store(1);
        } else if (r == 0 && live()) mate_root().state.store(2);
    }
	void launch(const std::string& sfen, uint64_t cyf_key, int nthreads, long nodelimit, double tlimit){
		stop_join();
		cancelled=false;
		Zyfamate::g_stop = 0;
		th = std::thread([this, sfen, cyf_key, nthreads, nodelimit, tlimit]{
            zyf_thread_priority();   
            if(nthreads>1)run_portfolio(sfen,cyf_key,nthreads,nodelimit,tlimit);
            else run(sfen,cyf_key,1,nodelimit,tlimit);
        });
	}
	void stop_join(){ cancelled=true;if (th.joinable()){ Zyfamate::g_stop = 1; th.join(); } }
	~MateWorker(){ stop_join(); }
};
inline MateWorker& mate_worker(){ static MateWorker w; return w; }





struct MponWorker {
	std::thread th;
	void run(std::vector<std::pair<std::string,uint64_t>> jobs, long nodelimit, double tlimit_each){
		zyf_thread_priority();   
		ZyfCpuGuard _cpu;        
		for (auto& jb : jobs){
			if (Zyfamate::g_stop) break;               
			Zyfamate::Pos p;
			if (!Zyfamate::parse_sfen(&p, jb.first.c_str())) continue;
			
			
			
			long nd = 0;
            const auto started = CyfZyf::now_us();
            int r = Zyfamate::solve(&p, nodelimit, tlimit_each, &nd);
            if (!zyf_action_live()) break;
			if (Zyfamate::g_stop) break;               
			if (r == 1){                               
                double left = std::max(0.0,tlimit_each-(CyfZyf::now_us()-started)/1e6);
                const auto hard=zyf_action_deadline().load();
                if(hard)left=std::min(left,std::max(0.0,(hard-CyfZyf::now_us())/1e6));
                
                int L = -1; 
                if (Zyfamate::g_stop || !zyf_action_live()) break;   
				mate_table().store(jb.second, 1, (int16_t)(L > 0 ? L : 0), zyf_action_deadline().load());
				if (options().verbose)
					sync_cout << "info string Cyfamate mpon proven key=" << jb.second
					          << " L=" << (L > 0 ? L : 0) << sync_endl;
			} else if (r == 0){
				
			}
		}
	}
	void launch(std::vector<std::pair<std::string,uint64_t>> jobs, long nodelimit, double tlimit_each){
		stop_join();
		Zyfamate::g_stop = 0;
		th = std::thread([this, jobs, nodelimit, tlimit_each]{ run(jobs, nodelimit, tlimit_each); });
	}
	void stop_join(){ if (th.joinable()){ Zyfamate::g_stop = 1; th.join(); } }
	~MponWorker(){ stop_join(); }
};
inline MponWorker& mpon_worker(){ static MponWorker w; return w; }










inline int  zyf_lanes(){ return active_cpu_budget().mate == 2 && options().zyf_defense ? 2 : 1; }
inline bool zyf_service_active(){ return options().zyf_service && options().mate_enable
                                         && (active_cpu_budget().mate == 1 || zyf_lanes() == 2); }
inline bool zyf_defense_active(){ return zyf_service_active() && zyf_lanes() == 2; }
inline uint64_t& zyf_go_gen(){ static uint64_t g = 0; return g; }      
struct ZyfProbeStats { std::atomic<uint64_t> pv_mate{0}, pv_nomate{0}, shallow_mate{0}, shallow_nomate{0}; };
inline ZyfProbeStats& zyf_probe_stats(){ static ZyfProbeStats s; return s; }
inline std::atomic<bool>& zyf_shallow_probe(){ static std::atomic<bool> b{false}; return b; }
struct ZyfCand {
	uint16_t move = 0; uint64_t key = 0; int nnue_rank = -1, cyf_rank = -1; int32_t cyfnet = 0; bool has_cyfnet = false;
	uint8_t source = CyfZyf::kSrcNnue; double priority = 0.0; int depth = 0;
};
struct ZyfGo {
	bool active = false, armed = false, root_job = false, hint = false;
	uint64_t gen = 0, supply_us = 0, collect_us = 0;
	int supplied = 0, supply_calls = 0, supply_illegal = 0, drained_in_search = 0, replays = 0;
	int best_checks = 0;                       
	int defense_supplied = 0;                  
	std::string tonshi_from, tonshi_to, tonshi_alt;   
	std::vector<uint16_t> cand_sent;           
	long proof_hits0 = 0;
	std::vector<ZyfCand> cands;                
	std::vector<CyfZyf::Result> results;       
};
inline ZyfGo& zyf_go(){ static ZyfGo g; return g; }



inline std::atomic<bool>& zyf_root_safe(){ static std::atomic<bool> b{false}; return b; }







struct ZyfTreeStats { std::atomic<uint64_t> jobs{0}, records{0}, exported{0}, us{0}, hint_hits{0}, hint_new{0}, hint_override{0}, hint_illegal{0}; };
inline ZyfTreeStats& zyf_tree_stats(){ static ZyfTreeStats s; return s; }
inline void zyf_harvest_tree(const std::string& sfen, int max_records, double secs, std::vector<std::pair<uint64_t,uint16_t>>& out){
    constexpr size_t MAXD = 96;
    Zyfamate::Pos zp;
    if (max_records <= 0 || !Zyfamate::parse_sfen(&zp, sfen.c_str())) return;
    Zyfamate::g_attacker = zp.side;
    const int64_t t0 = CyfZyf::now_us(), until = t0 + int64_t(std::max(0.0, secs) * 1e6);
    std::deque<std::vector<int>> q; q.emplace_back();
    std::unordered_set<uint64_t> seen;
    for (const auto& r : out) seen.insert(r.first);
    int added = 0;
    while (!q.empty() && added < max_records && !Zyfamate::g_stop && CyfZyf::now_us() < until){
        std::vector<int> path = std::move(q.front()); q.pop_front();
        for (int m : path) Zyfamate::make_move(&zp, m);
        const uint64_t key = Zyfamate::yo_key(&zp);
        const bool fresh = seen.insert(key).second;
        const int fm = Zyfamate::first_mate_move(&zp, 1);
        if (fm >= 0){
            if (fresh){ out.emplace_back(key, Zyfamate::yo_move16(fm)); ++added; }
            if (path.size() + 2 <= MAXD && q.size() < size_t(max_records) * 4){
                Zyfamate::make_move(&zp, fm);
                int rep[1024]; const int n = Zyfamate::gen_legal(&zp, rep);   
                Zyfamate::unmake_move(&zp);
                for (int i = 0; i < n; ++i){
                    std::vector<int> next(path); next.push_back(fm); next.push_back(rep[i]);
                    q.push_back(std::move(next));
                }
            }
        }
        for (size_t k = path.size(); k-- > 0;) Zyfamate::unmake_move(&zp);
    }
    auto& ts = zyf_tree_stats();
    ts.jobs.fetch_add(1, std::memory_order_relaxed); ts.records.fetch_add(uint64_t(added), std::memory_order_relaxed);
    ts.us.fetch_add(uint64_t(std::max<int64_t>(0, CyfZyf::now_us() - t0)), std::memory_order_relaxed);
}


inline CyfZyf::SolveOut zyf_solve_job(const CyfZyf::Job& j){
    ZyfCpuGuard _cpu;   
    zyf_thread_priority();
    Zyfamate::Pos p;
    if (!Zyfamate::parse_sfen(&p, j.sfen.c_str())) return {CyfZyf::kError, 0, 0};
    
    
    Zyfamate::tt_init(); Zyfamate::twin_init(); Zyfamate::m3_init(); if (Zyfamate::use_superior) Zyfamate::sup_init();
    const int64_t started = CyfZyf::now_us();
    long nd = 0;
    const double solve_secs = j.deadline_us ? std::min(j.secs, (j.deadline_us-started)/1e6) : j.secs;
    if (solve_secs <= 0 || !zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,0};
    
    std::vector<std::pair<uint64_t,uint16_t>> tree;
    const bool share = options().zyf_tree_share;
    if (share){ Zyfamate::g_yexport = &tree; Zyfamate::g_yexport_cap = size_t(std::max(1, options().zyf_tree_max)) * 4; }
    
    
    if (share && options().zyf_tree_live)
        Zyfamate::g_ylive = [](uint64_t k, uint16_t mv){ if (mate_table().active) mate_table().store_hint(k, mv, 0); };
    const int r = Zyfamate::solve(&p, j.nodes, solve_secs, &nd);
    Zyfamate::g_yexport = nullptr; Zyfamate::g_ylive = nullptr;
    if (share) zyf_tree_stats().exported.fetch_add(tree.size(), std::memory_order_relaxed);
    auto with_tree = [&](CyfZyf::SolveOut o){ o.tree = std::move(tree); return o; };
    CYF_ZYF_TEST_STAGE(1);
    auto remaining = [&](){
        double secs = j.secs > 0 ? j.secs - (CyfZyf::now_us()-started)/1e6 : 0.3;
        if (j.deadline_us) secs = std::min(secs, (j.deadline_us-CyfZyf::now_us())/1e6);
        const auto hard = zyf_action_deadline().load();
        if (hard) secs = std::min(secs,(hard-CyfZyf::now_us())/1e6);
        return std::max(0.0, secs);
    };
    if (Zyfamate::g_stop) return {CyfZyf::kCancelled, 0, nd};
    if (!zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,nd};
    if (r == 1){
        
        if (share && remaining() > 0)
            zyf_harvest_tree(j.sfen, options().zyf_tree_max, std::min(remaining(), 0.02), tree);
        
        if (j.kind == CyfZyf::kRoot && options().zyf_play_hint && remaining() > 0){
            Zyfamate::Pos q; Zyfamate::parse_sfen(&q,j.sfen.c_str()); Zyfamate::g_attacker=q.side;
            const int fm=Zyfamate::first_mate_move(&q,1);
            if (fm>=0){
                CyfZyf::SolveOut out{CyfZyf::kMate,0,nd};
                Zyfamate::move_csa(&q,fm,q.side,out.root_move_csa);
                if(Zyfamate::g_stop) return {CyfZyf::kCancelled,0,nd};
                if(!zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,nd};
                out.tree = std::move(tree);
                return out; 
            }
        }
        const double proof_budget = remaining();
        
        const int L = -1; 
        if (Zyfamate::g_stop) return {CyfZyf::kCancelled, 0, nd};
        CyfZyf::SolveOut out{CyfZyf::kMate, int16_t(L > 0 ? L : 0), nd};
        if (j.kind == CyfZyf::kRoot && remaining() > 0){
            Zyfamate::Pos q; Zyfamate::parse_sfen(&q, j.sfen.c_str());
            Zyfamate::g_attacker = q.side;
            const int fm = Zyfamate::first_mate_move(&q,1);
            if (fm >= 0) Zyfamate::move_csa(&q, fm, q.side, out.root_move_csa);
            else if (remaining() > 0){
                char pv[20000]{};
                
                int i = 0; for (; pv[i] && pv[i] != ' ' && i < 15; ++i) out.root_move_csa[i] = pv[i];
                out.root_move_csa[i] = 0;
            }
        }
        if (Zyfamate::g_stop) return {CyfZyf::kCancelled, 0, nd};
        if (!zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,nd};
        out.tree = std::move(tree);
        return out;
    }
    if (r == 0) return with_tree({CyfZyf::kNomate, 0, nd});
    switch (Zyfamate::g_stop_reason){
        case 1: return with_tree({CyfZyf::kUnknownNodes, 0, nd});
        case 2: return with_tree({CyfZyf::kUnknownTime, 0, nd});
        case 3: return {CyfZyf::kCancelled, 0, nd};
        default: return with_tree({CyfZyf::kUnknownOther, 0, nd});
    }
}
inline CyfZyf::Service& zyf_service(){
	static CyfZyf::Service s;
	static const bool configured = [](){
		s.configure(zyf_solve_job,
		            [](const CyfZyf::Job& j, const CyfZyf::SolveOut& o){   
                        CYF_ZYF_TEST_STAGE(5);
                        if (!zyf_action_live(j.deadline_us)) return;
		                
		                if (j.kind == CyfZyf::kRoot && CyfZyf::is_verdict(o.outcome)){   
		                    if (CyfZyf::is_mate(o.outcome)){
		                        mate_root().expiry.store(j.deadline_us);
		                        std::memcpy(mate_root().move_csa, o.root_move_csa, sizeof(o.root_move_csa));
		                        mate_root().move_valid.store(o.root_move_csa[0] != 0);
		                        mate_root().len.store(o.len);
		                        mate_root().state.store(1);
		                    } else mate_root().state.store(2);
		                }
		                if (!mate_table().active) return;
		                const bool mate = CyfZyf::is_mate(o.outcome);
		                if (mate) mate_table().store(j.key, 1, o.len, j.deadline_us);   
		                for (const auto& h : o.tree) mate_table().store_hint(h.first, h.second, j.deadline_us);   
		            },
		            []{ Zyfamate::g_stop = 1; }, []{ Zyfamate::g_stop = 0; },
                    [](const CyfZyf::Job& j){ return zyf_action_live(j.deadline_us); });
		
		s.set_lane_init([](int lane){ Zyfamate::use_context(lane); zyf_thread_priority(); });   
		return true;
	}();
	(void)configured;
	return s;
}



inline bool zyf_same_go(){ return zyf_go().gen != 0 && zyf_go().gen == zyf_go_gen(); }
inline void zyf_go_begin(bool armed){
	auto& g = zyf_go(); auto& s = zyf_service();
	s.start(zyf_lanes());
	s.pause_join();
	Zyfamate::g_keep_proofs = options().zyf_keep_proofs ? 1 : 0;
	if (!zyf_same_go()){
		g = ZyfGo{}; g.gen = zyf_go_gen(); g.proof_hits0 = Zyfamate::g_proof_hits;
		zyf_root_safe().store(false, std::memory_order_relaxed);
		CyfDefense::clear();
		auto& ps = zyf_probe_stats(); ps.pv_mate = 0; ps.pv_nomate = 0; ps.shallow_mate = 0; ps.shallow_nomate = 0;
		auto& ts = zyf_tree_stats(); ts.jobs = 0; ts.records = 0; ts.exported = 0; ts.us = 0; ts.hint_hits = 0; ts.hint_new = 0; ts.hint_override = 0; ts.hint_illegal = 0;
	}
	else ++g.replays;
	g.active = true; g.armed = g.armed || armed;
	zyf_shallow_probe().store(true, std::memory_order_relaxed);
	s.begin_go(g.gen);
}



inline void zyf_job_deadline(CyfZyf::Job& j){
    j.supply_us = CyfZyf::now_us();
    j.deadline_us = zyf_action_deadline().load();
}

inline void zyf_submit_root(const Position& pos, double legacy_tl){
	auto& o = options(); auto& g = zyf_go();
	CyfZyf::Job j; j.key = uint64_t(pos.key()); j.gen = g.gen; j.sfen = pos.sfen(); j.kind = CyfZyf::kRoot; j.source = CyfZyf::kSrcRoot;
	j.stm = pos.side_to_move() == BLACK ? 0 : 1; j.priority = 1e9;
	j.nodes = o.zyf_root_nodes > 0 ? long(o.zyf_root_nodes) : 1000000000L;
	
	
	j.secs = o.zyf_root_ms > 0 ? std::min(legacy_tl, o.zyf_root_ms / 1000.0)
	       : zyf_lanes() == 2 ? legacy_tl
	                          : std::min(legacy_tl, std::min(std::max(1, o.zyf_job_ms) / 1000.0, std::max(0.001, legacy_tl / 12.0)));
	zyf_job_deadline(j);
	if (g.replays) zyf_service().forget(j.key);   
	g.root_job = zyf_service().submit(std::move(j)) || g.root_job;
}
inline std::string zyf_value_str(Value v){
	if (v == -VALUE_INFINITE || v == VALUE_INFINITE || v == VALUE_NONE) return "none";
	if (std::abs(v) < VALUE_MATE_IN_MAX_PLY) return "cp:" + std::to_string(USIEngine::to_cp(v));
	return std::string("mate:") + std::to_string(v > 0 ? int(VALUE_MATE - v) : -int(VALUE_MATE + v));
}




template<class RootMovesT>
inline void zyf_iteration(Position& rootPos, const RootMovesT& rootMoves, int depth){
	auto& g = zyf_go(); auto& s = zyf_service(); auto& o = options();
	const int64_t tc = CyfZyf::now_us();
	const size_t before = g.results.size();
	s.drain(g.results);
	g.drained_in_search += int(g.results.size() - before);
	g.collect_us += uint64_t(CyfZyf::now_us() - tc);
	if (!g.armed || depth < o.zyf_min_depth || rootMoves.empty() || rootMoves[0].pv.empty()) return;
	
	
	const bool defense = zyf_defense_active();
	
	
	
	
	if (defense ? g.defense_supplied < o.zyf_defense_jobs
	            : (o.zyf_tree_share && g.supplied >= o.zyf_go_jobs && g.best_checks < std::max(1, o.zyf_go_jobs))){
		const Move m = rootMoves[0].pv[0];
		const bool known = std::find(g.cand_sent.begin(), g.cand_sent.end(), uint16_t(m.raw())) != g.cand_sent.end();
		if (!known && rootPos.pseudo_legal_s<true>(m) && rootPos.legal(m)){
			ZyfCand c; c.move = uint16_t(m.raw()); c.nnue_rank = 0; c.source = CyfZyf::kSrcNnue; c.depth = depth; c.priority = 3.0;
			CyfZyf::Job j;   
			{ StateInfo cst; rootPos.do_move(m, cst);
			  c.key = j.key = uint64_t(rootPos.key()); j.sfen = rootPos.sfen(); j.stm = rootPos.side_to_move() == BLACK ? 0 : 1;
			  rootPos.undo_move(m); }
			j.gen = g.gen; j.kind = CyfZyf::kCand; j.source = c.source; j.ply = 1; j.root_move = c.move; j.depth = int16_t(depth);
			j.priority = c.priority;
			j.nodes = defense ? std::max(1, o.zyf_defense_nodes) : std::max(1, o.zyf_job_nodes);
			j.secs = (defense ? std::max(1, o.zyf_defense_ms) : std::max(1, o.zyf_job_ms)) / 1000.0;
			zyf_job_deadline(j);
			if (s.submit(std::move(j))){
				if (defense) ++g.defense_supplied; else ++g.best_checks;
				g.cand_sent.push_back(c.move);
				bool listed = false;
				for (const auto& e : g.cands) if (e.move == c.move){ listed = true; break; }
				if (!listed) g.cands.push_back(c);
			}
		}
	}
	if (g.supplied >= o.zyf_go_jobs && !(defense && g.defense_supplied < o.zyf_defense_jobs)) return;
	const int64_t t0 = CyfZyf::now_us();
	++g.supply_calls;
	const Color us = rootPos.side_to_move();
	const Value rs = rootMoves[0].score;
	const int cp = (rs == -VALUE_INFINITE || rs == VALUE_INFINITE) ? 0
	             : std::abs(rs) < VALUE_MATE_IN_MAX_PLY ? USIEngine::to_cp(rs) : (rs > 0 ? 30000 : -30000);
	const auto& hg = CyfRootHint::state().go;   
	g.hint = hg.matched && !hg.scored.empty();
	auto cyf_of = [&](uint16_t raw, int32_t& v, int& rank){
		for (size_t i = 0; i < hg.scored.size(); ++i) if (hg.scored[i].first == raw){ v = hg.scored[i].second; rank = int(i); return true; }
		return false;
	};
	std::vector<CyfZyf::Job> jobs;
	auto make = [&](uint8_t kind, uint8_t source, int ply, uint16_t root_move, double prio){
		CyfZyf::Job j; j.key = uint64_t(rootPos.key()); j.gen = g.gen; j.sfen = rootPos.sfen(); j.kind = kind; j.source = source;
		j.stm = rootPos.side_to_move() == BLACK ? 0 : 1; j.ply = int8_t(ply); j.root_move = root_move; j.depth = int16_t(depth);
		int rk = -1; j.has_cyfnet = cyf_of(root_move, j.cyfnet, rk);
		j.priority = prio; j.nodes = std::max(1, o.zyf_job_nodes); j.secs = std::max(1, o.zyf_job_ms) / 1000.0;
		if (defense && kind == CyfZyf::kCand){ j.nodes = std::max(1, o.zyf_defense_nodes); j.secs = std::max(1, o.zyf_defense_ms) / 1000.0; }
		zyf_job_deadline(j);
		jobs.push_back(std::move(j));
	};
	
	const auto& pv = rootMoves[0].pv;
	const uint16_t best_raw = uint16_t(pv[0].raw());
	const int plies = std::min<int>(std::min<int>(int(pv.size()), std::max(0, o.zyf_pv_plies)), 32);
	StateInfo st[32]; int done = 0;
	for (int k = 0; k < plies; ++k){
		const Move m = pv[size_t(k)];
		if (m == Move::none() || !rootPos.pseudo_legal_s<true>(m) || !rootPos.legal(m)){ ++g.supply_illegal; break; }
		rootPos.do_move(m, st[k]); ++done;
		if (k + 1 >= 2){
			const bool ours = rootPos.side_to_move() == us;
			const double lean = std::clamp((ours ? cp : -cp) / 1500.0, -1.0, 1.0);
			make(CyfZyf::kPv, CyfZyf::kSrcNnuePv, k + 1, best_raw, 2.0 + lean - 0.05 * (k + 1));
		}
	}
	for (int k = done - 1; k >= 0; --k) rootPos.undo_move(pv[size_t(k)]);
	
	std::vector<size_t> idx;                         
	std::vector<uint8_t> src;
	auto add = [&](size_t r, uint8_t source){
		for (size_t i = 0; i < idx.size(); ++i) if (idx[i] == r){ if (src[i] != source) src[i] = CyfZyf::kSrcBoth; return; }
		idx.push_back(r); src.push_back(source);
	};
	const int n = std::max(0, o.zyf_cands);
	for (int i = 0; i < n && size_t(i) < rootMoves.size(); ++i) add(size_t(i), CyfZyf::kSrcNnue);
	if (g.hint)
		for (int i = 0; i < n && size_t(i) < hg.scored.size(); ++i)
			for (size_t r = 0; r < rootMoves.size(); ++r)
				if (uint16_t(rootMoves[r].pv[0].raw()) == hg.scored[size_t(i)].first){ add(r, CyfZyf::kSrcCyfnet); break; }
	int32_t best_cyf = 0; int best_rank = -1;
	const bool best_has = cyf_of(best_raw, best_cyf, best_rank);
	for (size_t i = 0; i < idx.size(); ++i){
		const Move m = rootMoves[idx[i]].pv[0];
		if (!rootPos.pseudo_legal_s<true>(m) || !rootPos.legal(m)){ ++g.supply_illegal; continue; }
		ZyfCand c; c.move = uint16_t(m.raw()); c.nnue_rank = int(idx[i]); c.source = src[i]; c.depth = depth;
		c.has_cyfnet = cyf_of(c.move, c.cyfnet, c.cyf_rank);
		double prio = 1.5 + (idx[i] == 0 ? 0.2 : 0.0);
		if (c.has_cyfnet && best_has && c.cyfnet < best_cyf) prio += 0.4;   
		if (c.source == CyfZyf::kSrcCyfnet) prio += 0.3;                    
		c.priority = prio;
		StateInfo cst; rootPos.do_move(m, cst);
		c.key = uint64_t(rootPos.key());
		make(CyfZyf::kCand, c.source, 1, c.move, prio);
		rootPos.undo_move(m);
		bool known = false;
		for (auto& e : g.cands) if (e.move == c.move){ known = true; break; }
		if (!known) g.cands.push_back(c);
	}
	std::stable_sort(jobs.begin(), jobs.end(), [](const CyfZyf::Job& a, const CyfZyf::Job& b){ return a.priority > b.priority; });
	for (auto& j : jobs){
		const bool cand = j.kind == CyfZyf::kCand; const uint16_t rm = j.root_move;
		if (defense && cand){
			if (g.defense_supplied >= o.zyf_defense_jobs
			    || std::find(g.cand_sent.begin(), g.cand_sent.end(), rm) != g.cand_sent.end()) continue;
			if (s.submit(std::move(j))){ ++g.defense_supplied; g.cand_sent.push_back(rm); }
			continue;
		}
		if (g.supplied >= o.zyf_go_jobs) continue;
		if (s.submit(std::move(j))){ ++g.supplied; if (cand) g.cand_sent.push_back(rm); }
	}
	g.supply_us += uint64_t(CyfZyf::now_us() - t0);
}


template<class RootMovesT>
inline void zyf_report(const RootMovesT& rootMoves, bool muted){
	auto& g = zyf_go();
	if (!g.active) return;
	g.active = false;
	zyf_shallow_probe().store(false, std::memory_order_relaxed);
	auto& s = zyf_service();
	s.drain(g.results);
	if (muted) return;
	const auto c = s.counters();
	auto& ps = zyf_probe_stats();
	std::ostringstream o;
	o << "info string cyfzyf v=1 go gen=" << g.gen << " replays=" << g.replays << " armed=" << int(g.armed) << " root_job=" << int(g.root_job)
	  << " keep_proofs=" << Zyfamate::g_keep_proofs << " hint=" << int(g.hint) << " supplied=" << g.supplied
	  << " best_checks=" << g.best_checks << " lanes=" << s.lanes() << " defense_supplied=" << g.defense_supplied
	  << " lane1_solves=" << c.lane1_solves << " tonshi_switch=" << (g.tonshi_from.empty() ? std::string("-") : g.tonshi_from + ">" + g.tonshi_to + ":" + g.tonshi_alt)
	  << " safe_order_picks=" << CyfDefense::picks().load(std::memory_order_relaxed)
	  << " supply_calls=" << g.supply_calls << " supply_illegal=" << g.supply_illegal << " supply_us=" << g.supply_us
	  << " collect_us=" << g.collect_us << " drained_in_search=" << g.drained_in_search << " submitted=" << c.submitted
	  << " accepted=" << c.accepted << " dup_go=" << c.dup_go << " stale_gen=" << c.stale_gen << " queue_full=" << c.queue_full
	  << " replaced=" << c.replaced << " solves=" << c.solves << " solve_us=" << c.solve_us << " solve_nodes=" << c.solve_nodes
	  << " max_wait_us=" << c.max_wait_us << " log_dropped=" << c.log_dropped;
	for (int k = 1; k < CyfZyf::kOutcomeCount; ++k) o << ' ' << CyfZyf::outcome_name(uint8_t(k)) << '=' << c.outcomes[k];
	o << " probe_pv_mate=" << ps.pv_mate.load() << " probe_pv_nomate=" << ps.pv_nomate.load()
	  << " probe_shallow_mate=" << ps.shallow_mate.load() << " probe_shallow_nomate=" << ps.shallow_nomate.load()
	  << " proof_hits=" << (Zyfamate::g_proof_hits - g.proof_hits0) << " cache=" << s.cache_size() << " solves_total=" << s.solves_total();
	{ auto& ts = zyf_tree_stats();
	  o << " tree_share=" << int(options().zyf_tree_share) << " tree_jobs=" << ts.jobs.load() << " tree_records=" << ts.records.load()
	    << " tree_exported=" << ts.exported.load()
	    << " tree_us=" << ts.us.load() << " tree_slots=" << mate_table().hint_count.load() << " hint_same=" << ts.hint_hits.load()
	    << " hint_new=" << ts.hint_new.load() << " hint_override=" << ts.hint_override.load() << " hint_illegal=" << ts.hint_illegal.load(); }
	{ auto& vs = zyf_verify_stats();
	  o << " verify_dead=" << vs.dead.load() << " verify_nolen=" << vs.nolen.load() << " verify_quota=" << vs.quota.load()
	    << " verify_tried=" << vs.tried.load() << " verify_ok=" << vs.ok.load() << " verify_fail=" << vs.fail.load()
	    << " verify_late=" << vs.late.load() << " use_cut=" << vs.use_cut.load() << " use_root=" << vs.use_root.load()
	    << " proof_detached=1 use_root_hint=" << vs.use_root_hint.load()
        << " action_deadline_us=" << zyf_action_deadline().load() << " report_us=" << CyfZyf::now_us(); }
	sync_cout << o.str() << sync_endl;
	int n = 0;
	for (const auto& r : g.results){
		if (n++ >= 32) break;
		const auto& j = r.job;
		sync_cout << "info string cyfzyf job i=" << (n - 1) << " kind=" << CyfZyf::kind_name(j.kind) << " src=" << CyfZyf::source_name(j.source)
		          << " ply=" << int(j.ply) << " stm=" << (j.stm ? 'w' : 'b') << " root_move=" << (j.root_move ? Move16(j.root_move).to_usi_string() : std::string("-"))
		          << " key=" << std::hex << j.key << std::dec << " depth=" << j.depth << " prio=" << j.priority
		          << " cyfnet=" << (j.has_cyfnet ? std::to_string(j.cyfnet) : std::string("none")) << " out=" << CyfZyf::outcome_name(r.outcome)
		          << " len=" << r.len << " nodes=" << r.nodes << " req_nodes=" << j.nodes << " req_ms=" << int(j.secs * 1000 + 0.5)
		          << " expiry_us=" << j.deadline_us << " supply_us=" << j.supply_us << " authority=hint"
		          << " wait_us=" << (r.start_us ? r.start_us - j.supply_us : -1) << " run_us=" << (r.start_us ? r.end_us - r.start_us : -1) << sync_endl;
	}
	for (const auto& cd : g.cands){
		uint8_t out = CyfZyf::kNone; int16_t len = 0;
		for (const auto& r : g.results) if (r.job.key == cd.key && r.job.kind == CyfZyf::kCand){ out = r.outcome; len = r.len; }
		int rank = -1; Value v = -VALUE_INFINITE;
		for (size_t i = 0; i < rootMoves.size(); ++i)
			if (uint16_t(rootMoves[i].pv[0].raw()) == cd.move){ rank = int(i); v = i == 0 ? rootMoves[i].uciScore : rootMoves[i].score; break; }
		sync_cout << "info string cyfzyf cand move=" << Move16(cd.move).to_usi_string() << " nnue_rank=" << cd.nnue_rank << " cyf_rank=" << cd.cyf_rank
		          << " cyfnet=" << (cd.has_cyfnet ? std::to_string(cd.cyfnet) : std::string("none")) << " src=" << CyfZyf::source_name(cd.source)
		          << " prio=" << cd.priority << " depth=" << cd.depth << " zyf=" << CyfZyf::outcome_name(out) << " len=" << len
		          << " final_rank=" << rank << " final=" << zyf_value_str(v) << " is_bestmove=" << int(rank == 0) << sync_endl;
	}
	sync_cout << "info string cyfzyf end gen=" << g.gen << " jobs=" << g.results.size() << " cands=" << g.cands.size() << sync_endl;
}



inline std::vector<uint32_t>& multiponder_pending(){ static std::vector<uint32_t> v; return v; }

Move csa_to_move(const Position& pos, const std::string& csa_in);   




struct RootPolicyPending { uint32_t id = 0; uint64_t key = 0, gen = 0; double dls = 0.0; bool active = false; };
inline RootPolicyPending& root_policy_pending(){ static RootPolicyPending p; return p; }
inline void dl_async_abandon_all(){
	auto& rp = root_policy_pending();
	if (rp.active){ sidecar().abandon(rp.id); rp.active = false; }
}

inline void set_root_policy(const std::vector<std::pair<std::string,float>>& pol, uint64_t root_key, uint64_t request_gen){
    if (!CyfSharp::state().bind_policy(request_gen, root_key)) return;
	last_dl_policy() = pol; last_dl_valid() = true;
	double ent = 0.0; int ent_k = 0;   
	for (auto& pr : pol) if (pr.second > 1e-7f){ ent -= pr.second * std::log((double)pr.second); ++ent_k; }
	last_dl_entropy() = (ent_k > 1) ? ent / std::log((double)ent_k) : 0.5;
	last_dl_entropy_valid() = (ent_k > 1);
}

inline void dl_async_collect(const Position& rootPos){
	if (!options().enable || bench_guard() || !sidecar().ok) return;
	auto& rp = root_policy_pending();
	if (rp.active){
		DlSidecar::Result r;
		if (sidecar().try_get(rp.id, r)){
			rp.active = false;
			if (r.ok && !r.policy.empty() && rp.key == (uint64_t)rootPos.key() && rp.gen == CyfSharp::state().gen){
				set_root_policy(r.policy, uint64_t(rootPos.key()), rp.gen);
			}
		}
	}
}



template<class RootMovesT>
inline void root_hint_collect(const Position& rootPos, const RootMovesT& rootMoves, int depth) {
	auto& s = CyfRootHint::state();
	if (!s.enabled.load(std::memory_order_relaxed)) return;
	CyfRootHint::Msg m;
	{
		std::lock_guard<std::mutex> lk(s.mtx);
		if (!s.pending) return;
		s.pending = false;
		if (s.msg.gen != s.go.gen || s.go.matched) { ++s.stale; return; }
        const auto kc=CyfRootHint::key_check(s.msg,s.go.root_key,s.go.require_key);
        if (kc==CyfRootHint::KeyCheck::missing) { ++s.key_missing; return; }
        if (kc==CyfRootHint::KeyCheck::mismatch) { ++s.key_mismatch; return; }
		m = std::move(s.msg);
	}
	auto& g = s.go;
	g.hint_has_key=m.has_key; g.hint_key=m.key; g.hint_native=m.native;
	g.matched = true; g.collect_depth = depth; g.kind = m.kind; g.model = m.model; g.apply = m.apply;
	g.n = int(m.moves.size());
	g.recv_ms = (m.recv_us - s.go_start_us) / 1000;
	g.collect_ms = (CyfRootHint::now_us() - s.go_start_us) / 1000;
	std::vector<std::pair<int, Move>> scored;
	const MoveList<LEGAL_ALL> rules_legal(rootPos);
	for (auto& [usi, sc] : m.moves) {
		const Move mv = USIEngine::to_move(rootPos, usi);
		if (mv == Move::none() || !rules_legal.contains(mv)) { ++g.illegal; continue; }
		if (std::find(rootMoves.begin(), rootMoves.end(), mv) == rootMoves.end()) { ++g.excluded_legal; continue; }
		++g.legal; scored.emplace_back(sc, mv);
	}
	std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (auto& p : scored) { g.order.push_back(p.second.raw()); g.scored.emplace_back(p.second.raw(), p.first); }
	if (!scored.empty()) g.top1 = scored[0].second.to_usi_string();
	if (!m.apply || scored.empty()) return;
	auto t = std::make_unique<CyfRootHint::Table>(); t->gen = g.gen;
	const int K = std::min<int>(std::min<int>(s.topk.load(), 64), int(scored.size()));
	const int step = s.step.load(), topk = std::min(s.topk.load(), 64);
	for (int i = 0; i < K; ++i) { t->move[i] = scored[size_t(i)].second.raw(); t->bonus[i] = (topk - i) * step; }
	t->n = K;
	s.active.store(t.get(), std::memory_order_release);
	s.owned.push_back(std::move(t));
	g.applied = K;
}




template<class RootMovesT>
inline int zyf_cand_verdict(Position& rootPos, const RootMovesT& rm) {
    if (rm.pv.empty()) return -1;
    const Move move=rm.pv[0];
    const auto& g=zyf_go();
    if (!std::any_of(g.results.begin(),g.results.end(),[&](const auto& r){
        return r.job.kind==CyfZyf::kCand && r.job.gen==g.gen && r.job.ply==1
            && r.job.root_move==uint16_t(move.raw()) && CyfZyf::is_verdict(r.outcome)
            && zyf_action_live(r.job.deadline_us);
    })) return -1;
    if (!rootPos.pseudo_legal_s<true>(move) || !rootPos.legal(move)) return -1;
    StateInfo st; rootPos.do_move(move, st);
    const uint64_t child_key=uint64_t(rootPos.key()); rootPos.undo_move(move);
    int verdict=-1;
    for (const auto& r : g.results) {
        if (r.job.kind!=CyfZyf::kCand || r.job.gen!=g.gen || r.job.ply!=1
            || r.job.root_move!=uint16_t(move.raw()) || r.job.key!=child_key
            || !zyf_action_live(r.job.deadline_us)) continue;
        if (CyfZyf::is_mate(r.outcome)) return 1; 
        if (r.outcome==CyfZyf::kNomate || r.outcome==CyfZyf::kCachedNomate) verdict=0;
    }
    return verdict;
}
template<class RootMovesT>
inline void zyf_safe_refresh(Position& rootPos, const RootMovesT& rootMoves) {
    const auto& g=zyf_go();
    if (!g.active || !g.armed || g.results.empty()) return;
    std::vector<CyfDefense::Candidate> candidates;
    const auto& hint=CyfRootHint::state().go;
    const bool applied=hint.matched && hint.apply && hint.applied>0
                    && hint.gen==g.gen && hint.root_key==uint64_t(rootPos.key());
    for (size_t i=0; i<rootMoves.size(); ++i) {
        if (rootMoves[i].pv.empty()) continue;
        CyfDefense::Candidate c;
        c.move=uint16_t(rootMoves[i].pv[0].raw()); c.search_rank=int(i);
        c.verdict=zyf_cand_verdict(rootPos, rootMoves[i]);
        if (c.verdict!=0 && c.verdict!=1) continue;   
        if (applied) for (const auto& h : hint.scored) if (h.first==c.move) {
            c.has_cyfnet=true; c.cyfnet=h.second; break;
        }
        candidates.push_back(c);
    }
    zyf_root_safe().store(std::any_of(candidates.begin(), candidates.end(), [](const auto& c){ return c.verdict==0; }),
                          std::memory_order_relaxed);
    CyfDefense::publish(g.gen, uint64_t(rootPos.key()), std::move(candidates));
}



template<class RootMovesT>
inline void frontier_export(const Position& rootPos, const RootMovesT& rootMoves, int completedDepth,
                            uint64_t gen, uint64_t nodes, uint64_t main_nodes, int64_t elapsed_ms) {
	auto& s = CyfFrontier::state();
	if (!s.enabled.load(std::memory_order_relaxed) || rootMoves.empty() || rootMoves[0].pv.empty()) return;
	if (s.emitted >= CyfFrontier::kMaxSnaps) return;   
	const int d1 = s.d1.load(std::memory_order_relaxed), d2 = s.d2.load(std::memory_order_relaxed);
	if (s.emitted == 1 && d2 <= d1) return;                    
	if (completedDepth < (s.emitted == 0 ? d1 : d2)) return;
	auto fmt = [](Value v) -> std::string {
		if (v == -VALUE_INFINITE || v == VALUE_NONE) return "none";
		if (std::abs(v) < VALUE_MATE_IN_MAX_PLY) return "cp:" + std::to_string(USIEngine::to_cp(v));
		const int n = v > 0 ? VALUE_MATE - v : -VALUE_MATE - v;   
		return std::string("mate:") + (v < 0 && n == 0 ? std::string("-0") : std::to_string(n));
	};
	const Color us = rootPos.side_to_move();
	const int K = std::min<int>(CyfFrontier::kMaxRootChildren, int(rootMoves.size()));
	std::ostringstream o;
	o << "info string cyffront v=1 gen=" << gen << " srch=" << s.serial << " pon=" << int(s.ponder)
	  << " snap=" << (s.emitted + 1) << " depth=" << completedDepth
	  << " sel=" << rootMoves[0].selDepth << " nodes=" << nodes << " mnodes=" << main_nodes << " ms=" << elapsed_ms
	  << " key=" << std::hex << uint64_t(rootPos.key()) << std::dec << " ply=" << rootPos.game_ply()
	  << " stm=" << (us == BLACK ? 'b' : 'w') << " nroot=" << rootMoves.size() << " k=" << K;
	for (int i = 0; i < K; ++i) {
		const auto& rm = rootMoves[size_t(i)];
		
		const Value v = i == 0 ? rm.uciScore : rm.score;
		const char* b = v == -VALUE_INFINITE ? (i == 0 ? "none" : "faillow")
		              : (rm.scoreLowerbound ? "lower" : rm.scoreUpperbound ? "upper" : "exact");
		o << " m" << i << '=' << rm.pv[0].to_usi_string() << " s" << i << '=' << fmt(v) << " b" << i << '=' << b
		  << " ar" << i << '=';
		if (rm.averageScore == -VALUE_INFINITE) o << "none"; else o << int(rm.averageScore);   
		o << " e" << i << '=' << (main_nodes ? rm.effort * 1000 / main_nodes : 0);
	}
	const auto& pv = rootMoves[0].pv;
	const size_t L = pv.size(), n = std::min<size_t>(L, CyfFrontier::kMaxPv);
	o << " pv=";
	for (size_t i = 0; i < n; ++i) o << (i ? "," : "") << pv[i].to_usi_string();
	
	const Color end_stm = (n % 2 == 0) ? us : ~us, full_stm = (L % 2 == 0) ? us : ~us;
	o << " pvlen=" << L << " pvtrunc=" << int(L > n) << " pvend=" << (end_stm == BLACK ? 'b' : 'w')
	  << " pvendfull=" << (full_stm == BLACK ? 'b' : 'w')
	  << " pvs=" << fmt(rootMoves[0].uciScore)
	  << " pvb=" << (rootMoves[0].uciScore == -VALUE_INFINITE ? "none"
	                 : rootMoves[0].scoreLowerbound ? "lower" : rootMoves[0].scoreUpperbound ? "upper" : "exact");
	const std::string line = o.str();
	++s.emitted; ++s.lines_total; s.bytes_total += line.size() + 1;
	sync_cout << line << sync_endl;
}


inline void preinfer_root_ready(const Position& pos, uint64_t gen, int64_t elapsed_ms,
                               int64_t optimum_ms, int64_t maximum_ms) {
    auto& s=CyfPreInfer::state();
    if (!s.enabled.load(std::memory_order_relaxed) || !s.root_ready(gen,uint64_t(pos.key()))) return;
    s.budget=CyfPreInfer::budget_decision(optimum_ms,elapsed_ms,s.min_budget_ms);
    auto_service().root_ready(pos,gen,s.budget);   
    sync_cout << "info string cyfgate v=2 gen=" << s.gen << " key=" << s.key << " ply=" << s.ply
      << " domain=root_ready gate=none q=na state=unknown mask=7 score=na candidate_fire=unknown"
      << " acquire=0 calibrated=0 threshold_domain=not_applicable legal_domain=rules_all legal=" << s.legal
      << " search_legal=" << s.search_legal << " target_nodes=" << s.target_nodes << " prev_target_nodes=" << s.prev_target_nodes
      << " prev=none prev_cp=na prev_nodes=na cur=none cur_cp=na depth=na bound=na nodes=0"
      << " tm_active=" << int(optimum_ms>=0)
      << " optimum_ms=" << (optimum_ms>=0 ? std::to_string(optimum_ms) : "na")
      << " maximum_ms=" << (maximum_ms>=0 ? std::to_string(maximum_ms) : "na")
      << " elapsed_ms=" << elapsed_ms
      << " budget_scope=initial_tm_optimum min_budget_ms=" << s.min_budget_ms
      << " budget_status=" << CyfPreInfer::name(s.budget.status)
      << " budget_remaining_ms=" << (s.budget.remaining_ms>=0 ? std::to_string(s.budget.remaining_ms) : "na")
      << " budget_eligible=" << CyfPreInfer::budget_eligible(s.budget) << sync_endl;
}


template<class RootMovesT>
inline void preinfer_completed(const Position& pos, const RootMovesT& rms, int depth,
                              uint64_t gen, uint64_t nodes, int64_t elapsed_ms) {
    auto& s=CyfPreInfer::state();
    if (!s.active || s.emitted || rms.empty() || rms[0].pv.empty()) return;
    const auto& rm=rms[0]; const Value v=rm.uciScore;
    CyfPreInfer::Input cur;
    cur.cp_ok=v!=-VALUE_INFINITE && v!=VALUE_NONE && std::abs(v)<VALUE_MATE_IN_MAX_PLY;
    cur.cp=cur.cp_ok ? USIEngine::to_cp(v) : 0;
    cur.move=uint16_t(rm.pv[0].raw()); cur.depth=depth; cur.nodes=nodes;
    cur.bound=rm.scoreLowerbound ? 'l' : rm.scoreUpperbound ? 'u' : 'e';
    if (!s.completed(gen,uint64_t(pos.key()),cur)) return;
    const auto& r=s.result;
    const bool fire=r.status==CyfPreInfer::Status::scored && CyfPreInfer::fires(r.score,20);
    sync_cout << "info string cyfgate v=1 gen=" << s.gen << " key=" << s.key << " ply=" << s.ply
      << " domain=iteration_proxy gate=G_Nprime q=20 state=" << CyfPreInfer::name(r.status)
      << " mask=" << r.mask << " score=" << std::setprecision(17) << r.score << std::setprecision(6)
      << " candidate_fire=" << int(fire) << " acquire=0 calibrated=0 threshold_domain=saved_independent_pair legal_domain=rules_all legal=" << s.legal
      << " search_legal=" << s.search_legal << " target_nodes=" << s.target_nodes << " prev_target_nodes=" << s.prev_target_nodes
      << " prev=" << (s.prev.move ? Move(uint32_t(s.prev.move)).to_usi_string() : "none")
      << " prev_cp=" << (s.prev.cp_ok ? std::to_string(s.prev.cp) : "na") << " prev_nodes=" << s.prev.nodes
      << " cur=" << rm.pv[0].to_usi_string() << " cur_cp=" << (cur.cp_ok ? std::to_string(cur.cp) : "na")
      << " depth=" << cur.depth << " bound=" << cur.bound << " nodes=" << cur.nodes << " elapsed_ms=" << elapsed_ms
      << sync_endl;
}
inline void preinfer_end(bool muted) {
    const auto& s=CyfPreInfer::state();
    if (muted || !s.enabled.load(std::memory_order_relaxed)) return;
    sync_cout << "info string cyfgate end gen=" << s.gen << " emitted=" << int(s.emitted)
      << " reason=" << s.reason << " rejected=" << s.rejected
      << " legal_domain=rules_all legal=" << s.legal << " search_legal=" << s.search_legal
      << " target_nodes=" << s.target_nodes << " prev_target_nodes=" << s.prev_target_nodes
      << " trigger=" << (s.ready_mode ? "root_ready" : "iteration")
      << " min_budget_ms=" << s.min_budget_ms << " budget_status=" << CyfPreInfer::name(s.budget.status) << sync_endl;
}



inline void iter_observe_dump(bool muted) {
	auto& s = CyfIter::state();
	if (!s.enabled.load(std::memory_order_relaxed)) return;
	if (muted) { ++s.muted_skipped; return; }
	const auto t0 = std::chrono::steady_clock::now();
	auto fmt = [](int32_t v) -> std::string {
		if (v == VALUE_NONE) return "none";
		if (v >= VALUE_INFINITE) return "inf";
		if (v <= -VALUE_INFINITE) return "-inf";
		if (std::abs(v) < VALUE_MATE_IN_MAX_PLY) return "cp:" + std::to_string(USIEngine::to_cp(Value(v)));
		const int n = v > 0 ? VALUE_MATE - v : -VALUE_MATE - v;   
		return std::string("mate:") + (v < 0 && n == 0 ? std::string("-0") : std::to_string(n));
	};
	auto mv = [](uint16_t raw) { return Move16(raw).to_usi_string(); };
	std::vector<std::string> out;
	{
		std::ostringstream o;
		o << "info string cyfiter v=1 begin gen=" << s.gen << " srch=" << s.serial << " pon=" << int(s.ponder)
		  << " key=" << std::hex << s.key << std::dec << " ply=" << s.ply << " stm=" << s.stm << " nroot=" << s.nroot
		  << " threads=" << s.threads << " rec=" << int(s.recording) << " snaps=" << s.snaps.size()
		  << " passes=" << s.passes.size() << " accepts=" << s.accepts.size() << " cands=" << s.cands.size()
		  << " trunc_snaps=" << s.snaps_trunc << " trunc_passes=" << s.passes_trunc << " trunc_accepts=" << s.accepts_trunc
		  << " trunc_cands=" << s.cands_trunc << " muted_skipped=" << s.muted_skipped
		  << " max=" << CyfIter::kMaxSnaps << '/' << CyfIter::kMaxCandPerSnap << '/' << CyfIter::kMaxCandTotal << '/'
		  << CyfIter::kMaxPasses << '/' << CyfIter::kMaxAcceptPerPass << '/' << CyfIter::kMaxAcceptTotal;
		out.push_back(o.str());
	}
	for (size_t i = 0; i < s.snaps.size(); ++i) {
		const auto& sn = s.snaps[i];
		std::ostringstream o;
		o << "info string cyfiter snap i=" << i << " depth=" << sn.depth << " sel=" << sn.sel << " nodes=" << sn.nodes
		  << " t_us=" << sn.t_us << " tm_ms=" << sn.tm_ms << " pass=" << sn.pass << " head=" << mv(sn.head)
		  << " nroot=" << sn.nroot << " k=" << sn.k << " trunc=" << int(sn.trunc) << " c=";
		for (int j = 0; j < sn.k; ++j) {
			const auto& c = s.cands[sn.off + uint32_t(j)];
			o << (j ? "," : "") << mv(c.move) << ':' << (c.bound == 'n' ? std::string("none") : fmt(c.score)) << ':' << c.bound << ':' << c.status;
		}
		out.push_back(o.str());
	}
	for (size_t i = 0; i < s.passes.size(); ++i) {
		const auto& p = s.passes[i];
		std::ostringstream o;
		o << "info string cyfiter pass i=" << i << " depth=" << p.depth << " adj=" << p.adj << " pv=" << p.pv << " att=" << p.att
		  << " alpha=" << fmt(p.alpha) << " beta=" << fmt(p.beta) << " best=" << (p.res == 'a' ? std::string("none") : fmt(p.best))
		  << " res=" << p.res << " t0_us=" << p.t0_us << " t1_us=" << p.t1_us << " nodes0=" << p.nodes0 << " nodes1=" << p.nodes1
		  << " seen=" << p.seen << " n=" << p.n << " a=";
		for (uint32_t j = 0; j < p.n; ++j) {
			const auto& a = s.accepts[p.off + j];
			o << (j ? "," : "") << mv(a.move) << ':' << a.stage;
		}
		out.push_back(o.str());
	}
	int64_t age = -1;
	const int latest = s.recording ? CyfIter::latest_before(CyfIter::now_us(s), age) : -1;
	uint64_t bytes = 0;
	for (const auto& l : out) { sync_cout << l << sync_endl; bytes += l.size() + 1; }
	const int64_t dump_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
	s.lines_total += out.size() + 1; s.bytes_total += bytes;
	sync_cout << "info string cyfiter end gen=" << s.gen << " srch=" << s.serial << " lines=" << (out.size() + 1)
	          << " bytes=" << bytes << " latest=" << latest << " latest_age_us=" << age << " n_accept=" << s.n_accept
	          << " n_score=" << s.n_score << " cyc_accept=" << s.cyc_accept << " cyc_score=" << s.cyc_score
	          << " cyc_pass=" << s.cyc_pass << " cyc_snap=" << s.cyc_snap << " dump_us=" << dump_us
	          << " lines_total=" << s.lines_total << " bytes_total=" << s.bytes_total << sync_endl;
	s.recording = false;
}





template<class RootMovesT>
inline void on_root_mainthread(Position& pos, RootMovesT& rootMoves, bool ponder){
    if (!bench_guard()) fusion_service().begin();
	{	
		
		Options& po = options();
		if (bench_guard()) harvest_probe().disable();
		else harvest_probe().begin(po.probe_min_depth,
		                           (size_t)(po.probe_cap_k > 0 ? po.probe_cap_k : 0) * 1024);
	}
	Options& o = options();
	const double gp  = game_phase(pos);
	const double dls = dl_policy_scale(gp, o.gp_onset, o.gp_floor);
	last_gp() = gp;   
	                  

	bool dl_active=false;
	std::string dl_top; int dl_topp=0;
	dl_async_abandon_all();   
	{ auto& mpp = multiponder_pending();   
	  for (uint32_t id : mpp) sidecar().abandon(id); mpp.clear(); }
	if (bench_guard()){ last_dl_valid()=false; return; }   
	
	if (o.mate_enable && zyf_armed().load(std::memory_order_relaxed) && active_cpu_budget().mate>0){
		
		{ char _hb[24]; std::snprintf(_hb, sizeof _hb, "%d", std::max(16, o.mate_hash_mb));
#if defined(_WIN32)
		  _putenv_s("ZYFAMATE_HASH_MB", _hb);
#else
		  setenv("ZYFAMATE_HASH_MB", _hb, 1);
#endif
		}
		mate_worker().stop_join(); mpon_worker().stop_join();
		if (zyf_service_active()) zyf_service().pause_join();   
		if (!(zyf_service_active() && zyf_same_go())) mate_table().begin();   
		mate_root().reset();
		
		
		{
			const double _hint_ms = (double)tm_optimum_hint();
			
			const double _tl = _hint_ms > 0.0 ? std::min(60.0, std::max(0.05, _hint_ms * 3.0 / 1000.0)) : 60.0;
			if (zyf_service_active()){ zyf_go_begin(true); zyf_submit_root(pos, _tl); }   
			else mate_worker().launch(pos.sfen(), (uint64_t)pos.key(), active_cpu_budget().mate, 1000000000L, _tl);
		}
	}
	else if (zyf_service_active()) zyf_go_begin(false);   

	if (!o.enable || fusion_service().cfg.mode) { last_dl_valid()=false; last_dl_entropy_valid()=false; return; }
	if (!sidecar_tried()){
		sidecar_tried()=true;
		const char* model=getenv("CYFAMATE_DL_MODEL");
		if (model && *model){
			
			const char* scr=getenv("CYFAMATE_DL_SCRIPT");
			std::string scrp = (scr&&*scr) ? std::string(scr) : cyf_dl_materialize_script();
			if (scrp.empty()) scrp = "cyfamate_dl.py";   
			sidecar().start(scrp, model);
		}
	}
	last_dl_valid()=false;
	if ((o.tm_enable || o.multiponder_topk > 0) && sidecar().ok && rootMoves.size()>1){
		
		
		std::vector<std::pair<std::string,float>> pol;
		const uint32_t rid = sidecar().post_policy(pos.sfen());
		DlSidecar::Result rres;
		const bool rhave = sidecar().try_get(rid, rres);
		if (!rhave){ auto& rp = root_policy_pending(); rp.id = rid; rp.key = (uint64_t)pos.key(); rp.dls = dls; rp.gen = CyfSharp::state().gen; rp.active = true; }
		if (rhave && rres.ok && !rres.policy.empty()){
			pol = std::move(rres.policy);
			dl_active=true;
			set_root_policy(pol, uint64_t(pos.key()), CyfSharp::state().gen);
			
			std::string best; float bestp=-1.f;
			for (auto& pr:pol) if (pr.second>bestp){ bestp=pr.second; best=pr.first; }
			dl_top=best; dl_topp=(int)(bestp*100);
		}
	}

	if (o.verbose){
		auto& os = sync_cout;
		os << "info string Cyfamate GP=" << (int)(gp*100)
		   << " phase=" << phase_label(gp)
		   << " dl_scale=" << (int)(dls*100)
		   << " roots=" << (int)rootMoves.size();
		if (dl_active){ os << " dl=on top=" << dl_top << " p=" << dl_topp
		                   << " ent=" << (int)(last_dl_entropy()*100); }
		else           os << " dl=off";
		{	
			auto& nc = sidecar().nncache;
			std::lock_guard<std::mutex> lk(nc.mtx);
			uint64_t tot = nc.hits + nc.misses;
			if (tot) os << " nnc=" << nc.hits << "/" << tot
			            << " ncsz=" << (nc.cur.size()+nc.prev.size());
		}
		os << sync_endl;
	}

	
	
	
	
	if (o.mate_enable && zyf_armed().load(std::memory_order_relaxed) && ponder && o.multiponder_topk > 0 && sidecar().ok && !bench_guard()){
		std::vector<std::pair<std::string,float>> pol_mp;
		bool have = last_dl_valid();
		if (have) pol_mp = last_dl_policy();                    
		
		if (have && !pol_mp.empty()){
			std::stable_sort(pol_mp.begin(), pol_mp.end(),
				[](const std::pair<std::string,float>& a, const std::pair<std::string,float>& b){ return a.second > b.second; });
			const int k = std::min<int>(o.multiponder_topk, (int)pol_mp.size());
			std::vector<std::pair<std::string,uint64_t>> jobs; jobs.reserve(k);
			for (int t = 0; t < k; ++t){
				Move rm = USIEngine::to_move(pos, pol_mp[t].first);
				if (rm == Move::none() || !pos.legal(rm)) continue;
				StateInfo si; pos.do_move(rm, si);
				jobs.push_back({ pos.sfen(), (uint64_t)pos.key() });   
				pos.undo_move(rm);
			}
			
			
			if (!jobs.empty() && zyf_go().active){   
				for (auto& jb : jobs){
					CyfZyf::Job j; j.key = jb.second; j.sfen = jb.first; j.gen = zyf_go().gen; j.kind = CyfZyf::kPonder;
					j.source = CyfZyf::kSrcMpon; j.ply = 2; j.priority = 1.0;
					j.nodes = std::max(1, o.mpon_node_limit); j.secs = std::max(1, o.zyf_job_ms) / 1000.0;
					zyf_job_deadline(j);
					zyf_service().submit(std::move(j));
				}
			}
			else if (!jobs.empty())
				mpon_worker().launch(jobs, (long)std::max(1, o.mpon_node_limit), 2.0);
		}
	}
}

} 
} 
#endif 