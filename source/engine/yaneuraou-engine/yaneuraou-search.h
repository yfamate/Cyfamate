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

// 🌈 やねうら王 探索部の設定
//     エンジンオプションで設定した設定値
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

    // この構造体メンバーに対応するエンジンオプションを生やす
    void add_options(OptionsMap& options);

    // この手数で引き分けとなる。256なら256手目を指したあとに引き分け。
    // 📝 options["MaxMovesToDraw"]の設定値。
    int max_moves_to_draw;

    // PVの出力の抑制のために前回出力時間からの間隔を指定できる。単位は[ms]
    // 📝 options["PvInterval"]の設定値。
    // ⚠ 探索中は、こちらの値を使うのではなく、computed_pv_intervalを使う。
    TimePoint pv_interval;

    // 検討モード用のPVを出力するのか
    // 📝 options["ConsiderationMode"]の設定値。
    bool consideration_mode;

    // fail low/highの時にPVを出力するか。
    // 📝 options["OutputFailLHPV"]の設定値。
    bool outout_fail_lh_pv;

    // 合法手を生成する時に全合法手を生成するのか(歩の不成など)
    // エンジンオプションのGenerateAllLegalMovesの値がこのフラグに反映される。
    // 📝 options["GenerateAllLegalMoves"]の設定値。
    bool generate_all_legal_moves;

    // 入玉ルール設定
    // 📝 options["EnteringKingRule"]の値。
    EnteringKingRule enteringKingRule;

    // 📌 ここ以降は、SearchManagerで用いるメンバ変数 📌

    // 前回のPV出力した時刻。PVが詰まるのを抑制するためのもの。
    // lastPvInfoTime       : 出力した時のnow()の値。
    // computed_pv_interval : 実際のPVの出力間隔[ms]。
    //                      📝 options["PvInterval"]とoptions["ConsiderationMode"]から決定したもの。
    //                      ⚠ "go infinite"された時や、ConsiderationMode == trueなら、0 が設定される。
    TimePoint lastPvInfoTime;
    TimePoint computed_pv_interval;
};

// 📌 Skill .. 手加減のための仕組み 📌
//    やねうら王では実装しない。

#if STOCKFISH
// Skill structure is used to implement strength limit. If we have a UCI_Elo,
// we convert it to an appropriate skill level, anchored to the Stash engine.
// This method is based on a fit of the Elo results for games played between
// Stockfish at various skill levels and various versions of the Stash engine.
// Skill 0 .. 19 now covers CCRL Blitz Elo from 1320 to 3190, approximately
// Reference: https://github.com/vondele/Stockfish/commit/a08b8d4e9711c2

// Skill 構造体は強さ制限を実装するために使われる。
// UCI_Elo が指定されている場合、Stash エンジンを基準として
// 適切なスキルレベルに変換する。
// この方法は、さまざまなスキルレベルの Stockfish と
// さまざまなバージョンの Stash エンジンとの対局結果に基づいている。
// 現在、Skill 0 から 19 は、おおよそ CCRL Blitz の Elo 1320 から 3190 をカバーする。
// 参考: https://github.com/vondele/Stockfish/commit/a08b8d4e9711c2

// 💡 Skill構造体は強さの制限の実装に用いられる。
//    (わざと手加減して指すために用いる) →　やねうら王では未使用

struct Skill {
    // Lowest and highest Elo ratings used in the skill level calculation
    constexpr static int LowestElo  = 1320;
    constexpr static int HighestElo = 3190;

    // skill_level : 手加減のレベル。20未満であれば手加減が有効。0が一番弱い。(R2000以上下がる)
    // uci_elo     : 0以外ならば、そのelo ratingになるように調整される。
    Skill(int skill_level, int uci_elo) {
        if (uci_elo)
        {
            double e = double(uci_elo - LowestElo) / (HighestElo - LowestElo);
            level = std::clamp((((37.2473 * e - 40.8525) * e + 22.2943) * e - 0.311438), 0.0, 19.0);
        }
        else
            level = double(skill_level);
    }

    // 手加減が有効であるか。
    bool enabled() const { return level < 20.0; }

    // SkillLevelがNなら探索深さもNぐらいにしておきたいので、
    // depthがSkillLevelに達したのかを判定する。
    bool time_to_pick(Depth depth) const { return depth == 1 + int(level); }

    // 手加減が有効のときはMultiPV = 4で探索
    Move pick_best(const RootMoves&, size_t multiPV);

    double level;
    Move   best = Move::none();
};

#else

// 💡 やねうら王ではSkillLevelを実装しない。
struct Skill {
    // dummy constructor
    Skill(int, int) {}

    // 常にfalseを返す。つまり、手加減の無効化。
    bool enabled() { return false; }
    bool time_to_pick(Depth) const { return true; }
    Move pick_best(const RootMoves&, size_t multiPV) { return Move::none(); }
    Move best = Move::none();
};

#endif


// 残り時間チェックを行ったり、main threadからのみアクセスされる探索manager
// 💡 Stockfishの同名のclassとほぼ同じ内容。Stockfishのsearch.hにあるSearchManagerを参考にすること。
// 🤔 YaneuraOuEngineの1インスタンスに対して、SearchManagerが1インスタンスあれば良いので、
//     やねうら王では、YaneuraOuEngineのメンバーとして持たせることにする。
class SearchManager {
   public:
    // 📝 やねうら王では、これはnamespace Searchで定義しておく。
#if STOCKFISH
    // Infoを更新した時のcallback。このcallbackを行うと標準出力に出力する。
    using UpdateShort    = std::function<void(const InfoShort&)>;
    using UpdateFull     = std::function<void(const InfoFull&)>;
    using UpdateIter     = std::function<void(const InfoIteration&)>;
    using UpdateBestmove = std::function<void(std::string_view, std::string_view)>;

    // PVを設定した時に出力するためのlistener
    struct UpdateContext {
        UpdateShort    onUpdateNoMoves;  // root局面で指し手がない時のhandler
        UpdateFull     onUpdateFull;
        UpdateIter     onIter;
        UpdateBestmove onBestmove;
    };
#endif

    SearchManager(const UpdateContext& updateContext) :
        updates(updateContext) {}

    // 指し手をGUIに返す時刻になったかをチェックする。
    // [NPS 2026-10-08] Counter test inlined into search(); the rest is check_time_expired().
    void check_time(YaneuraOuWorker& worker) {
        if (--callsCnt > 0)
            return;
        check_time_expired(worker);
    }
    void check_time_expired(YaneuraOuWorker& worker);

    // 現在のPV(読み筋)をUpdateContext::onUpdateFull()で登録する。
    // tt      : このスレッドに属する置換表
    // depth   : 反復深化のiteration深さ。
    void pv(Search::YaneuraOuWorker&  worker,
            const ThreadPool&         threads,
            const TranspositionTable& tt,
            Depth                     depth);

    // 🌈 start_searching()より前にUI threadから
    //		Worker::pre_start_searching()が呼び出され、virtualなので派生classの
    //      YaneuraOuWorker::pre_start_searching()が呼び出され、そこから委譲される。
    //     ponderフラグなどの初期化はここで行う。
    void pre_start_searching(YaneuraOuWorker& worker);

    /*
		📓 持ち時間管理

		tm                 :     持ち時間制御用。
		originalTimeAdjust :     持ち時間制御のためのパラメーター。やねうら王では使用しない。
		callsCnt           :     main threadが一定のnode数を探索するごとにcheck_time()を呼び出したいので、そのためのカウンター。
	*/
    TimeManagement tm;
#if STOCKFISH
    double originalTimeAdjust;
#endif
    int callsCnt;

    // "go ponder"実行中であるかのフラグ
    // 💡 "ponderhit"が来るとfalseになる。
    // 📓 "ponderhit"をUI threadが受信 → Engine::set_ponderhit() → YaneuraOuEngine::set_ponderhit()
    //     という流れでこのフラグが変更される。
    std::atomic_bool ponder;

    std::array<Value, 4> iterValue;

    // 💡 timeReductionは読み筋が安定しているときに時間を短縮するための係数。
    //     ここで保存しているのは、前回の反復深化のiterationの時のtimeReductionの値。
    double previousTimeReduction;

    // 前回の探索時のbestScore,bestAverageScore。
    // 📝 aspiration searchの初期値として用いる。
    Value bestPreviousScore;
    Value bestPreviousAverageScore;

    // ("go ponder"で思考を開始していて)次に"ponderhit"を受信したら
    // 探索を即座に終了させていいところまで探索が進んでいるフラグ。
    bool stopOnPonderhit;

    size_t id;

    const UpdateContext& updates;

    // 🌈 やねうら王独自 🌈

    /*
		 📓 やねうら王ではThread, ThreadPoolを完全に抽象化しているため、
			 ThreadPoolはincreaseDepthを持たず、このMainManagerが持っている。
	*/
    std::atomic<bool> increaseDepth;

    // やねうら王探索部で用いるオプション一覧
    SearchOptions search_options;

    // ponder用の指し手
    // 📝 やねうら王では、ponderの指し手がないとき、一つ前のiterationのときのPV上の(相手の)指し手を用いるという独自仕様。
    //     Stockfish本家もこうするべきだと思う。
    Move ponder_candidate;

    // 前回のgamePly。今回と手番が異なるかを検出するのに用いる。
    // 📝 Stochastic Ponderの場合、手番が異なることになる。
    //     この時、bestPreviousScore、bestPreviousAverageScoreを反転させる必要がある。
    int lastGamePly;
};

// -----------------------
//  探索のときに使うStack
// -----------------------

// 💡 このコードは、Stockfishの search.hにあったもの。

// Stack struct keeps track of the information we need to remember from nodes
// shallower and deeper in the tree during the search. Each search thread has
// its own array of Stack objects, indexed by the current ply.

// Stack構造体は、検索中にツリーの浅いノードや深いノードから記憶する必要がある情報を管理します。
// 各検索スレッドは、現在の深さ（ply）に基づいてインデックスされた、独自のStackオブジェクトの配列を持っています。

struct Stack {
    // PVへのポインター。
    PVMoves* pv;

    // historyのうち、counter moveに関するhistoryへのポインタ。実体はThreadが持っている。
    PieceToHistory* continuationHistory;

    // [pc][to]のペアに対する correction history。
    CorrectionHistory<PieceTo>* continuationCorrectionHistory;

    // rootからの手数。rootならば0。
    int ply;

    // そのスレッドの探索においてこの局面で現在選択されている指し手
    Move currentMove;

    // singular extension判定のときに置換表の指し手をそのnodeで除外して探索したいのでその除外する指し手
    Move excludedMove;

    // 評価関数を呼び出して得た値。NULL MOVEのときに親nodeでの評価値が欲しいので保存しておく。
    Value staticEval;

    // 一度計算したhistoryの合計値をcacheしておくのに用いる。
    int statScore;

    // このnodeでdo_move()した生成した何手目の指し手か。(1ならおそらく置換表の指し手だろう)
    int moveCount;

    // この局面で王手がかかっていたかのフラグ
    bool inCheck;

    // 置換表にPV nodeで調べた値が格納されていたか(これは価値が高い)
    bool ttPv;

    // 置換表にhitしたかのフラグ
    bool ttHit;

    // 前回の反復深化で得たPVを辿っているnodeか。
    // Stockfishでは、このPV line上では一部の浅い枝刈りを抑制する。
    bool followPV;

    // cut off(betaを超えたので枝刈りとしてreturn)した回数。
    int cutoffCnt;

    // このnodeでのreductionの量
    int reduction;
};


/*
   やねうら王 Engine(やねうら王の通常探索部)

   📓 Stockfishから拡張して、やねうら王はエンジンを自由に差し替えられるようになっているので、
      自分のEngineを定義するには、Engine classから派生させる。

      このclassがStockfishのEngine classに相当する。
      エンジン共通で必要なものは、IEngine/Engine(これが、それぞれエンジンのinterfaceとエンジン基底class)に移動させた。
*/
class YaneuraOuEngine: public Engine {
   public:
    // 📌 StockfishのEngine classに合わせる 📌

    // 📝 やねうら王では、namespace Searchに書いてあるので不要。
#if STOCKFISH
    using InfoShort = Search::InfoShort;
    using InfoFull  = Search::InfoFull;
    using InfoIter  = Search::InfoIteration;
#endif

    YaneuraOuEngine(/* std::optional<std::string> path = std::nullopt */) :
        manager(updateContext) {}

    // 📝 やねうら王では、CommandLine::gから取得できるので使わない。
    // const std::string binaryDirectory;

    // TODO : あとで整理する

    //NumaReplicationContext numaContext;

    // 📝 やねうら王では、base classであるEngine classが持っている。
#if STOCKFISH
    Position     pos;
    StateListPtr states;

    OptionsMap options;
    ThreadPool threads;
#endif

