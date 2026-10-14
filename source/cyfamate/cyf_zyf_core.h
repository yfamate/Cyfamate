#pragma once
// Cyf_Zyf_Service core (ZY-01; USI option Cyf_Zyf_Service). No engine types here, so the
// queue/lifetime rules can be tested with a stub solver (tests/test_zyf_core.cpp).
//
// Contract
//  * One resident worker thread is the only caller of the solver while the service is not paused. It takes the
//    highest-priority job of the running go, consults the board cache, otherwise runs the solve callback, publishes
//    final verdicts through the publish callback (the MateTable in the engine) and appends a typed Result to the
//    go's log. Unknown is never turned into nomate: the outcome keeps why the request ended.
//  * Producers call submit() from the engine's main thread (go start and completed iterations). A job carries the
//    go serial (gen); submit() rejects another gen, a key already supplied in this go, and a full queue the job
//    cannot outrank (an outranked queued job is logged kExpired). submit() never waits for the worker.
//  * pause_join() interrupts a running request through the stop callback and returns only when the worker is idle;
//    it stays paused until begin_go(). The engine calls it before it runs Zyfamate itself (tonshi guard, tesuki) or
//    writes the MateTable after a search, so the solver always has a single owner.
//  * end_go() logs queued-but-unstarted jobs as kExpired. begin_go(gen) starts a new go log and unpauses; the same
//    gen again (Stochastic_Ponder replay of one go) keeps the queue and the log.
//  * Final solver verdicts may be cached; unknown is retained in the log only.
//  * A non-verdict with SolveOut::tree (proven subtrees, Cyf_Zyf_Tree_Share) is still published, under the same
//    guard, so PublishFn can store those hints; it must not treat that outcome as a verdict.
//    Cross-go unknown suppression would need resumable work and a proved retry contract.
//    History suitability is not certified here. Root jobs always run the solver.
//  * PublishFn runs under the service mutex and must be bounded/non-reentrant.
//    It is the sole place for engine publication; SolveFn must only return local state.
//  * Lanes (Cyf_Zyf_Defense): start(2) runs two workers. Lane 0 takes root/PV/ponder jobs, lane 1 only candidate
//    jobs (the opponent attacking after one of our root moves). With one lane, that lane takes every kind. Each lane
//    is the only caller of the solver for its own solver context (LaneInitFn runs first on the lane's thread); the
//    pause, stop, queue, cache, log and publication rules above hold for both lanes together.
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CyfZyf {

enum Kind : uint8_t { kRoot = 0, kPv = 1, kCand = 2, kPonder = 3 };
enum Source : uint8_t { kSrcRoot = 0, kSrcNnuePv = 1, kSrcNnue = 2, kSrcCyfnet = 3, kSrcBoth = 4, kSrcMpon = 5 };
enum Outcome : uint8_t {
    kNone = 0, kMate, kNomate, kUnknownNodes, kUnknownTime, kUnknownOther, kCancelled, kError,
    kExpired, kCachedMate, kCachedNomate, kCachedUnknown, kOutcomeCount
};

inline const char* kind_name(uint8_t k) { static const char* n[] = {"root", "pv", "cand", "ponder"}; return k < 4 ? n[k] : "?"; }
inline const char* source_name(uint8_t s) {
    static const char* n[] = {"root", "nnue_pv", "nnue", "cyfnet", "both", "mpon"};
    return s < 6 ? n[s] : "?";
}
inline const char* outcome_name(uint8_t o) {
    static const char* n[] = {"none", "mate", "nomate", "unknown_nodes", "unknown_time", "unknown_other", "cancelled",
                              "error", "expired", "cached_mate", "cached_nomate", "cached_unknown"};
    return o < kOutcomeCount ? n[o] : "?";
}
inline bool is_verdict(uint8_t o) { return o == kMate || o == kNomate || o == kCachedMate || o == kCachedNomate; }
inline bool is_mate(uint8_t o) { return o == kMate || o == kCachedMate; }
inline int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Job {
    uint64_t key = 0, gen = 0, serial = 0;
    std::string sfen;
    uint8_t kind = kPv, source = kSrcNnuePv, stm = 0;  // stm: side to move = the attacker Zyfamate tries to mate with
    int8_t ply = 0;                 // plies from the go's root along the job's origin (0 = the root itself)
    uint16_t root_move = 0;         // raw Move16 of the root move the position descends from (0 = the root)
    int16_t depth = 0;              // completed iteration depth when supplied
    int32_t cyfnet = 0;             // parent hint score of root_move (root side's view); valid if has_cyfnet
    bool has_cyfnet = false;
    double priority = 0.0;
    long nodes = 0;                 // request limits
    double secs = 0.0;
    int64_t supply_us = 0, deadline_us = 0;
};
struct SolveOut {
    uint8_t outcome = kError; int16_t len = 0; long nodes = 0;
    char root_move_csa[16] = {}; // local result; root publication belongs to PublishFn
    std::vector<std::pair<uint64_t, uint16_t>> tree; // optional proof-tree (key, move) hints; PublishFn decides
};
struct Result {
    Job job;                        // sfen is dropped when logged
    uint8_t outcome = kNone;
    int16_t len = 0;
    long nodes = 0;
    int64_t start_us = 0, end_us = 0;
};
struct GoCounters {
    uint64_t submitted = 0, accepted = 0, dup_go = 0, stale_gen = 0, queue_full = 0, replaced = 0, solves = 0;
    uint64_t outcomes[kOutcomeCount] = {};
    uint64_t solve_us = 0, solve_nodes = 0, max_wait_us = 0, log_dropped = 0, lane1_solves = 0;
};

class Service {
public:
    static constexpr size_t kQueueCap = 16, kLogCap = 512, kCacheCap = 8192;
    using SolveFn = std::function<SolveOut(const Job&)>;
    using PublishFn = std::function<void(const Job&, const SolveOut&)>;
    using PublishGuardFn = std::function<bool(const Job&)>;
    using SignalFn = std::function<void()>;
    using LaneInitFn = std::function<void(int)>;

    ~Service() { shutdown(); }

    void configure(SolveFn solve, PublishFn publish, SignalFn stop, SignalFn arm, PublishGuardFn guard = {}) {
        std::lock_guard<std::mutex> lk(m_);
        solve_ = std::move(solve); publish_ = std::move(publish); stop_ = std::move(stop); arm_ = std::move(arm); guard_ = std::move(guard);
    }
    // Called on each lane's thread before its first job (e.g. to select the lane's solver context).
    void set_lane_init(LaneInitFn f) { std::lock_guard<std::mutex> lk(m_); lane_init_ = std::move(f); }
    bool running() const { std::lock_guard<std::mutex> lk(m_); return !th_.empty(); }
    int lanes() const { std::lock_guard<std::mutex> lk(m_); return th_.empty() ? 0 : lanes_; }
    // Starts paused with `lanes` workers (1 or 2). A running service with another lane count is shut down first.
    void start(int lanes = 1) {
        lanes = lanes == 2 ? 2 : 1;
        {
            std::lock_guard<std::mutex> lk(m_);
            if (!th_.empty() && lanes_ == lanes) return;
        }
        shutdown();
        std::lock_guard<std::mutex> lk(m_);
        quit_ = false; paused_ = true; lanes_ = lanes;
        for (int i = 0; i < lanes; ++i) th_.emplace_back([this, i] { loop(i); });
    }
    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_);
            if (th_.empty()) return;
            quit_ = true; paused_ = true;
            if (busy_ && stop_) stop_();
        }
        cv_.notify_all();
        for (auto& t : th_) t.join();
        std::lock_guard<std::mutex> lk(m_);
        th_.clear();
    }
    // Main thread, no search running. A new gen starts a fresh go log; the same gen keeps queue and log.
    void begin_go(uint64_t gen) {
        pause_join();
        std::lock_guard<std::mutex> lk(m_);
        if (gen != gen_) {
            for (auto& j : q_) log_locked(j, kExpired, 0, 0, 0, 0);   // normally empty: end_go() ran
            q_.clear(); log_.clear(); drained_ = 0; go_ = GoCounters{}; go_keys_.clear(); gen_ = gen;
        }
        paused_ = false;
        cv_.notify_all();
    }
    // Interrupt the running request and wait until the worker is idle. Stays paused until begin_go().
    void pause_join() {
        std::unique_lock<std::mutex> lk(m_);
        paused_ = true;
        if (busy_ && stop_) stop_();
        idle_cv_.wait(lk, [&] { return !busy_; });
    }
    void end_go() {
        std::lock_guard<std::mutex> lk(m_);
        for (auto& j : q_) log_locked(j, kExpired, 0, 0, 0, 0);
        q_.clear();
    }
    bool submit(Job j) {
        std::lock_guard<std::mutex> lk(m_);
        ++go_.submitted;
        if (j.gen != gen_) { ++go_.stale_gen; return false; }
        if (!go_keys_.insert(j.key).second) { ++go_.dup_go; return false; }
        j.serial = ++serial_;
        if (!j.supply_us) j.supply_us = now_us();
        if (q_.size() >= kQueueCap) {
            auto lo = std::min_element(q_.begin(), q_.end(), [](const Job& a, const Job& b) { return a.priority < b.priority; });
            if (lo->priority >= j.priority) { ++go_.queue_full; go_keys_.erase(j.key); return false; }
            ++go_.replaced; log_locked(*lo, kExpired, 0, 0, 0, 0); go_keys_.erase(lo->key); q_.erase(lo);
        }
        q_.push_back(std::move(j));
        ++go_.accepted;
        cv_.notify_all();   // lanes wait for different job kinds
        return true;
    }
    // Let a key of this go be supplied again (a Stochastic_Ponder replay re-solves its root).
    void forget(uint64_t key) { std::lock_guard<std::mutex> lk(m_); go_keys_.erase(key); }
    // Results logged since the previous drain (main thread).
    size_t drain(std::vector<Result>& out) {
        std::lock_guard<std::mutex> lk(m_);
        const size_t n = log_.size() - drained_;
        out.insert(out.end(), log_.begin() + std::ptrdiff_t(drained_), log_.end());
        drained_ = log_.size();
        return n;
    }
    std::vector<Result> log() const { std::lock_guard<std::mutex> lk(m_); return log_; }
    GoCounters counters() const { std::lock_guard<std::mutex> lk(m_); return go_; }
    size_t queued() const { std::lock_guard<std::mutex> lk(m_); return q_.size(); }
    size_t cache_size() const { std::lock_guard<std::mutex> lk(m_); return cache_.size(); }
    uint64_t solves_total() const { std::lock_guard<std::mutex> lk(m_); return solves_total_; }
    uint64_t gen() const { std::lock_guard<std::mutex> lk(m_); return gen_; }

