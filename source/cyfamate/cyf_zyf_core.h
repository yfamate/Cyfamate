#pragma once
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
    uint8_t kind = kPv, source = kSrcNnuePv, stm = 0;  
    int8_t ply = 0;                 
    uint16_t root_move = 0;         
    int16_t depth = 0;              
    int32_t cyfnet = 0;             
    bool has_cyfnet = false;
    double priority = 0.0;
    long nodes = 0;                 
    double secs = 0.0;
    int64_t supply_us = 0, deadline_us = 0;
};
struct SolveOut {
    uint8_t outcome = kError; int16_t len = 0; long nodes = 0;
    char root_move_csa[16] = {}; 
    std::vector<std::pair<uint64_t, uint16_t>> tree; 
};
struct Result {
    Job job;                        
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
    void set_lane_init(LaneInitFn f) { std::lock_guard<std::mutex> lk(m_); lane_init_ = std::move(f); }
    bool running() const { std::lock_guard<std::mutex> lk(m_); return !th_.empty(); }
    int lanes() const { std::lock_guard<std::mutex> lk(m_); return th_.empty() ? 0 : lanes_; }
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
    void begin_go(uint64_t gen) {
        pause_join();
        std::lock_guard<std::mutex> lk(m_);
        if (gen != gen_) {
            for (auto& j : q_) log_locked(j, kExpired, 0, 0, 0, 0);   
            q_.clear(); log_.clear(); drained_ = 0; go_ = GoCounters{}; go_keys_.clear(); gen_ = gen;
        }
        paused_ = false;
        cv_.notify_all();
    }
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
        cv_.notify_all();   
        return true;
    }
    void forget(uint64_t key) { std::lock_guard<std::mutex> lk(m_); go_keys_.erase(key); }
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
        if (outcome != kMate && outcome != kNomate) return; 
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
                    const bool current = publish_current_locked(j, o);
                    --busy_;
                    log_locked(j, current ? o.outcome : uint8_t(kExpired), current ? o.len : 0, 0, t0, now_us());
                    idle_cv_.notify_all();
                    continue;
                }
            }
            ++busy_;
            if (arm_ && busy_ == 1) arm_();   
            lk.unlock();
            SolveOut o = solve_ ? solve_(j) : SolveOut{};
            lk.lock();
            bool current = true;
            if (is_verdict(o.outcome)) current = publish_current_locked(j, o);
            else if (!o.tree.empty()) publish_current_locked(j, o);   
            const int64_t t1 = now_us();
            --busy_;
            ++go_.solves; ++solves_total_; if (lane == 1) ++go_.lane1_solves;
            go_.solve_us += uint64_t(std::max<int64_t>(0, t1 - t0));
            go_.solve_nodes += uint64_t(std::max(0L, o.nodes));
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
    int busy_ = 0;   
    bool paused_ = true, quit_ = false;
};

} 