    // 📝 以下は、Engine classにある

	// 置換表
    // TranspositionTable tt;

    // TODO : あとで
    //LazyNumaReplicatedSystemWide<Eval::NNUE::Networks> networks;

    // Search::UpdateContext updateContext;

    // TODO : あとで
    //std::function<void(std::string_view)> onVerifyNetworks;

	// historyの集めたやつ。
    std::map<NumaIndex, SharedHistories>  sharedHists;

    // 🌈 やねうら王独自 🌈

    // 思考エンジンの追加オプションを設定する。
    virtual void add_options() override;

    // "isready"のタイミングでの初期化処理。
    virtual void isready() override;

    // "ponderhit"に対するhandler。
    virtual void set_ponderhit(bool b) override;

    // Threadのresizeするときのevent。
    virtual void resize_threads() override;

    // 置換表のresize event。
    virtual void set_tt_size(size_t mb) override;

	// 置換表の使用率を返す。
    virtual int get_hashfull(int maxAge) const override;

    // USI拡張コマンド "qsearch_psv" の実体。
    // .psv(PsvRecord列)の各局面をqsearch PVのleaf nodeで置換して書き出す。
    virtual bool qsearch_psv(const std::string& inputPath,
                             const std::string& outputPath,
                             size_t             workerCount,
                             std::string&       message) override;

    // [ZYF-RESERVE] Zyfamate の起動時予約と GP ラッチの実体。
    virtual void cyf_resource_sync(bool game_end) override;

	// 現在の局面の評価値の詳細を出力する。
    virtual void trace_eval() const override;

	// 現在の局面の評価値を出力する。
    virtual Value evaluate() const override;

    // StockfishのThreadPool::clear()にあったもの。
    void clear();

    // 定跡の指し手を選択するモジュール
    Book::BookMoveSelector book;

    // 探索manager
    // 📝 やねうら王では、Engine派生classがSearchMangerを持っている。
    Search::SearchManager manager;

    // Stockfishとの互換性のために用意。
    Search::SearchManager* main_manager() { return &manager; }
};

// やねうら王の探索Worker
// 📌 Stockfishから拡張して、やねうら王はエンジンを自由に差し替えられるようになっているので、
//     自分のWorkerを定義するには、Search::Worker classから派生させる。
// 💡 このclassのコードは、Stockfishのsearch.hにあるWorker classを参考にすること。
class YaneuraOuWorker: public Worker {
   public:
    // 💡 コンストラクタでWorkerのコンストラクタを初期化しないといけないので、
    //     少なくともWorkerのコンストラクタと同じ引数が必要。
    YaneuraOuWorker(SharedState&        sharedState,
					const ThreadIds&	ids,
                    // 追加でYaneuraOuEngineからもらいたいもの
                    YaneuraOuEngine&    engine);

    // "usinewgame"に対して呼び出される。対局前の初期化。
    virtual void clear() override;

    // "go"コマンドでの探索の開始時にmain threadから呼び出される。
    virtual void start_searching() override;

	// 📝 これは、やねうら王ではbase classで定義されている。
    //bool is_mainthread() const { return threadIdx == 0; }

    // 評価関数のパラメーターが各NUMAにコピーされているようにする。
    virtual void ensure_network_replicated() override;

    // qsearch<PV>()をTT hitなし扱いで行い、この呼び出し中に得られたPVを返す。
    // 返されたPVを進めた局面が、qsearchで到達したleaf nodeになる。
    Value qsearch_pv(Position& pos, PVMoves& pv);

    // 📌 Stockfishのsearch.hで定義されているWorkerが持っているメンバ変数 📌

	// Public because they need to be updatable by the stats
    // stats によって更新可能である必要があるため、public

    // 近代的なMovePickerではオーダリングのために、スレッドごとにhistoryとcounter movesなどのtableを持たないといけない。
    ButterflyHistory mainHistory;
    LowPlyHistory    lowPlyHistory;

    CapturePieceToHistory captureHistory;

    // コア数が多いか、長い持ち時間においては、ContinuationHistoryもスレッドごとに確保したほうが良いらしい。
    // cf. https://github.com/official-stockfish/Stockfish/commit/5c58d1f5cb4871595c07e6c2f6931780b5ac05b5
    // 添字の[2][2]は、[inCheck(王手がかかっているか)][capture_stage]
    // →　この改造、レーティングがほぼ上がっていない。悪い改造のような気がする。
    ContinuationHistory continuationHistory[2][2];
    CorrectionHistory<Continuation> continuationCorrectionHistory;

    TTMoveHistory    ttMoveHistory;
    SharedHistories& sharedHistory;

   private:
    // 反復深化
    // 💡 並列探索のentry point。
    //     start_searching()から呼び出される。
    bool iterative_deepening();

    // 📌 do_move～undo_move
    // 📝 do_move()は、Worker::nodesをインクリメントする。
    //     do_null_move()は、nodesはインクリメントしない。
    // 💡 givesCheckはこの指し手moveで王手になるか。
    //     これが事前にわかっているなら、do_move(move,st,givesCheck)を呼び出したほうが速い。

    void do_move(Position& pos, const Move move, StateInfo& st, Stack* const ss);
    void
    do_move(Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss);
    void do_null_move(Position& pos, StateInfo& st);

	void undo_move(Position& pos, const Move move);
    void undo_null_move(Position& pos);

	// 探索本体
    // This is the main search function, for both PV and non-PV nodes
    // これは PV ノードおよび非 PV ノードの両方に対応するメインの探索関数
    // 💡 最初、iterative_deepening()のなかから呼び出される。
    // [NPS 2026-10-08] search() is split in two. This inline part is the former first statement of
    // search() (dive into qsearch when depth <= 0); search_body() is the rest of the old body. A call
    // that only forwards to qsearch() therefore no longer sets up search()'s ~10KB frame first
    // (register saves, stack realignment), and qsearch()'s frame starts right below the caller's.
    template<NodeType nodeType>
    Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) {
        if (depth <= 0)
            return qsearch<nodeType != NonPV ? PV : NonPV>(pos, ss, alpha, beta);
        return search_body<nodeType>(pos, ss, alpha, beta, depth, cutNode);
    }
    template<NodeType nodeType>
    Value search_body(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);

    // 静止探索
    // Quiescence search function, which is called by the main search
    // メイン探索から呼ばれる静止探索関数
    // 💡 search()から、残りdepthが小さくなった時に呼び出される。
    template<NodeType nodeType, bool ReadTT = true>
    Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta);

	// LMRのreductionの値を計算する。
    // ⚠ この関数は、Stockfish 17(2024.11)で、1024倍して返すことになった。
    //   i     : improving , 評価値が2手前から上がっているかのフラグ。
    //                       上がっていないなら悪化していく局面なので深く読んでも仕方ないからreduction量を心もち増やす。
    //   d     : depth
    //   mn    : move_count
    //   delta : staticEvalとchildのeval(value)の差。これが低い時にreduction量を増やしたい。
    int reduction(bool i, Depth d, int mn, int delta) const;
    // [NPS 2026-10-09] reduction() with its delta * 764 / rootDelta term precomputed by the caller.
    int reduction_dt(bool i, Depth d, int mn, int deltaTerm) const {
        int reductionScale = reductions[d] * reductions[mn];
        return reductionScale - deltaTerm + !i * reductionScale * 161 / 512 + 963;
    }

	// Pointer to the search manager, only allowed to be called by the main thread
    // 検索マネージャへのポインタ。メインスレッドからのみ呼び出すことが許可されています。

#if STOCKFISH
    SearchManager* main_manager() const {
        assert(threadIdx == 0);
        return static_cast<SearchManager*>(manager.get());
    }
#else
    // 💡 Stockfishとの互換性のために用意。
    SearchManager* main_manager() const { return &manager; }
#endif

	// 時間経過。
	// 💡 やねうら王では、SearchManagerがTimeManagement tmを持っていて、
	//     このclassがelapsed()を持っているのでそちらを用いる。
#if STOCKFISH
    TimePoint elapsed() const;
    TimePoint elapsed_time() const;
#endif

	// 評価関数
    Value evaluate(const Position& pos);

	// "go"で渡された探索条件。
	// 💡 やねうら王ではbase classが持っている。
    //LimitsType limits;

    // MultiPVの時の現在探索中のPVのindexと、PVの末尾
    size_t pvIdx, pvLast;

    // nodes           : 探索node数これはbase classのほうにある。
    // tbHits          : tablebaseにhitした回数。将棋では使わない。
    // bestMoveChanges : bestMoveが反復深化のなかで変化した回数
    std::atomic<uint64_t> /* nodes, tbHits,*/ bestMoveChanges;

    // selDepth : 選択探索の深さ。
    // 💡depthとPV lineに対するUSI infoで出力するselDepth。
    int selDepth, nmpMinPly;

	// 探索時に評価値に楽観的バイアスを与えるために用いるパラメーター。
	Value optimism[COLOR_NB];

#if STOCKFISH
	// 探索開始局面とrootでのStateInfo
	// 📝 やねうら王では、Engine classが持っている。
    Position  rootPos;
    StateInfo rootState;
    RootMoves rootMoves;
#endif

    // aspiration searchで使う。
    Depth rootDepth, completedDepth;
    Value rootDelta;

    // 前回の反復深化で確定したPV。
    // 次の反復深化でこのPV lineを辿っている間は、IIRとquiet shallow pruningを抑制する。
    PVMoves lastIterationPV;

#if STOCKFISH
	// 📓 やねうら王では、Engine classが持っている。
	// threadIdx     : threadのindex(0からの連番), 0がmain thread
	// numaThreadIdx : NUMA内のthread index。
	// numaTotal     : NUMA内のtotal thread数。numaTotalThreads。
	// 📑この3つはコンストラクタで渡されたもの

	size_t                    threadIdx, numaThreadIdx, numaTotal;
    NumaReplicatedAccessToken numaAccessToken;
#endif

    // Reductions lookup table initialized at startup
    // 起動時に初期化されるreductionsの参照表
    // 💡 reductionとは、LMRで残り探索深さを減らすこと。
    /*
		📓	このテーブル、各workerが同じものを持っている。
		    頻繁に参照するテーブルなのでこのほうが良いのだと思われる。
	*/ 
    std::array<int, MAX_MOVES> reductions;  // [depth or moveNumber]

    // [MT-SHARE] elastic helper (Cyf_Zyf_Share=1): rank r parks while more than r Zyfamate solves run.
    int      elasticRank = -1;
    unsigned elasticTick = 0;
    void     elastic_park();

#if STOCKFISH
	// やねうら王では、これらは基底classであるWorkerが持っている。

    // The main thread has a SearchManager, the others have a NullSearchManager
    // メインスレッドは SearchManager を持ち、他のスレッドは NullSearchManager を持つ。
    std::unique_ptr<ISearchManager> manager;

    // 📝 tablebaseは将棋では使わない。
    Tablebases::Config tbConfig;

    const OptionsMap&                               options;
    ThreadPool&                                     threads;
    TranspositionTable&                             tt;
	// TODO : あとで考える。
    //const LazyNumaReplicatedSystemWide<Eval::NNUE::Networks>& networks;

#endif

	// 💡 やねうら王では、SearchManagerは、NullObject patternをやめて、単に参照で持つ。
    // 🤔 Stockfishも、main threadからしか呼び出さないのだから、これでいいと思うのだが…。
    SearchManager& manager;

	// start_searching()より前にUI threadから呼び出される。
    // 📝 より詳しい説明は、Worker::pre_start_searching()のコメントを読むこと。
    virtual void pre_start_searching() override;

	// NNUEの評価関数の計算用

#if STOCKFISH
	// TODO : あとでよく考える。

	// NNUE評価関数のパラメーターがNumaごとにコピーされるようにする。
	const LazyNumaReplicatedSystemWide<Eval::NNUE::Networks>& networks;

	// Used by NNUE
	// NNUEで使う

	// NNUE評価関数のnetworkのL1層を保持しているstack
    Eval::NNUE::AccumulatorStack  accumulatorStack;
	// NNUE評価関数の差分計算用
    Eval::NNUE::AccumulatorCaches refreshTable;
#endif

#if STOCKFISH
    // 📝 こちらは、StockfishではThreadPool::get_best_thread()の実装のために必要。
    //     やねうら王では、YaneuraOuWorkerでget_best_thread()を実装しているのでこのfriendは不要。
    friend class Stockfish::ThreadPool;