private:
    struct CacheEntry { uint8_t verdict = kNone; int16_t len = 0; };

    void log_locked(const Job& j, uint8_t outcome, int16_t len, long nodes, int64_t t0, int64_t t1) {
        ++go_.outcomes[outcome < kOutcomeCount ? outcome : kError];
        if (log_.size() >= kLogCap) { ++go_.log_dropped; return; }
        Result r; r.job = j; r.job.sfen.clear(); r.outcome = outcome; r.len = len; r.nodes = nodes; r.start_us = t0; r.end_us = t1;
        log_.push_back(std::move(r));
    }
    void cache_put_locked(uint64_t key, uint8_t outcome, int16_t len) {
        if (outcome != kMate && outcome != kNomate) return; // unknown is not completed work
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            if (cache_.size() >= kCacheCap) { cache_.erase(cache_order_.front()); cache_order_.pop_front(); }
            it = cache_.emplace(key, CacheEntry{}).first;
            cache_order_.push_back(key);
        }
        if (outcome == kMate || outcome == kNomate) { it->second.verdict = outcome; it->second.len = len; }
    }
    bool publish_current_locked(const Job& j, const SolveOut& o) {
        if (paused_ || quit_ || j.gen != gen_ || (j.deadline_us && now_us() >= j.deadline_us)) return false;
        // Re-read dynamic engine stop/ponderhit deadline under the publication lock.
        if (guard_ && !guard_(j)) return false;
        if (publish_) publish_(j, o);
        return true;
    }
    bool takes_locked(int lane, const Job& j) const { return lanes_ == 1 || ((j.kind == kCand) == (lane == 1)); }
    std::vector<Job>::iterator next_locked(int lane) {
        auto best = q_.end();
        for (auto it = q_.begin(); it != q_.end(); ++it) {
            if (!takes_locked(lane, *it)) continue;
            if (best == q_.end() || best->priority < it->priority || (best->priority == it->priority && best->serial > it->serial)) best = it;
        }
        return best;
    }
    void loop(int lane) {
        LaneInitFn init;
        { std::lock_guard<std::mutex> lk(m_); init = lane_init_; }
        if (init) init(lane);
        std::unique_lock<std::mutex> lk(m_);
        for (;;) {
            cv_.wait(lk, [&] { return quit_ || (!paused_ && next_locked(lane) != q_.end()); });
            if (quit_) break;
            auto it = next_locked(lane);
            Job j = std::move(*it);
            q_.erase(it);
            const int64_t t0 = now_us();
            go_.max_wait_us = std::max<uint64_t>(go_.max_wait_us, uint64_t(std::max<int64_t>(0, t0 - j.supply_us)));
            if (j.deadline_us && t0 >= j.deadline_us) { log_locked(j, kExpired, 0, 0, t0, t0); continue; }
            if (j.kind != kRoot) {
                auto c = cache_.find(j.key);
                if (c != cache_.end() && (c->second.verdict == kMate || c->second.verdict == kNomate)) {
                    const SolveOut o{uint8_t(c->second.verdict == kMate ? kCachedMate : kCachedNomate), c->second.len, 0};
                    ++busy_;
                    // Publication is serialized with pause_join(). Callback must be
                    // bounded and must not call Service methods.
                    const bool current = publish_current_locked(j, o);
                    --busy_;
                    log_locked(j, current ? o.outcome : uint8_t(kExpired), current ? o.len : 0, 0, t0, now_us());
                    idle_cv_.notify_all();
                    continue;
                }
            }
            ++busy_;
            if (arm_ && busy_ == 1) arm_();   // clears an old interrupt under the lock (the first busy lane only); a later pause_join() sets it again
            lk.unlock();
            SolveOut o = solve_ ? solve_(j) : SolveOut{};
            lk.lock();
            bool current = true;
            if (is_verdict(o.outcome)) current = publish_current_locked(j, o);
            else if (!o.tree.empty()) publish_current_locked(j, o);   // proven-subtree hints only; still logged unknown
            const int64_t t1 = now_us();
            --busy_;
            ++go_.solves; ++solves_total_; if (lane == 1) ++go_.lane1_solves;
            go_.solve_us += uint64_t(std::max<int64_t>(0, t1 - t0));
            go_.solve_nodes += uint64_t(std::max(0L, o.nodes));
            // A solver-local verdict may remain cached; request expiry only removes
            // its authority to act on this go. History suitability remains separate.
            cache_put_locked(j.key, o.outcome, o.len);
            log_locked(j, current ? o.outcome : uint8_t(kExpired), current ? o.len : 0, o.nodes, t0, t1);
            idle_cv_.notify_all();
        }
        idle_cv_.notify_all();
    }

    mutable std::mutex m_;
    std::condition_variable cv_, idle_cv_;
    std::vector<std::thread> th_;
    int lanes_ = 1;
    SolveFn solve_; PublishFn publish_; SignalFn stop_, arm_; PublishGuardFn guard_; LaneInitFn lane_init_;
    std::vector<Job> q_;
    std::vector<Result> log_;
    size_t drained_ = 0;
    std::unordered_set<uint64_t> go_keys_;
    std::unordered_map<uint64_t, CacheEntry> cache_;
    std::deque<uint64_t> cache_order_;
    GoCounters go_;
    uint64_t gen_ = 0, serial_ = 0, solves_total_ = 0;
    int busy_ = 0;   // lanes inside a solve or a cached publication
    bool paused_ = true, quit_ = false;
};

} // namespace CyfZyf