#else
	// 🌈 以下、やねうら王独自追加 🌈

    // 並列探索において一番良い思考をしたthreadの選出。
    // 💡 Stockfishでは ThreadPool::get_best_thread()に相当するもの。
    YaneuraOuWorker* get_best_thread() const;

	// WorkerのポインタをYaneuraOuWorkerのポインタにupcastする。
    // 💡 このWorkerから派生させるようなclass設計だと必要になるので用意した。
    YaneuraOuWorker* toYaneuraOuWorker(LargePagePtr<Worker>& worker) {
        //return dynamic_cast<YaneuraOuWorker*>(worker.get());
		// ⚠  RTTIが無効(コンパイル時に-frttiを指定している)ので
		//     dytnamic_castは使えない。
		//     このupcastができることはわかっているのでstatic_castを行う。
        return static_cast<YaneuraOuWorker*>(worker.get());
    }

    // Engine本体
	// 📝 コンストラクタで受け取ったもの。
    YaneuraOuEngine& engine;
#endif

	friend class SearchManager;
};

// ContinuationHistoryに対するbonusの係数
struct ConthistBonus {
    int index;
    int weight;
};

} // namespace Search

} // namespace YaneuraOu

#endif // defined(YANEURAOU_ENGINE)
#endif // YANEURAOU_SEARCH_H_INCLUDED

// ============================================================================
// ============================================================================
// Zyfamate ― 自己完結の df-pn 詰将棋エンジン。単独の USI 詰将棋エンジン Zyfamate（https://github.com/yfamate/Zyfamate）
// の source/zyfamate_core.h（commit a036e97）を source/zyfamate/ に借りている（変更せずに使う。更新は Zyfamate 側で行い写す）。
// ZYFAMATE_HOST_YANEURAOU で、証明木を探索側の局面鍵・指し手形式で渡す経路を有効にする。
// ============================================================================
#define ZYFAMATE_HOST_YANEURAOU 1
#include "../../zyfamate/zyfamate_core.h"
static bool _zyfamate_inited = (Zyfamate::init_tables(), true);  // 起動時1回
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
        // [USER 2026-10-05] len=std::max(0,mate_len_proof(&p,99));
        int first=first_mate_move(&p);
        if(first>=0)move_csa(&p,first,p.side,move);
        else {char pv[20000]{};/* [USER 2026-10-05] print_pv(&p,len>0?len:1,pv,0.1); */
            if(pv[0])std::sscanf(pv,"%15s",move);}
    }
    std::printf("CYF_ZYF_RESULT %d %d %s\n",result,len,move);std::fflush(stdout);return 0;
}
// ==== Zyfamate の子プロセス入口（Cyfamate 側）ここまで ====

#ifndef CYFAMATE_H_INCLUDED
#define CYFAMATE_H_INCLUDED
// ============================================================================
//  cyfamate.h ― Cyfamate 融合エンジン（単一ファイル / single-file）
// ----------------------------------------------------------------------------
//  NNUE-αβ master ＋ DL policy 参謀。やねうら王V941 NNUEビルドへ1行フックで統合。
//  移植元：pyfamate/Pyfamate.py（yfamate/Pyfamate 649e414）。対応表は source/cyfamate/PYFAMATE_PORTS.md。
//  Py→Cy = Python(Pyfamate)→C++ の後継。NNUE は「読み奴隷」(αβで深く読む)、
//  DL(policy) は候補を提案する参謀。判定は NNUE-αβ（非競合の役割分担）。
//
//  この1ファイルに集約:
//    (1) GamePhase  : 進行度GP（Pyfamate _calc_game_phase の C++移植・純関数）
//    (2) Options    : Cyfamate オプション（既定値内蔵キャッシュ）
//    (3) DlSidecar  : DL policy 参謀（onnxruntime常駐プロセス cyfamate_dl.py を駆動）
//    (4) on_root_mainthread() : 融合フック（GP算出→DL policy取得→GPゲート整列→info）
//
//  DLバックエンドについて:
//    DLネットの推論は cyfamate_dl.py（cshogi.dlshogi の特徴量抽出＝model.onnx の
//    学習時と同一）を ORT バックエンドとして fork/exec で常駐させ、root で問い合わせる。
//    （ふかうら王が ORT/TensorRT を別バックエンドとして使うのと同じ構図。融合判断＝
//      GPゲート・root整列・最終決定は すべて本C++エンジン側で行う。）
//    有効化: 環境変数  CYFAMATE_DL_MODEL=<model.onnx>  をセットして起動するだけ。
//            （任意で CYFAMATE_DL_SCRIPT=<cyfamate_dl.py パス>。既定 "cyfamate_dl.py"）
//    未設定なら DL は使わず素のやねうら王(NNUE-αβ)として動作（dl=off）。
//
//  統合（search.cpp に2点のみ）:
//    a) #include "cyfamate.h"
//    b) YaneuraOuWorker::start_searching() の mainthread 経路で
//         Cyfamate::on_root_mainthread(rootPos, rootMoves);
// ============================================================================
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

#if defined(CYF_ARCH_K3K3)
// k3k3 build: the NNUE side does not include the GP8 header; GP (time management, routing tables) still needs it.
#include "cyf_gp8_threat.h"
#endif
namespace YaneuraOu {
namespace Cyfamate {

// ============================================================================
// (1) GamePhase ― 進行度GP。Pyfamate _calc_game_phase の逐語移植 (2026-09-15)。
//     単一真実源は Pyfamate.py (_calc_state_phase / _game_phase_state_counts_core /
//     _gp_dev_contact_core / king_advance_signal)。
//     既定値は Pyfamate _CONFIG_DEFAULTS の fit 値 (2026-07-15c/TEACH-V3)。再 fit 時は
//     Pyfamate 側の焼付け値をここへ同期すること (DEV/CONTACT は重み 0 = no-op で待機)。
//     S  = Σ w_i·F_i/ref_i  (F = WHAND, PROMO, WCAMP, KING, EXPOSURE, DEV, CONTACT)
//     [NO-PLY 2026-09-17] 手数 (PLY) 項は全廃。教師局面 (PSV) から手数は取れないので、GP は盤面状態だけの関数。
//     GP = clamp( 1 - exp(-S/scale) + 0.5·clamp((KING-onset)/(full-onset),0,1) , 0, 1.5 )
//     [旧版との差] 旧 cpp は 4 特徴・重み全 1.0・手駒/敵陣は単純個数・CAMP に玉を含み
//     KING は段位置のみ、で Pyfamate と別スケールだった。
// ============================================================================
struct GamePhaseParams {
	// 手駒価値 (P L N S G B R) / 敵陣価値 (P L N S G B R +P +L +N +S +B +R)
	double hand_val[7]  = {1.0, 2.775113, 2.066253, 3.120539, 3.911216, 4.574237, 7.863081};
	double camp_val[13] = {1.396302, 1.177588, 1.248085, 1.297911, 1.444452, 1.066618, 1.40945,
	                       0.563566, 1.186525, 0.91197, 1.020019, 0.592588, 1.458039};
	// [FIT 2026-09-16] Pyfamate game_phase_fit (16.6M 順序ペア, iter 600) と同値。駒種価値表は 07-15c のまま。
	double w_hand=0.334803, w_promo=0.0,      w_camp=0.312461, w_king=0.540983;
	double ref_hand=2.035839, ref_promo=5.443070, ref_camp=1.173523, ref_king=0.121706;
	double w_expo=0.0, ref_expo=12.212311;
	double w_dev=0.0, ref_dev=15.0, w_contact=0.0, ref_contact=4.0;
	double scale=1.285300, nyugyoku_onset=0.514070, nyugyoku_full=4.757159;
};
struct GamePhaseFeatures {
	double whand=0, promo=0, wcamp=0, king=0, expo=0, dev=0, contact=0;
};
inline int gp_piece_idx(PieceType pt){   // 手駒/敵陣テーブル index。玉・無効は -1
	switch (pt) {
	case PAWN: return 0; case LANCE: return 1; case KNIGHT: return 2; case SILVER: return 3;
	case GOLD: return 4; case BISHOP: return 5; case ROOK: return 6;
	case PRO_PAWN: return 7; case PRO_LANCE: return 8; case PRO_KNIGHT: return 9;
	case PRO_SILVER: return 10; case HORSE: return 11; case DRAGON: return 12;
	default: return -1;
	}
}
// 平手初期配置 (DEV 用)。Pyfamate _GP_INITIAL_SQUARES と同一 (40 マス)。
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
// 玉前進シグナルの随伴駒マスク: 玉から Chebyshev 距離 2 以内かつ後方 1 段まで (玉自身を除く)。[color][king sq]
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
// 玉前進シグナル (KING-v2: 入口段からの深さ × 随伴ゲート) と、両玉 ring1 の味方駒数。
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
		const int kr = (int)rank_of(ksq) + 1;   // 1..9
		const int raw = (c == BLACK) ? (7 - kr) : (kr - 3);
		if (raw < 0) continue;
		const int sup = std::min(3, (own & sup_mask[c][ksq]).pop_count());
		king += (raw + 0.5) * (0.25 + 0.25 * sup);
	}
	return king;
}
// 特徴抽出。値は Pyfamate _game_phase_state_counts_core / king_advance_signal / _gp_dev_contact_core と同一。
//   [GP-FAST 2026-09-16] 盤走査をやめ bitboard の popcount で計算。DEV / CONTACT は重み≠0 のときだけ計算する
//   (寄与 0 なら GP の値は不変)。
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

// GP gate: 終盤(高GP)ほど DL の寄与を弱める係数 ∈ [floor,1]。
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
// ============================================================================
// [5GP] SFNN_halfka2_1024_7_64_5GP のレイヤースタック選択 (組 = GP を Eval::NNUE::kGpStackBounds で区切る)。
//   GP の値そのものは game_phase() と完全に同一。ここでは毎ノード呼ばれるので差分計算で速くしているだけ。
//   S = 手駒・敵陣・成駒 (StateInfo::gp_s を do_move で差分更新) + 玉前進項。
//   玉前進シグナル KING は玉が入口段 (先手七段 / 後手三段) 以遠にいるときだけ 0 でないので、
//   その場合だけ玉周辺の bitboard から計算する。
//   [2026-09-17 修正] 旧実装は「w_king == 0 のときだけ差分計算」としていたが fit 値は W_King≠0 なので
//   常に全計算 (毎ノード盤面全体から GP) に落ちていた。KING を上記の条件付き計算に分けて解消。
// ============================================================================
inline const GamePhaseParams kGpParams{};   // 差分計算用 (関数内 static の初期化チェックを避ける)
// 差分更新テーブルを GamePhaseParams (単一の真実源) から構築する。
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
// 差分計算が使えるか。S に局面全体を見る項 (玉露出・DEV・CONTACT) が入らない係数であること。
inline bool gp_inc_usable(){
	const GamePhaseParams& p = kGpParams;
	return p.w_expo == 0.0 && p.w_dev == 0.0 && p.w_contact == 0.0;
}
// 玉が入口段以遠にいるか (gp_king_term が 0 でなくなる条件と同値)。
inline bool gp_king_in_zone(const Position& pos){
	const Square kb = pos.square<YaneuraOu::KING>(BLACK), kw = pos.square<YaneuraOu::KING>(WHITE);
	return (kb < SQ_NB && (int)rank_of(kb) <= 6) || (kw < SQ_NB && (int)rank_of(kw) >= 2);
}
// [GP8 research] the live stack count / boundaries come from the file header (Gp8); the
// tables below are rebuilt by gp8_rebuild_tables() after each load.
constexpr int kGpTopStack = Gp8::kMaxStacks - 1;
inline int gp_top_stack() { return Gp8::stacks - 1; }
inline int gp_bucket_of(double gp){
	int b = 0;
	const int top = gp_top_stack();
	while (b < top && gp >= Gp8::bounds[b]) ++b;
	return b;
}
// 入玉加点が 0 の局面では GP = 1 − exp(−S/Scale) なので、GP ≥ b ⇔ S ≥ −Scale·ln(1−b) (b ≥ 1 は到達不能)。
inline std::array<double, kGpTopStack> gp_stack_s_bounds{};
struct GpProgressTerms { double sum, bonus; };
inline double gp_progress_value(GpProgressTerms terms){
    const double neg = -terms.sum / kGpParams.scale;
    const double core = neg < -10.0 ? 1.0 : 1.0 - std::exp(neg);
    return std::clamp(core + terms.bonus, 0.0, 1.5);
}

// Integer king code is exactly eight times KING-v2: no quantization is introduced.
// The support mask excludes the king itself, so counting own pieces needs no
// separate removal of the kings or ring1 computation (unused by the fitted GP).
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

// The previous routing path is retained as an independent contract for table
// construction/tests. Preserve its floating-point order, clamp and S>10*scale jump.
inline int gp_bucket_reference(double sum, double bonus) {
    if (bonus == 0.0 && sum < 10.0*kGpParams.scale) {
        int b=0;
        const int top = gp_top_stack();
        while (b<top && sum>=gp_stack_s_bounds[b]) ++b;
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
inline std::array<GpKingRow,105> gp_king_rows{};
// Rebuilds the S-space boundaries and the per-king-code routing rows for the live Gp8
// boundary list. Same construction as the former compile-time tables.
inline void gp8_rebuild_tables() {
    static_assert(sizeof(double)==sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559);
    const int top = gp_top_stack();
    for (int i = 0; i < kGpTopStack; ++i) {
        const double b = i < top ? Gp8::bounds[i] : 1e300;
        gp_stack_s_bounds[i] = b <= 0.0 ? -1e300 : (b >= 1.0 ? 1e300 : -kGpParams.scale * std::log(1.0 - b));
    }
    for (unsigned code=0; code<gp_king_rows.size(); ++code) {
        auto& row=gp_king_rows[code];
        const double king=double(code)/8.0;
        row.offset=kGpParams.w_king*king/kGpParams.ref_king;
        row.bonus=king>kGpParams.nyugyoku_onset
            ? 0.5*std::min(1.0,(king-kGpParams.nyugyoku_onset)
                                     /(kGpParams.nyugyoku_full-kGpParams.nyugyoku_onset)) : 0.0;
        for (int b=0;b<kGpTopStack;++b) {
            if (b >= top) { row.boundary[b] = std::numeric_limits<double>::infinity(); continue; }
            // Find the FIRST representable gp_s that enters the next bucket.
            // This handles strict saturation boundaries and exp rounding exactly;
            // runtime routing needs neither exp nor epsilon/fallback heuristics.
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
}
inline const bool gp8_tables_initialised = [] { gp8_rebuild_tables(); return true; }();
inline GpProgressTerms gp_progress_terms(const Position& pos) {
    const auto& row=gp_king_rows[gp_king_code(pos)];
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
    const auto& row=gp_king_rows[gp_king_code(pos)];
    const double s=st->gp_s;
    // [GP8 research] up to eight ordered comparisons select one of the live stacks.
    int b = 0;
    const int top = gp_top_stack();
    while (b < top && s >= row.boundary[b]) ++b;
    st->gp_bucket=std::uint8_t(b);
    return b;
}
inline const char* phase_label(double gp){
	if (gp > 1.0) return "nyugyoku";
	const double g = game_phase_for_phase(gp);
	if (g < 0.33) return "opening";
	if (g < 0.66) return "middlegame";
	return "endgame";
}

// ============================================================================
// (2) Options ― Cyfamate オプション（既定値内蔵）
// ============================================================================
struct Options {
	bool   enable        = false;  // Legacy root sidecar; asynchronous Fusion is configured separately.
	bool   verbose       = true;   // info string Cyfamate ... を出す
	double gp_onset      = 0.6;    // GP gate 開始
	double gp_floor      = 0.3;    // GP gate 下限
	double dl_conf_min   = 0.30;   // DL top policy 確信度の下限 (TM の不一致判定が参照)
	// ---- 計装 (harvest 再訪率。DL とは独立。既定 off) ----
	int    probe_min_depth   = 0;    // Cyf_Probe_MinDepth: X。残り深さがこれ以上の節点だけ記録 (0=off)
	int    probe_cap_k       = 4096; // Cyf_Probe_Cap_K: 記録上限 [K events] (16B/event)
	// ---- P0: DL Sidecar v2（非同期 reader / バッチ / value 対応）----
	// [2026-07-17 残骸除去] sidecar_v2 フラグ・旧同期プロトコル ("sfen"→"OK") は撤去。
	//   埋め込み sidecar は常に READY v2 を名乗り (L32056)、native 経路も v2 相当のため
	//   旧経路は到達不能の死重だった。非 v2 を名乗る外部サーバは start() で明示拒否する。
	// ---- P2: PV ノード Value 混合（DL value を eval へ。要 v2 Python の VALUE 応答。legacy では自動 no-op）----
	// ---- Ponder + Zyfamate df-pn 統合（SPEC_cyfamate_ponder_zyfamate §2/§4）----
	int    multiponder_topk  = 0;    // Legacy sidecar disabled in the operational profile.
	bool   mate_enable       = true; // Included in the GUI Threads budget.
	bool   mt_obs            = false; // [MT-LOGIC] Cyf_MT_Obs: observation lines only
	bool   mt_privhist       = true; // [MT-LOGIC] Cyf_MT_PrivHist: per-thread pawn/correction history (next isready)
	bool   vote_depth        = true; // [MT-LOGIC] Cyf_Vote_Depth: vote among deepest threads only
	int    mate_threads      = 0;    // 0=automatic; explicit counts remain inside Threads.
	int    zyf_share         = 1;    // [MT-SHARE] Cyf_Zyf_Share (default 1 since 2026-10-14: M1ab share=1 vs reserve 0.625 [0.566,0.684]): 0=reserve, 1=elastic helpers park while Zyfamate solves, 2=oversubscribe at low priority
	// [ZY-01] Cyf_Zyf_*: resident Zyfamate service (cyf_zyf_core.h). Off: the original root/MPON workers run.
	bool   zyf_service       = true;  // Cyf_Zyf_Service: bounded resident root/PV/candidate owner
    bool   zyf_play_hint     = false; // legal root move advice, without mate-score authority. [USER 2026-10-05] off:
                                      //   with tree sharing the search plays the mate itself (Zyfamate is an aid only)
	bool   zyf_keep_proofs   = false; // Cyf_Zyf_Keep_Proofs: Zyfamate keeps proofs across solves (same attacker)
	bool   zyf_tree_share    = true;  // Cyf_Zyf_Tree_Share: Zyfamate's proven OR nodes become search move hints ([USER 2026-10-05] on)
	int    zyf_tree_max      = 1024;  // Cyf_Zyf_Tree_Max: proof-tree OR nodes harvested per proven job
	bool   zyf_tree_extend   = false; // Cyf_Zyf_Tree_Extend: +1 ply for the proof-tree hint move (experiment)
	bool   zyf_tree_live     = false; // Cyf_Zyf_Tree_Live: publish each proven OR node to the MateTable while solving (experiment)
	bool   zyf_tree_bound    = false; // Cyf_Zyf_Tree_Bound: a node of a shared proof tree is a mate lower bound for the side
	                                  //   to move (no length). With the opponent's (tonshi) trees the search itself drops
	                                  //   the root move that walks into them; once a root move is proven safe, such a
	                                  //   root move ranks last (zyf_root_safe). (experiment)
	int    zyf_go_jobs       = 8;     // Cyf_Zyf_Go_Jobs: PV/candidate jobs supplied per go (root not counted)
	// [ZYF-DEFENSE 2026-10-05, USER] With a Zyfamate budget of 2 threads, the second thread is a defense lane: it reads
	//   the opponent's mate after our root moves (candidate jobs), and a proven sudden mate (tonshi) can change our move.
	bool   zyf_defense       = true;  // Cyf_Zyf_Defense: Zyfamate threads=2 -> service with attack + defense lanes
	int    zyf_defense_jobs  = 16;    // Cyf_Zyf_Defense_Jobs: candidate jobs per go on the defense lane
	int    zyf_defense_ms    = 200;   // Cyf_Zyf_Defense_Ms: time per defense job
	int    zyf_defense_nodes = 1000000; // Cyf_Zyf_Defense_Nodes: nodes per defense job
	int    zyf_tonshi_switch = 1;     // Cyf_Zyf_Tonshi_Switch: 0 off, 1 switch a proven-tonshi move to a proven-safe one,
	                                  //   2 also to the best not-proven-losing move when none is proven safe.
	                                  //   Which move replaces it is deliberately open (USER 2026-10-05: Cyfnet is involved;
	                                  //   design pending). 1/2 are provisional.
	bool   zyf_safe_order    = true;  // prefer a proven-no-checking-mate root move; Cyfnet ranks within that set
	int    zyf_job_nodes     = 100000;// Cyf_Zyf_Job_Nodes
	int    zyf_job_ms        = 50;    // Cyf_Zyf_Job_Ms
	int    zyf_root_nodes    = 0;     // Cyf_Zyf_Root_Nodes: 0 = the original root budget (1e9)
	int    zyf_root_ms       = 0;     // Cyf_Zyf_Root_Ms: 0 = auto slice (optimum/4, capped by job time)
	int    zyf_pv_plies      = 6;     // Cyf_Zyf_PV_Plies
	int    zyf_cands         = 3;     // Cyf_Zyf_Cands: root candidates by NNUE order (and as many by Cyfnet hint)
	int    zyf_min_depth     = 6;     // Cyf_Zyf_Min_Depth: first completed depth that supplies jobs
	int    mate_hash_mb      = 256;  // Total portfolio table budget, divided across processes.
	int    mate_probe_depth  = 10;   // Cyf_Mate_Probe_Depth: この depth 以上の PV ノードで MateTable probe
	int    mpon_node_limit   = 200000;  // Cyf_MPON_Node_Limit: MPON 各候補の df-pn ノード上限
	int    tonshi_ms         = 60;   // Cyf_Tonshi_Ms: 頓死ガードの相手番 df-pn 予算[ms]（0=無効）
	// [2026-07-17] tsumero_enable 撤去（参照ゼロの死重。CSA `tsumero` コマンドは独立に健在）
	// ---- NN 結果キャッシュ（lc0 NNCache 系。RESEARCH_nvidia_vram §6 / part2 §G-1）----
	int    nncache_mb        = 64;   // Cyf_NNCache_MB: policy+value キャッシュ予算[MB]（0=無効）
	// ---- [2026-07-17 part3 §I] 時間管理×DL一致 (§I-4) + lowPlyHistory シード (§I-2) ----
	bool   tm_enable         = false; // Legacy DL time management.
	int    tm_agree          = 92;   // Cyf_TM_Agree: rootMoves[0]==DL argmax 時の % (100=中立)
	int    tm_disagree       = 115;  // Cyf_TM_Disagree: 確信 (dl_conf_min 以上) つき不一致時の %
	// ---- [2026-07-17] Pyfamate time_aggression から移植する2因子 (GP / 形勢)。他因子は
	//      SF系 in-loop TM と重複のため不採用 (falling-eval/安定短縮/sharpness/手変わり延長は既在)。
	int    tm_gp_early       = 96;   // Cyf_TM_GP_Early: 序盤 (GP≈0) の % — 貯めて中盤へ回す
	int    tm_gp_mid         = 112;  // Cyf_TM_GP_Mid:   中盤 (GP≈0.5) の % — 厚く使う (100/100 で無効化)
	int    tm_score_start_cp = 1000; // Cyf_TM_Score_Start: |bestValue| がこの cp を超えたら節約開始
	int    tm_score_floor    = 75;   // Cyf_TM_Score_Floor: 大差時の下限 % (span は start→2*start で線形減衰)
	// ---- [TA 移設 2026-09-16] Pyfamate Time Aggression の未移植因子 ----
	int    tm_gp_gamma       = 100;  // Cyf_TM_GP_Gamma: GP テントの曲率 γ×100 (100=従来の線形と恒等)
	int    tm_momentum_ext   = 25;   // Cyf_TM_Momentum_Ext: 負 momentum の延長ゲイン % (Pyfamate 既定 0.25)
	int    tm_momentum_acc   = 10;   // Cyf_TM_Momentum_Acc: 正 momentum の加速ゲイン % (Pyfamate 既定 0.10)
	int    tm_sharp_base     = 85;   // Cyf_TM_Sharp_Base: sharpness 因子の下端 % (base + range*sharp)
	bool   tm_sharp_current  = false; // opt-in current-root Sharpness instead of lag-1
	int    tm_sharp_range    = 30;   // Cyf_TM_Sharp_Range: 同 振れ幅 %
	int    tm_tesuki_ms      = 40;   // Cyf_TM_Tesuki_Ms: 自玉の詰めろ深さ測定の予算[ms] (0=測らない)
	int    tm_ponder_bonus   = 125;  // Cyf_TM_Ponder_Bonus: go ponder 由来の探索の延長 % (Pyfamate TA ⑥ 既定 1.25)
};
// [TA-CLAMP] Pyfamate _TA_CEIL / _TA_FAC_FLOOR と同値の絶対境界 (tm_dl_scale の積に課す)。
constexpr double CYF_TM_CEIL  = 1.25;
constexpr double CYF_TM_FLOOR = 0.05;
inline Options& options(){ static Options o; return o; }

// [2026-07-17 短時間耐性] 今手の optimum (ms) ヒント。start_searching が go 毎に設定。
//   0 = 時間管理外 (go nodes/depth/infinite) → 各予算はオプション値のまま。
inline int& tm_optimum_hint(){ static int h = 0; return h; }
//   TC 連動クランプ (query≤optimum/8 / prior≤optimum/10 / prefetch は optimum<800ms で不発) は
//   各利用点で tm_optimum_hint() から直接算出する。共通ヘルパ eff_ms は参照ゼロのため撤去。

// bench 経路で DL(sidecar/prior)を強制 off にするガード（§4）。素ビルドと bench 署名一致。
inline bool& bench_guard(){ static bool b=false; return b; }

// pre-search(on_root) で DL policy/GP を保存し、TM / MPON が参照する。
inline std::vector<std::pair<std::string,float>>& last_dl_policy(){ static std::vector<std::pair<std::string,float>> v; return v; }
inline double& last_gp(){ static double g=0.0; return g; }
inline bool&   last_dl_valid(){ static bool b=false; return b; }
// DL policy entropy (pre-search difficulty signal)。Pyfamate が観測する typicality の
// Cyfamate 版。正規化 entropy ∈ [0,1]。低=DL 確信(明快な局面)、高=DL 不確実(複雑な局面)。
inline double& last_dl_entropy(){ static double e=0.5; return e; }
inline bool&   last_dl_entropy_valid(){ static bool b=false; return b; }

// ============================================================================
// [CSM] Momentum / Sharpness — Pyfamate _compute_momentum / _compute_sharpness の移設。
//   いずれも「前手の値 (lag-1)」を時間管理へ返す観測量。局面推移は緩やかなので 1 手遅延は許容。
//   bestmove 確定後 (post_move_metrics) に更新し、次手の tm_dl_scale が読む。
//   DL (Cyfnet) 無しでも成立する: sharpness は NNUE スコアの接近度、momentum は
//   スコア差分・GP 差分・best 手の安定度 + Zyfamate の詰めろ深さ (tesuki) から作る。
// ============================================================================
inline double& last_momentum(){ static double m=0.0; return m; }          // [-1,1] 負=悪化/詰めろ, 正=好調
inline double& last_sharpness(){ static double s=0.5; return s; }         // [0,1] 高=難しい局面
inline int&    last_tesuki_n(){ static int n=-1; return n; }              // Zyfamate n手すき (-1=未測定)
inline int&    prev_best_cp(){ static int c=0; return c; }
inline bool&   prev_best_cp_valid(){ static bool b=false; return b; }
inline double& prev_gp(){ static double g=-1.0; return g; }
inline int&    calm_streak(){ static int s=0; return s; }                 // 読み筋どおりに進んだ連続手数
inline std::string& prev_pred_usi(){ static std::string s; return s; }    // 前手 PV の「次の自分の手」(pv[2])
inline int&    csm_last_ply(){ static int p=-1; return p; }

// cp → 勝率 (Pyfamate _cp_to_winrate と同式。slope は Pyfamate の _CSM_* 既定値)。
inline double cp_to_wr(double cp, double slope){ return 1.0 / (1.0 + std::exp(-cp / slope)); }

// Pyfamate _compute_sharpness の 4 軸合成 (instability / tightness / closeness / disagree)。
//   DL が無い軸は 0.5 (中立) を入れる — Pyfamate の None 扱いと同じ。
inline double compute_sharpness(int best_cp, bool dl_ok, double dl_policy_max, bool agree, bool cp_ok = true){
	return CyfSharp::observe(best_cp, cp_ok, dl_ok, dl_policy_max, agree).scalar;
}

// Pyfamate _compute_momentum の移設 (重み 0.45/0.35/0.20・正規化 200/5/2.5・tesuki penalty も同値)。
inline double compute_momentum(int best_cp, bool cp_ok, double gp, double gp_prev, int streak, int tesuki_n){
	auto cl = [](double v){ return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v); };
	const double score_trend = cp_ok ? cl((best_cp - prev_best_cp()) / 200.0) : 0.0;
	const double gp_push     = gp_prev >= 0.0 ? cl((gp - gp_prev) * 5.0) : 0.0;
	const double streak_calm = cl((streak - 2.5) / 2.5);
	double base = 0.45 * score_trend + 0.35 * gp_push + 0.20 * streak_calm;
	if (tesuki_n >= 0 && tesuki_n <= 2)   // 0=被詰 / 1=詰めろ / 2=2手すき → 詰め脅威ペナルティ
		base -= std::max(0.0, 1.0 - tesuki_n / 3.0);
	return base < -1.0 ? -1.0 : (base > 1.0 ? 1.0 : base);
}

// DL argmax (TM 一致度 / MPON が参照)。
//   last_dl_valid() 前提。conf<0 なら policy 空。
inline std::string last_dl_argmax(float& conf){
	conf = -1.f; std::string top;
	for (auto& pr : last_dl_policy()) if (pr.second > conf){ conf = pr.second; top = pr.first; }
	return top;
}

// [2026-07-17 一本化] 時間管理の Cyfamate 4因子 (entropy / DL一致度 / GP / 形勢) を単一関数へ。
//   iterative_deepening の totalTime 式から呼ばれる。全因子は有界乗数・既定穏当・USI で恒等化可。
//   root_best_usi: 現 rootMoves[0].pv[0] の USI / best_cp: USIEngine::to_cp(bestValue) /
//   mate_range: |bestValue| が TB/mate 圏か (形勢因子を切る)。
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
	// (1) entropy: 難易度 — DL 確信 (低ent) で短縮、不確実で延長
	if (last_dl_entropy_valid())
		s *= 0.85 + 0.30 * last_dl_entropy();
	// (2) 一致度: 意見の割れ — rootMoves[0]==DL argmax で短縮、確信つき不一致で延長 (GP減衰)
	if (o.tm_enable && last_dl_valid() && !root_best_usi.empty()){
		float dbp; const std::string dtop = last_dl_argmax(dbp);
		const double dls = dl_policy_scale(last_gp(), o.gp_onset, o.gp_floor);
		if (!dtop.empty() && dls > 0.0){
			const double f = (root_best_usi == dtop) ? o.tm_agree / 100.0
			               : (dbp >= (float)o.dl_conf_min ? o.tm_disagree / 100.0 : 1.0);
			s *= 1.0 + (f - 1.0) * dls;
		}
	}
	// (3) GP テント: 序盤で貯め、中盤 (GP=0.5) に厚く、GP≥1.0 (byoyomi 支配域) は中立。
	//     [TA-GAMMA] Pyfamate TA の進行度ファクタと同じ曲率ノブ γ を各脚に入れる。
	//     γ=1.0 (既定) で従来の線形テントと恒等。γ>1 で「終盤側で傾きが増す」(Qhapaq の
	//     変分配分: 深さの限界価値は終盤ほど高い) 形になる。
	{
		const double gpv = last_gp();
		const double e = o.tm_gp_early / 100.0, m = o.tm_gp_mid / 100.0;
		const double g = std::max(0.05, o.tm_gp_gamma / 100.0);
		if      (gpv <= 0.5) s *= e + (m - e) * std::pow(gpv / 0.5, g);
		else if (gpv <  1.0) s *= m + (1.0 - m) * std::pow((gpv - 0.5) / 0.5, g);
	}
	// (4) 形勢: |bestValue| が start 超で線形節約、2×start で floor 飽和 (mate/TB 圏は除外)
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
	// (5) [TA-MOMENTUM] Pyfamate TA ④b の移設。負 (悪化・詰めろ) は延長、正 (好調) は加速。
	//     加速ゲインを延長より小さく取るのも Pyfamate と同じ (勝勢の過小思考 < 劣勢の頓死)。
	{
		const double mm = last_momentum();
		if      (mm < 0.0) s *= 1.0 + (-mm) * (o.tm_momentum_ext / 100.0);
		else if (mm > 0.0) s *= 1.0 -   mm  * (o.tm_momentum_acc / 100.0);
	}
	// (6) [TA-SHARPNESS] Pyfamate TA ⑦ の移設。難しい局面は延長、明快な局面は短縮。
	//     DL 無しでも NNUE スコアの接近度で動く (compute_sharpness 参照)。
	const double sharp = o.tm_sharp_current
        ? current_sharpness(root_best_usi, best_cp, cp_ok && !mate_range) : last_sharpness();
    s *= (o.tm_sharp_base + o.tm_sharp_range * sharp) / 100.0;
	// (7) [TA-PONDER] 撤去: やねうら王側が USI_Ponder 有効時に optimum += optimum/4 で
	//     同等の延長を行っているため、ここでさらに掛けると二重計上になる。
	// [TA-CLAMP] Pyfamate の _TA_FAC_FLOOR / _TA_CEIL と同じ絶対境界。各因子は有界だが
	//   積には上下限が無く、勝勢局面では減衰因子が同時発火して過小思考になり得る。
	//   totalTime は本値に加え fallingEval/reduction/instability/effort も掛かるため、
	//   ここで積を締めておく (強さの主張ではなく事故防止)。
	if (s < CYF_TM_FLOOR) s = CYF_TM_FLOOR;
	else if (s > CYF_TM_CEIL) s = CYF_TM_CEIL;
	return s;
}

// [CSM] bestmove 確定後に呼ぶ: momentum / sharpness / 各 prev 値を更新する。
//   tesuki_n は呼び出し側が Zyfamate で測った値 (未測定なら -1)。
inline void post_move_metrics(int ply, int best_cp, bool cp_ok, bool dl_ok,
                              double dl_gap_p, bool agree, int tesuki_n,
                              const std::string& best_usi, const std::string& pred_usi){
	const double gp = last_gp();
	// 局面が飛んだ (新しい対局 / 検討でのジャンプ) なら履歴を捨てる。
	if (csm_last_ply() >= 0 && (ply <= csm_last_ply() || ply - csm_last_ply() > 4)){
		calm_streak() = 0; prev_best_cp_valid() = false; prev_gp() = -1.0; prev_pred_usi().clear();
	}
	// 前手の PV どおりに進んだ (= 前手が予測した自分の手をそのまま指した) なら落ち着いている。
	//   Pyfamate の council 合意連続数に相当する「揺れていない」信号。
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

// ============================================================================
// (3.7) Zyfamate df-pn 統合 ― MateTable(seqlock) + 専用 mate worker (§2/§3)。
//   worker(専用 std::thread, 外側1本)が root を Zyfamate::solve。証明を MateTable + root 結果へ
//   publish。探索スレッドは MateTable を seqlock read のみ（df-pn 起動は worker だけ §6）。
//   bench_guard 下では launch を抑止（発火タイミング依存の非決定性を遮断 §3）。
// ============================================================================
// Go-wide authority limit; each service job also has its own earlier expiry.
inline std::atomic<int64_t>& zyf_action_deadline(){ static std::atomic<int64_t> d{0}; return d; }
inline std::atomic<bool>& zyf_cancelled(){ static std::atomic<bool> c{false}; return c; }
inline std::atomic<int64_t>& zyf_ponderhit_ms(){ static std::atomic<int64_t> m{0}; return m; }   // go ponder: tm.maximum()
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
	// [ZYF-TREE 2026-10-05] Proof-tree move hints (Cyf_Zyf_Tree_Share). One bit per slot that holds a move,
	//   so a search node pays an L1/L2 bit test, not a table line, when nothing was harvested there.
	//   A hint is a fact about the position (Zyfamate proved this move mates, board and hands only) and acts only as
	//   move ordering, so it outlives its go: the next move's search still finds the proofs of the last one. Bits are
	//   cleared when sharing is off at go start or when half the slots are marked.
	std::vector<std::atomic<uint64_t>> hint_bits;
	std::atomic<uint32_t> hint_count{0};
	std::mutex hint_mx;   // serializes store_hint writers (seqlock writer side)
	MateTable(): tbl(SIZE), hint_bits(SIZE / 64) {}
	void begin(){   // mainthread, go start
		++gen; active = true;
		if (!options().zyf_tree_share || hint_count.load(std::memory_order_relaxed) > SIZE / 2) clear_hints(); }
	void disable(){ active = false; }
	// mate worker only。seqlock 書込。state:1=mate 2=nomate 3=tsumero。
	void store(uint64_t k, uint8_t st, int16_t ln, int64_t expiry){
		// Positive mate hints only. A hint has no score authority: each consumer re-proves it with NativeMateProof in
		// its actual history. nomate / unknown / board-cache negatives are never stored (they cannot act anyway).
		if (st != 1) return;
		Entry& e = tbl[(uint32_t)k & MASK];
		const uint32_t s = e.seq.load(std::memory_order_relaxed) + 1;
		e.seq.store(s, std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_release);
		e.key = k; e.state = st; e.len = ln; e.gen = gen; e.move = 0;
        e.expiry = expiry;   // explicit at every call site
		std::atomic_thread_fence(std::memory_order_release);
		e.seq.store(s + 1, std::memory_order_relaxed);
	}
	// [ZYF-TREE] Service publisher only (one writer). A proven OR node of a Zyfamate proof tree and the Move16 that
	//   Zyfamate's TT proves there. Like store(), the entry has no score authority: the search only tries the move
	//   first and has to find the mate itself.
	void store_hint(uint64_t k, uint16_t mv, int64_t expiry){
		if (!mv) return;
		std::lock_guard<std::mutex> lk(hint_mx);   // [ZYF-TREE-LIVE] two lanes may publish live at once
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
	// Any search thread. 0 = no hint for k. The generation and expiry are not checked (see above).
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
	// Main thread at go start (no search, the publisher paused).
	void clear_hints(){
		if (!hint_count.load(std::memory_order_relaxed)) return;
		for (auto& w : hint_bits) w.store(0, std::memory_order_relaxed);
		hint_count.store(0, std::memory_order_relaxed);
	}
	// any search thread read（seqlock）。state==0/世代不一致/torn は miss。
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
// [NPS 2026-10-09] Namespace scope instead of a function-local static: search() reads mate_table().hint_count at
//   every non-root node, and a function-local static costs a guard test per access. The table (about 3MB) is now
//   allocated at startup. No static initializer uses mate_table() (every user runs after main()), and it is
//   destroyed after the function-local statics that use it (mate_worker() etc.), which are constructed later.
inline MateTable mate_table_instance;
inline MateTable& mate_table(){ return mate_table_instance; }

// ============================================================================
// (3.8) HarvestProbe ― 反復深化の「再訪率」計装 (DL / GPU / 学習済み模型を使わない)
//
//   C1/C2 で書いて C0 から読む予算表 (harvest table) が成立するかは、「反復 d で
//   収穫した節点が、反復 d+1 で残り深さ ≥ X の *消費地点* に再び現れるか」だけで
//   決まる。模型の質も調整も、この一点より後の話。ゆえにここだけを先に測る。
//
//   計測地点は moves_loop: の 1 箇所のみ。ここへ到達した節点は必ず LMR の
//   reduction を計算する = 予算表の消費地点そのもの。search() 入口で数えると
//   TT cutoff / MateTable probe で move loop へ入らない再訪まで hit に数えてしまい、
//   再訪率が上振れする。
//
//   記録するイベントは 1 種類だけ (key, 時刻, rootDepth, depth, thread)。再訪率・
//   間隔 (gap) 分布・反復内順位はすべてオフラインで導出する。往復遅延 R と
//   スループット T は後から掃引できるので、GPU を 1 枚も回さずに
//       使える率(X, R, T) = 「gap ≥ R + 反復内順位 / T」を満たす再訪の割合
//   まで 1 回の計測走行から出せる。
//
//   lazy SMP では thread ごとに rootDepth がずれ、表は共有なので thread 内で閉じて
//   測ると過小評価になる。ゆえに thread 横断で 1 本のリングへ積む。
//
//   16 bytes/event 固定長。ディスク I/O は go 終了時の 1 回のみ (計測中に書くと
//   時刻分布そのものが歪む)。既定 off (Cyf_Probe_MinDepth = 0)。
// ============================================================================
struct HarvestProbe {
#pragma pack(push, 1)
	struct Ev {
		uint64_t key;    // pos.key()
		uint32_t t_us;   // go 開始からの経過 [us]
		uint16_t rd;     // その thread の rootDepth
		uint8_t  d;      // 残り深さ (255 飽和)
		uint8_t  th;     // threadIdx (255 飽和)
        uint16_t move;   // Move16; 0 at entry
        uint8_t kind;    // 0 entry after small ProbCut, 1 reducible quiet LMR
        uint8_t flags;   // 1 arrived, 2 policy-selected, 4 value-gated, 8 depth changed
        int16_t baseline, actual;
	};
#pragma pack(pop)
	static_assert(sizeof(Ev) == 24, "HarvestProbe v2 event is 24 bytes");

	std::vector<Ev>     ev;
	std::atomic<size_t> n{0};
	std::chrono::steady_clock::time_point t0;
	int         minDepth = 0;      // X。0 = 無効
	bool        on       = false;
	std::string outPath;

	// mainthread, go 冒頭。cap = 記録できるイベント数。
	void begin(int X, size_t cap){
		on = false;                       // 準備中は log() を止める
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

	// 全探索スレッドから呼ぶ。off のときは bool 1 本で即 return (無コスト)。
	inline void log(uint64_t k, int d, int rd, size_t th, uint16_t move=0, uint8_t kind=0, uint8_t flags=0, int baseline=0, int actual=0){
		if (!on || d < minDepth) return;
		const size_t i = n.fetch_add(1, std::memory_order_relaxed);
		if (i >= ev.size()) return;       // 溢れたら黙って捨てる (先頭側だけが有効)
		const auto dt = std::chrono::steady_clock::now() - t0;
		Ev& e  = ev[i];
		e.key  = k;
		e.t_us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(dt).count();
		e.rd   = (uint16_t)(rd < 0 ? 0 : (rd > 65535 ? 65535 : rd));
		e.d    = (uint8_t)(d > 255 ? 255 : (d < 0 ? 0 : d));
		e.th   = (uint8_t)(th > 255 ? 255 : th);
        e.move=move; e.kind=kind; e.flags=flags; e.baseline=int16_t(baseline); e.actual=int16_t(actual);
	}

	// mainthread, go 終了時に 1 回。追記 (1 局分の go が 1 ファイルに並ぶ)。
	void dump(){
		if (minDepth <= 0) return;
		const size_t raw = n.load(std::memory_order_relaxed);
		const size_t cnt = raw < ev.size() ? raw : ev.size();
		if (!cnt) return;
		FILE* f = std::fopen(outPath.c_str(), "ab");
		if (f){
			// 走行の区切り。解析側はこの magic レコードで go ごとに切る
			// (t_us は go ごとに 0 から数え直すため、追記された複数走行を混ぜられない)。
			Ev hdr{};
			hdr.key  = 0xC1FA0B0EC1FA0B02ULL;
			hdr.t_us = (uint32_t)cnt;            // この走行のイベント数
			hdr.rd   = 0xFFFFu;
			hdr.d    = (uint8_t)(minDepth > 255 ? 255 : minDepth);
			hdr.th   = 0xFFu;
            hdr.flags = raw > ev.size() ? 1 : 0; // incomplete trace: reject in replay
			std::fwrite(&hdr, sizeof(Ev), 1, f);
			std::fwrite(ev.data(), sizeof(Ev), cnt, f);
			std::fclose(f);
		}
		sync_cout << "info string Cyfamate probe X=" << minDepth
		          << " events=" << cnt << (raw > ev.size() ? " (overflow)" : "")
		          << " out=" << outPath << sync_endl;
	}
};
// [NPS 2026-10-08] Namespace scope instead of a function-local static: search() calls harvest_probe() at every
//   node and every quiet LMR move, and a function-local static costs a guard test per call. The object is
//   may be initialized dynamically at startup on some standard libraries (it holds a std::string), so no static
//   initializer may use harvest_probe(); none does today (every user runs after main()).
inline HarvestProbe harvest_probe_instance;
inline HarvestProbe& harvest_probe(){ return harvest_probe_instance; }

// [PARENT-INFO 2026-09-29] Structured information for a parent process (Pyfamate). Off by default:
// with Cyf_Parent_Info=false the USI output is unchanged. go_received counts every USI "go".
inline bool& parent_info_enabled() { static bool v = false; return v; }
inline std::atomic<uint64_t>& go_received() { static std::atomic<uint64_t> v{0}; return v; }
// Set by the UI thread while Stochastic_Ponder replays the saved "go" on ponderhit: that search
// answers the parent's earlier "go ponder", so it keeps that gen instead of taking a new one.
inline bool& replaying_go() { static bool v = false; return v; }
// True while the UI thread stops a Stochastic_Ponder search whose bestmove it discards.
inline std::atomic<bool>& bestmove_muted() { static std::atomic<bool> v{false}; return v; }
// The saved "go ponder ..." line with the clock a ponderhit carries (YaneuraOu extension
// "ponderhit btime B wtime W [binc I] [winc I] [byoyomi T]") substituted or appended, so the replayed search
// budgets from the clock at ponderhit, not the one at go ponder. Non-numeric or unknown tokens are ignored.
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


// The exact NNUE value used by every evaluator (plain or CNN-blended): a harvested resident-pool
// value when one exists for this position (CYF_GPU_POOL), otherwise the CPU NNUE. A blend applies
// its own rule on top; the pool never replaces the blended value itself.
inline Value exact_nnue(const Position& pos) {
    return Eval::evaluate(pos);
}
#if defined(CYF_EVAL_PROFILE)
// Diagnostic only: TSC cycles spent inside YaneuraOuWorker::evaluate during one go (Threads=1).
inline uint64_t& eval_cycles() { static uint64_t c = 0; return c; }
inline uint64_t& eval_calls() { static uint64_t c = 0; return c; }
inline uint64_t& go_tsc0() { static uint64_t c = 0; return c; }
#endif


// root 結果（bestmove 決定用, §2.3a）。worker が publish、mainthread が join 後に読む。
struct MateRootResult {
	std::atomic<int> state{0};   // 0 none / 1 mate / 2 nomate
	std::atomic<int> len{0};
    std::atomic<int64_t> expiry{0};
	std::atomic<bool> move_valid{false};
	char move_csa[16] = {0};     // mate 一手目(CSA)。state==1 && move_valid 時のみ有効
	void reset(){ state.store(0); len.store(0); expiry.store(zyf_action_deadline().load()); move_valid.store(false); move_csa[0]=0; }
};
inline MateRootResult& mate_root(){ static MateRootResult r; return r; }
#include "cyf_mate_validate.h"
struct ZyfVerifyStats {   // per go; reset with the verification quota
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

// The owner is joined before this is called. Advice selects only an allowed legal
// root move. Its score/PV stays the native search's; no mate value or cutoff is added.
Move csa_to_move(const Position& pos, const std::string& csa_in);
template<class RootMovesT>
inline bool play_zyf_root_hint(Position& p, RootMovesT& moves){
    auto& r=mate_root();
    if (!options().zyf_play_hint || !mate_table().active || r.state.load()!=1
        || !r.move_valid.load() || !zyf_action_live(r.expiry.load())) return false;
    // A mate the search already proved is kept: the solver's first proven check is not the shortest mate
    // (generated-tsume differential: 235/984 mate-in-1 roots got a mate-in-3+ move), and the swapped
    // move would report score cp 0 instead of the search's mate score.
    if(!moves.empty() && moves[0].score!=VALUE_NONE && is_win(moves[0].score)) return false;
    const Move m=csa_to_move(p,std::string(r.move_csa));
    if(m==Move::none() || !p.pseudo_legal_s<true>(m) || !p.legal(m)) return false;
    for(size_t i=0;i<moves.size();++i) if(!moves[i].pv.empty() && moves[i].pv[0]==m){
        std::swap(moves[0],moves[i]); ++zyf_verify_stats().use_root_hint; return true;
    }
    return false;
}

// [ZYF-HASH-LATCH] Zyfamate の表確保 (hash_autosize) は done フラグで初回 solve の 1 回のみ。
//   以後 Cyf_Mate_Hash を変えても効かないので、ラッチ済みかを持って setoption 時に警告する。
inline bool& zyf_hash_latched(){ static bool b=false; return b; }

// ============================================================================
// [ZYF-RESERVE] Zyfamate の資源は固定割当。Cyfamate 起動時に 2 スレッド / 2048MB を
//   確保し、プロセスが生きている間は解放も停止もしない (Cyfamate の起動と同期)。
//   Cyfamate 側は別枠で 14 スレッド / 6144MB (計 16 / 8192MB)。動的な受け渡しはしない。
//   df-pn を実際に回すのは GP≥0.5 の局面から終局まで (zyf_armed)。資源は回さない間も保持する。
// ============================================================================
// USI resets this flag while the search main thread may still be finishing.
// It gates work only; it does not publish any other data, so relaxed is enough.
inline std::atomic<bool>& zyf_armed(){ static std::atomic<bool> b{false}; return b; }
inline bool& zyf_reserved(){ static bool b=false; return b; }      // 起動時の表確保を済ませたか
constexpr double ZYF_GP_ONSET = 0.5;                               // df-pn を回し始める GP

#include "cyf_resources.h"
// One in-process serial solver, or an isolated process portfolio.
struct MateWorker {
	std::thread th;
    std::atomic<bool> cancelled{false};
    void run_portfolio(const std::string& sfen,uint64_t key,int count,long nodes,double seconds){
        // Same publication guard as run(): a child's positive must not reach mate_table/mate_root after stop,
        // gameover, a new go or the go's authority deadline.
        const int64_t expiry=zyf_action_deadline().load();
        auto live=[&](){ return !cancelled && !Zyfamate::g_stop && zyf_action_live(expiry); };
        if(!live())return;
        ZyfCpuGuard _cpu(count);   // [MT-SHARE] child processes
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
                // Only positive proofs are published from the portfolio.
                it=children.erase(it);
            }
            if(!children.empty())std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
	// §2.3b: mate_table の key は cyfamate 側 Position::key()（下位64bit）を用いる。Zyfamate::Pos::key は
	//   別 zobrist ドメインなので、探索の pos.key() probe と当たらない。launch 時に cyf_key を受け取り store。
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
        ZyfCpuGuard _cpu(std::max(1, nthreads));   // [MT-SHARE]
        Zyfamate::Pos p;
        if (!Zyfamate::parse_sfen(&p, sfen.c_str())) return;
        long nd = 0;
        const int r = nthreads > 1 ? Zyfamate::solve_parallel(sfen.c_str(),nthreads,nodelimit,remaining(),&nd)
                                  : Zyfamate::solve(&p,nodelimit,remaining(),&nd);
        CYF_ZYF_TEST_STAGE(1);
        if (!live()) return;
        if (r == 1){
            const double proof_budget = remaining();
            // [USER 2026-10-05] const int L = proof_budget > 0 ? Zyfamate::mate_len_proof(&p,99,proof_budget) : -1;
            const int L = -1; // distance intentionally unknown
            if (!live()) return;
            CyfZyf::SolveOut out{CyfZyf::kMate,int16_t(L > 0 ? L : 0),nd};
            if (remaining() > 0){
                Zyfamate::Pos q; Zyfamate::parse_sfen(&q,sfen.c_str()); Zyfamate::g_attacker=q.side;
                int fm=Zyfamate::first_mate_move(&q,1);
                if (fm >= 0) Zyfamate::move_csa(&q,fm,q.side,out.root_move_csa);
                else if (live() && remaining() > 0){
                    char pv[20000]{};
                    // [USER 2026-10-05] Zyfamate::print_pv(&q,L > 0 ? L : 1,pv,std::min(0.3,remaining()),1);
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
            zyf_thread_priority();   // [MT-SHARE]
            if(nthreads>1)run_portfolio(sfen,cyf_key,nthreads,nodelimit,tlimit);
            else run(sfen,cyf_key,1,nodelimit,tlimit);
        });
	}
	void stop_join(){ cancelled=true;if (th.joinable()){ Zyfamate::g_stop = 1; th.join(); } }
	~MateWorker(){ stop_join(); }
};
inline MateWorker& mate_worker(){ static MateWorker w; return w; }

// §2.3c MPON worker（ponder 中のみ起動。MateWorker とは呼出タイミングで排他＝非同時実行。専用 std::thread 1本）。
//   ② の policy top-k 各応手を適用した「候補適用後の自局面」を順次 df-pn し、証明できた局面を mate_table
//   へ store（①の probe が同一 ponder 探索中に拾う / 的中時は次手 root 並走が即 proven から始まる）。
//   Zyfamate global state は本 worker 内専有（MateWorker を stop_join した後にのみ launch する — 親仕様§3）。
struct MponWorker {
	std::thread th;
	void run(std::vector<std::pair<std::string,uint64_t>> jobs, long nodelimit, double tlimit_each){
		zyf_thread_priority();   // [MT-SHARE]
		ZyfCpuGuard _cpu;        // [MT-SHARE]
		for (auto& jb : jobs){
			if (Zyfamate::g_stop) break;               // stop 要求で打ち切り
			Zyfamate::Pos p;
			if (!Zyfamate::parse_sfen(&p, jb.first.c_str())) continue;
			// ⚠ ここで g_stop=0 をしてはならない。launch() が起動前に1度クリア済で、run() 内で g_stop を
			//   1 にするのは外部の stop_join() だけ。ループ内で 0 に戻すと stop 要求を取りこぼし（solve は
			//   g_stop を復元しないため）残ジョブ全部を満額 tlimit で回し、join() が数秒ブロックする。
			long nd = 0;
            const auto started = CyfZyf::now_us();
            int r = Zyfamate::solve(&p, nodelimit, tlimit_each, &nd);
            if (!zyf_action_live()) break;
			if (Zyfamate::g_stop) break;               // 打ち切られた solve の結果は採用しない
			if (r == 1){                               // 候補適用後の自局面で side-to-move（自分）が詰ませられる
                double left = std::max(0.0,tlimit_each-(CyfZyf::now_us()-started)/1e6);
                const auto hard=zyf_action_deadline().load();
                if(hard)left=std::min(left,std::max(0.0,(hard-CyfZyf::now_us())/1e6));
                // [USER 2026-10-05] int L = left > 0 ? Zyfamate::mate_len_proof(&p,99,left) : -1;
                int L = -1; // distance intentionally unknown
                if (Zyfamate::g_stop || !zyf_action_live()) break;   // [PHASE5] 再探索なし即答 (上記 MateWorker と同じ)
				mate_table().store(jb.second, 1, (int16_t)(L > 0 ? L : 0), zyf_action_deadline().load());
				if (options().verbose)
					sync_cout << "info string Cyfamate mpon proven key=" << jb.second
					          << " L=" << (L > 0 ? L : 0) << sync_endl;
			} else if (r == 0){
				// nomate: no table entry
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

// ============================================================================
// [ZY-01 2026-10-02] Cyf_Zyf_Service glue. One resident Zyfamate owner (cyf_zyf_core.h) takes the root at go start
//   and PV / root-candidate positions supplied by the main thread after completed iterations. Verdicts reach the
//   search through the MateTable (PV-node probe, plus non-PV nodes at ply<=2 while the service runs); every request
//   ends in a typed outcome reported after bestmove. Off (default), or with a portfolio budget (mate threads > 1),
//   nothing here runs and the original MateWorker / MponWorker paths are used unchanged.
// ============================================================================
// [ZYF-DEFENSE] Two Zyfamate threads run the service with a defense lane (Cyf_Zyf_Defense); otherwise they run the
//   legacy MateWorker as before.
inline int  zyf_lanes(){ return active_cpu_budget().mate == 2 && options().zyf_defense ? 2 : 1; }
inline bool zyf_service_active(){ return options().zyf_service && options().mate_enable
                                         && (active_cpu_budget().mate == 1 || zyf_lanes() == 2); }
inline bool zyf_defense_active(){ return zyf_service_active() && zyf_lanes() == 2; }
inline uint64_t& zyf_go_gen(){ static uint64_t g = 0; return g; }      // the go serial, set before on_root_mainthread
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
	int best_checks = 0;                       // [ZYF-TREE] candidate jobs for a new best move beyond Cyf_Zyf_Go_Jobs
	int defense_supplied = 0;                  // [ZYF-DEFENSE] candidate jobs sent to the defense lane
	std::string tonshi_from, tonshi_to, tonshi_alt;   // [ZYF-DEFENSE] bestmove switched away from a proven tonshi
	std::vector<uint16_t> cand_sent;           // root moves whose candidate job the service accepted this go
	long proof_hits0 = 0;
	std::vector<ZyfCand> cands;                // root candidates sent for an opponent-mate check
	std::vector<CyfZyf::Result> results;       // drained at iteration boundaries and after the search
};
inline ZyfGo& zyf_go(){ static ZyfGo g; return g; }
// [ZYF-TREE-SAFE 2026-10-06] Set when a candidate job of this go proved a root move safe (the opponent has no forced
//   mate after it). Read by search threads: with Cyf_Zyf_Tree_Bound, a root move that walks into a shared tonshi tree
//   is then ranked below every other move, so the search itself turns to the alternatives.
inline std::atomic<bool>& zyf_root_safe(){ static std::atomic<bool> b{false}; return b; }

// [ZYF-TREE 2026-10-05] Worker thread only, right after solve() proved sfen (Zyfamate's TT still holds the proof).
//   Walks the proof tree breadth-first: at each OR node the move first_mate_move() proves (mate in 1 first), at each
//   AND node every legal reply. Zyfamate's Pos carries the YaneuraOu key (ybk/yhk), so no engine Position is needed;
//   the search checks a hint move's legality where it reads it. This adds the OR nodes that mid() proved only through
//   the TT (or before the export started) to the ones it exported while solving.
//   Output: (search key, Move16) per OR node, the root included, capped by max_records and secs.
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
                int rep[1024]; const int n = Zyfamate::gen_legal(&zp, rep);   // Zyfamate MAXMOVES
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

// Worker thread only (the service owns Zyfamate while it is not paused).
inline CyfZyf::SolveOut zyf_solve_job(const CyfZyf::Job& j){
    ZyfCpuGuard _cpu;   // [MT-SHARE]
    zyf_thread_priority();
    Zyfamate::Pos p;
    if (!Zyfamate::parse_sfen(&p, j.sfen.c_str())) return {CyfZyf::kError, 0, 0};
    const int64_t started = CyfZyf::now_us();
    long nd = 0;
    const double solve_secs = j.deadline_us ? std::min(j.secs, (j.deadline_us-started)/1e6) : j.secs;
    if (solve_secs <= 0 || !zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,0};
    // [ZYF-TREE] mid() exports the OR nodes it proves (also below an unknown or disproved root).
    std::vector<std::pair<uint64_t,uint16_t>> tree;
    const bool share = options().zyf_tree_share;
    if (share){ Zyfamate::g_yexport = &tree; Zyfamate::g_yexport_cap = size_t(std::max(1, options().zyf_tree_max)) * 4; }
    // [ZYF-TREE-LIVE] Hints are position facts used for ordering (and, with Cyf_Zyf_Tree_Bound, as a mate lower bound),
    //   so they are published as soon as they are proven, before the job's verdict and deadline checks.
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
        // [ZYF-TREE] Read-only walk of the proof just found, before anything else uses the solver.
        if (share && remaining() > 0)
            zyf_harvest_tree(j.sfen, options().zyf_tree_max, std::min(remaining(), 0.02), tree);
        // The verdict survives a length timeout, but a finite distance is never invented.
        if (j.kind == CyfZyf::kRoot && options().zyf_play_hint && remaining() > 0){
            Zyfamate::Pos q; Zyfamate::parse_sfen(&q,j.sfen.c_str()); Zyfamate::g_attacker=q.side;
            const int fm=Zyfamate::first_mate_move(&q,1);
            if (fm>=0){
                CyfZyf::SolveOut out{CyfZyf::kMate,0,nd};
                Zyfamate::move_csa(&q,fm,q.side,out.root_move_csa);
                if(Zyfamate::g_stop) return {CyfZyf::kCancelled,0,nd};
                if(!zyf_action_live(j.deadline_us)) return {CyfZyf::kExpired,0,nd};
                out.tree = std::move(tree);
                return out; // unknown distance; move advice only
            }
        }
        const double proof_budget = remaining();
        // [USER 2026-10-05] const int L = proof_budget > 0 ? Zyfamate::mate_len_proof(&p, 99, proof_budget) : -1;
        const int L = -1; // distance intentionally unknown
        if (Zyfamate::g_stop) return {CyfZyf::kCancelled, 0, nd};
        CyfZyf::SolveOut out{CyfZyf::kMate, int16_t(L > 0 ? L : 0), nd};
        if (j.kind == CyfZyf::kRoot && remaining() > 0){
            Zyfamate::Pos q; Zyfamate::parse_sfen(&q, j.sfen.c_str());
            Zyfamate::g_attacker = q.side;
            const int fm = Zyfamate::first_mate_move(&q,1);
            if (fm >= 0) Zyfamate::move_csa(&q, fm, q.side, out.root_move_csa);
            else if (remaining() > 0){
                char pv[20000]{};
                // [USER 2026-10-05] Zyfamate::print_pv(&q, L > 0 ? L : 1, pv, std::min(0.3, remaining()),1);
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
		            [](const CyfZyf::Job& j, const CyfZyf::SolveOut& o){   // the single MateTable writer during a search
                        CYF_ZYF_TEST_STAGE(5);
                        if (!zyf_action_live(j.deadline_us)) return;
		                // Service has validated the generation, pause state and supplied deadline.
		                if (j.kind == CyfZyf::kRoot && CyfZyf::is_verdict(o.outcome)){   // unknown reaches here only with tree hints
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
		                if (mate) mate_table().store(j.key, 1, o.len, j.deadline_us);   // nomate: report only, no table entry
		                for (const auto& h : o.tree) mate_table().store_hint(h.first, h.second, j.deadline_us);   // bounded: 5 x Cyf_Zyf_Tree_Max
		            },
		            []{ Zyfamate::g_stop = 1; }, []{ Zyfamate::g_stop = 0; },
                    [](const CyfZyf::Job& j){ return zyf_action_live(j.deadline_us); });
		// [ZYF-DEFENSE] Lane 1 (defense) solves with its own Zyfamate context and tables; lane 0 keeps context 0.
		s.set_lane_init([](int lane){ Zyfamate::use_context(lane); zyf_thread_priority(); });   // [MT-SHARE]
		return true;
	}();
	(void)configured;
	return s;
}
// Main thread at go start, no search running, the MateTable already begun when armed.
// The same go serial again is a Stochastic_Ponder replay after ponderhit: the go continues (results, candidates,
// supply quota, MateTable generation and the core's queue/log are kept), only the root is solved again.
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
// The action deadline (authority to publish and to be consumed) is this go's time-management limit. The compute
// budget j.secs is separate and enforced by zyf_solve_job. 0 = no limit known yet (ponder/infinite): the
// generation and an explicit stop bound it; a regular ponderhit installs the limit (zyf_ponderhit_ms).
inline void zyf_job_deadline(CyfZyf::Job& j){
    j.supply_us = CyfZyf::now_us();
    j.deadline_us = zyf_action_deadline().load();
}
// The root keeps its original budget (nodes 1e9, time optimum x3) unless Cyf_Zyf_Root_* caps it for an experiment.
inline void zyf_submit_root(const Position& pos, double legacy_tl){
	auto& o = options(); auto& g = zyf_go();
	CyfZyf::Job j; j.key = uint64_t(pos.key()); j.gen = g.gen; j.sfen = pos.sfen(); j.kind = CyfZyf::kRoot; j.source = CyfZyf::kSrcRoot;
	j.stm = pos.side_to_move() == BLACK ? 0 : 1; j.priority = 1e9;
	j.nodes = o.zyf_root_nodes > 0 ? long(o.zyf_root_nodes) : 1000000000L;
	// Keep the first root request finite so the same owner can solve queued PV/candidates.
	// [ZYF-FIX 1] With the defense lane, candidates run on lane 1, so the root keeps its legacy budget.
	j.secs = o.zyf_root_ms > 0 ? std::min(legacy_tl, o.zyf_root_ms / 1000.0)
	       : zyf_lanes() == 2 ? legacy_tl
	                          : std::min(legacy_tl, std::min(std::max(1, o.zyf_job_ms) / 1000.0, std::max(0.001, legacy_tl / 12.0)));
	zyf_job_deadline(j);
	if (g.replays) zyf_service().forget(j.key);   // the replay needs its own root proof and first move
	g.root_job = zyf_service().submit(std::move(j)) || g.root_job;
}
inline std::string zyf_value_str(Value v){
	if (v == -VALUE_INFINITE || v == VALUE_INFINITE || v == VALUE_NONE) return "none";
	if (std::abs(v) < VALUE_MATE_IN_MAX_PLY) return "cp:" + std::to_string(USIEngine::to_cp(v));
	return std::string("mate:") + std::to_string(v > 0 ? int(VALUE_MATE - v) : -int(VALUE_MATE + v));
}

// Main thread after a completed iteration: take finished results, then supply the NNUE PV (plies 2..N, side to move
// as attacker) and root candidates (the opponent as attacker after our move: NNUE top-N plus the parent's Cyfnet
// top-N of this go). Priorities lean on the NNUE score sign and on where Cyfnet rates a candidate below the NNUE best.
template<class RootMovesT>
inline void zyf_iteration(Position& rootPos, const RootMovesT& rootMoves, int depth){
	auto& g = zyf_go(); auto& s = zyf_service(); auto& o = options();
	const int64_t tc = CyfZyf::now_us();
	const size_t before = g.results.size();
	s.drain(g.results);
	g.drained_in_search += int(g.results.size() - before);
	g.collect_us += uint64_t(CyfZyf::now_us() - tc);
	if (!g.armed || depth < o.zyf_min_depth || rootMoves.empty() || rootMoves[0].pv.empty()) return;
	// [ZYF-DEFENSE] With a defense lane, candidate jobs (the opponent attacking after a root move) go to that lane
	//   with their own count (Cyf_Zyf_Defense_Jobs) and budget (Cyf_Zyf_Defense_Ms/Nodes); PV jobs keep Cyf_Zyf_Go_Jobs.
	const bool defense = zyf_defense_active();
	// [ZYF-TREE 2026-10-05] The move about to be played gets its opponent-mate check even after the job cap: with
	//   tree sharing its proof reaches the search as hints below that move, so the search itself sees the mate.
	//   B2 corpus (runs before this): 15 of 34 moves that allowed mate were never a candidate.
	//   With a defense lane the current best move is always checked first (its verdict can change the move).
	if (defense ? g.defense_supplied < o.zyf_defense_jobs
	            : (o.zyf_tree_share && g.supplied >= o.zyf_go_jobs && g.best_checks < std::max(1, o.zyf_go_jobs))){
		const Move m = rootMoves[0].pv[0];
		const bool known = std::find(g.cand_sent.begin(), g.cand_sent.end(), uint16_t(m.raw())) != g.cand_sent.end();
		if (!known && rootPos.pseudo_legal_s<true>(m) && rootPos.legal(m)){
			ZyfCand c; c.move = uint16_t(m.raw()); c.nnue_rank = 0; c.source = CyfZyf::kSrcNnue; c.depth = depth; c.priority = 3.0;
			CyfZyf::Job j;   // like make(): the job is the position after the move, the opponent attacking
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
	g.hint = false;
	auto cyf_of = [&](uint16_t, int32_t&, int&){ return false; };
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
	// (1) NNUE PV positions. Ply 1 is the best move's child, handled as a candidate below.
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
	// (2) Root candidates: can the opponent force mate after this move?
	std::vector<size_t> idx;                         // indices into rootMoves
	std::vector<uint8_t> src;
	auto add = [&](size_t r, uint8_t source){
		for (size_t i = 0; i < idx.size(); ++i) if (idx[i] == r){ if (src[i] != source) src[i] = CyfZyf::kSrcBoth; return; }
		idx.push_back(r); src.push_back(source);
	};
	const int n = std::max(0, o.zyf_cands);
	for (int i = 0; i < n && size_t(i) < rootMoves.size(); ++i) add(size_t(i), CyfZyf::kSrcNnue);
	int32_t best_cyf = 0; int best_rank = -1;
	const bool best_has = cyf_of(best_raw, best_cyf, best_rank);
	for (size_t i = 0; i < idx.size(); ++i){
		const Move m = rootMoves[idx[i]].pv[0];
		if (!rootPos.pseudo_legal_s<true>(m) || !rootPos.legal(m)){ ++g.supply_illegal; continue; }
		ZyfCand c; c.move = uint16_t(m.raw()); c.nnue_rank = int(idx[i]); c.source = src[i]; c.depth = depth;
		c.has_cyfnet = cyf_of(c.move, c.cyfnet, c.cyf_rank);
		double prio = 1.5 + (idx[i] == 0 ? 0.2 : 0.0);
		if (c.has_cyfnet && best_has && c.cyfnet < best_cyf) prio += 0.4;   // Cyfnet rates it below the NNUE best
		if (c.source == CyfZyf::kSrcCyfnet) prio += 0.3;                    // only Cyfnet put it in the top-N
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

// After bestmove (the service was paused and its queue expired before the bestmove decision).
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

// §1.3 MultiPonder prefetch: bestmove 送出後に fire-and-forget post した policy id を保持し、次 go 冒頭で
//   abandon 一括処理する（P0 契約：post した id は必ず try_get/wait/abandon のいずれかで始末すること）。
inline std::vector<uint32_t>& multiponder_pending(){ static std::vector<uint32_t> v; return v; }

Move csa_to_move(const Position& pos, const std::string& csa_in);   // 定義は後方。Zyfamate df-pn の CSA 手 → Move 変換


// Cyf_Root_Hint (cyf_root_hint.h): take a parent hint only at an iteration boundary. The hint must
// belong to this go (gen), its moves must be root moves; apply=1 publishes one ordering table per go.

// Main search thread only. Validate result identity against the actual legal root
// child, then publish an immutable table for all search threads. A cancelled,
// expired, unknown, old-go, excluded or mismatched result cannot imply safety.
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
        if (CyfZyf::is_mate(r.outcome)) return 1; // conflicting proof never becomes safe
        if (r.outcome==CyfZyf::kNomate || r.outcome==CyfZyf::kCachedNomate) verdict=0;
    }
    return verdict;
}
template<class RootMovesT>
inline void zyf_safe_refresh(Position& rootPos, const RootMovesT& rootMoves) {
    const auto& g=zyf_go();
    if (!g.active || !g.armed || g.results.empty()) return;
    std::vector<CyfDefense::Candidate> candidates;
    for (size_t i=0; i<rootMoves.size(); ++i) {
        if (rootMoves[i].pv.empty()) continue;
        CyfDefense::Candidate c;
        c.move=uint16_t(rootMoves[i].pv[0].raw()); c.search_rank=int(i);
        c.verdict=zyf_cand_verdict(rootPos, rootMoves[i]);
        if (c.verdict!=0 && c.verdict!=1) continue;   // [ZYF-FIX 2] keep proven-mated moves for the override test
        candidates.push_back(c);
    }
    zyf_root_safe().store(std::any_of(candidates.begin(), candidates.end(), [](const auto& c){ return c.verdict==0; }),
                          std::memory_order_relaxed);
    CyfDefense::publish(g.gen, uint64_t(rootPos.key()), std::move(candidates));
}


// ============================================================================
// (4)融合フック ― mainthread の root で1回呼ぶ。
//     GP算出 → (DL有効なら)DL policy取得 → last_dl_* へ保存 → info。
// ============================================================================
template<class RootMovesT>
inline void on_root_mainthread(Position& pos, RootMovesT& rootMoves, bool ponder){
	{	// [PROBE] harvest 再訪率の計装。Cyfamate 融合の on/off とは独立に効かせる。
		//   bench では回さない (発火タイミング依存の非決定性と署名不一致を避ける §4)。
		Options& po = options();
		if (bench_guard()) harvest_probe().disable();
		else harvest_probe().begin(po.probe_min_depth,
		                           (size_t)(po.probe_cap_k > 0 ? po.probe_cap_k : 0) * 1024);
	}
	Options& o = options();
	const double gp  = game_phase(pos);
	const double dls = dl_policy_scale(gp, o.gp_onset, o.gp_floor);
	last_gp() = gp;   // [2026-07-17] TM の GP 因子 (dlGpScale) が DL 無しでも参照するため無条件更新
	                  //   (従来は DL query 成功時のみ更新で、DL 無効時に stale 値が残った)

	bool dl_active=false;
	std::string dl_top; int dl_topp=0;
	if (bench_guard()){ last_dl_valid()=false; return; }   // §4: bench 中は DL 完全バイパス（sidecar start/post/prior を発火させない）
	// Zyfamate df-pn: go ごとに root 局面を専用 mate worker で並走 df-pn（§2.3a）。bench は上で return 済。
	if (o.mate_enable && zyf_armed().load(std::memory_order_relaxed) && active_cpu_budget().mate>0){
		// Cyf_Mate_Hash → Zyfamate hash_autosize（env 経由。初回 solve の *_init で 1 度だけ確保・以後再利用）。
		{ char _hb[24]; std::snprintf(_hb, sizeof _hb, "%d", std::max(16, o.mate_hash_mb));
#if defined(_WIN32)
		  _putenv_s("ZYFAMATE_HASH_MB", _hb);
#else
		  setenv("ZYFAMATE_HASH_MB", _hb, 1);
#endif
		}
		mate_worker().stop_join(); mpon_worker().stop_join();
		if (zyf_service_active()) zyf_service().pause_join();   // ZY-01: idle before the table generation moves
		if (!(zyf_service_active() && zyf_same_go())) mate_table().begin();   // a ponderhit replay keeps this go's proofs
		mate_root().reset();
		// root 並走 df-pn（MateWorker, cyf_key で store）。ponder 中も起動する（ponderhit 時にそのまま
		//   継続できる。ponder miss で stop した場合は次の go 冒頭で stop_join される）。
		{
			const double _hint_ms = (double)tm_optimum_hint();
			// ponder 中は hint=0（無制限）→ 既定 60 秒。通常 go は optimum×3 で上限 60 秒。
			const double _tl = _hint_ms > 0.0 ? std::min(60.0, std::max(0.05, _hint_ms * 3.0 / 1000.0)) : 60.0;
			if (zyf_service_active()){ zyf_go_begin(true); zyf_submit_root(pos, _tl); }   // ZY-01: same budget, one owner
			else mate_worker().launch(pos.sfen(), (uint64_t)pos.key(), active_cpu_budget().mate, 1000000000L, _tl);
		}
	}
	else if (zyf_service_active()) zyf_go_begin(false);   // ZY-01: not armed yet; the go is still reported

	if (!o.enable) { last_dl_valid()=false; last_dl_entropy_valid()=false; return; }
	last_dl_valid()=false;

	if (o.verbose){
		auto& os = sync_cout;
		os << "info string Cyfamate GP=" << (int)(gp*100)
		   << " phase=" << phase_label(gp)
		   << " dl_scale=" << (int)(dls*100)
		   << " roots=" << (int)rootMoves.size();
		if (dl_active){ os << " dl=on top=" << dl_top << " p=" << dl_topp
		                   << " ent=" << (int)(last_dl_entropy()*100); }
		else           os << " dl=off";
		os << sync_endl;
	}

}

} // namespace Cyfamate
} // namespace YaneuraOu
#endif // CYFAMATE_H_INCLUDED