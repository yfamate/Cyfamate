#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace CyfAutoHint {

inline int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

enum class Final : int { none, published, terminated, expired, epoch_changed, gen_changed, key_changed, duplicate,
                         run_error, snapshot_error, shutdown, dropped_unstarted, replaced };
inline const char* name(Final f) {
    switch (f) {
    case Final::none: return "none";               case Final::published: return "published";
    case Final::terminated: return "terminated";   case Final::expired: return "expired";
    case Final::epoch_changed: return "epoch_changed"; case Final::gen_changed: return "gen_changed";
    case Final::key_changed: return "key_changed"; case Final::duplicate: return "duplicate";
    case Final::run_error: return "run_error";     case Final::snapshot_error: return "snapshot_error";
    case Final::shutdown: return "shutdown";       case Final::dropped_unstarted: return "dropped_unstarted";
    case Final::replaced: return "replaced";
    }
    return "?";
}

struct Request {
    uint64_t id = 0, epoch = 0, gen = 0, key = 0;
    int ply = 0, test_delay_ms = 0;
    bool dump = false; 
    std::string sfen;
    int64_t t_request_us = 0;
};

struct Report {
    uint64_t id = 0, gen = 0, key = 0, epoch = 0;
    Final final = Final::none;
    std::string why;
    int rows = 0, mated = 0;
    int64_t t_request_us = 0, t_start_us = -1, t_done_us = -1, t_publish_us = -1, t_cancel_us = -1;
    int64_t snapshot_us = -1, children_us = -1, encode_us = -1, infer_us = -1, delay_us = -1, post_us = -1;
    bool cancel_requested = false, canceller_called = false, run_terminated = false;
    uint64_t worker_cycles = 0, body_fnv = 0;
    std::string dump;   
};

enum class Submit { queued, busy, stopped };

template <class Payload>
class WorkerState : public std::enable_shared_from_this<WorkerState<Payload>> {
public:
    using Cancelled = std::function<bool()>;
    using Job = std::function<bool(const Request&, Report&, Payload&, const Cancelled&)>;
    using Publish = std::function<void(Payload&&, const Request&)>;

    struct Totals {
        uint64_t submitted = 0, skipped_busy = 0, skipped_stopped = 0, replaced = 0, started = 0, finished = 0,
                 published = 0, terminated = 0, expired = 0, epoch_changed = 0, gen_changed = 0, key_changed = 0,
                 duplicate = 0, run_error = 0, snapshot_error = 0, dropped_unstarted = 0, shutdown_dropped = 0,
                 cancel_requests = 0, canceller_calls = 0, finished_after_go_end = 0;
    };

    ~WorkerState() { shutdown(2000); }

    void start(Job job, Publish publish) {
        std::lock_guard<std::mutex> lk(m_);
        if (th_.joinable() || detached_) return;
        job_ = std::move(job); publish_ = std::move(publish);
        stop_ = false; exited_ = false; detached_ = false;
        th_ = std::thread([owner = this->shared_from_this()] { owner->loop(); });
    }
    bool running_thread() const { std::lock_guard<std::mutex> lk(m_); return th_.joinable(); }
    bool detached() const { std::lock_guard<std::mutex> lk(m_); return detached_; }

    bool shutdown(int timeout_ms, int64_t* waited_us = nullptr) {
        const int64_t t0 = now_us();
        std::unique_lock<std::mutex> lk(m_);
        if (!th_.joinable()) { if (waited_us) *waited_us = 0; return !detached_; }
        stop_ = true; live_ = false; ++epoch_;
        if (pending_) finish_unstarted_locked(Final::shutdown);
        cancel_running_locked(t0);
        cv_.notify_all();
        const bool done = cv_exit_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return exited_; });
        lk.unlock();
        if (done) th_.join(); else { th_.detach(); std::lock_guard<std::mutex> g(m_); detached_ = true; }
        if (waited_us) *waited_us = now_us() - t0;
        return done;
    }

    uint64_t epoch() const { std::lock_guard<std::mutex> lk(m_); return epoch_; }

    void begin_go(uint64_t gen, uint64_t key) {
        std::lock_guard<std::mutex> lk(m_);
        live_ = true; live_gen_ = gen; live_key_ = key; go_end_us_ = -1;
        if (pending_) finish_unstarted_locked(Final::replaced);
    }

    Submit submit(Request r, uint64_t* id_out = nullptr) {
        std::lock_guard<std::mutex> lk(m_);
        if (stop_ || !th_.joinable()) { ++t_.skipped_stopped; return Submit::stopped; }
        if (running_) { ++t_.skipped_busy; return Submit::busy; }
        if (pending_) finish_unstarted_locked(Final::replaced);
        r.id = ++next_id_; r.epoch = epoch_;
        Report rep; rep.id = r.id; rep.gen = r.gen; rep.key = r.key; rep.epoch = r.epoch; rep.t_request_us = r.t_request_us;
        keep_locked(std::move(rep));
        pending_ = std::move(r); ++t_.submitted;
        if (id_out) *id_out = pending_->id;
        cv_.notify_one();
        return Submit::queued;
    }

    void end_go(uint64_t gen) {
        std::lock_guard<std::mutex> lk(m_);
        if (live_gen_ != gen) return; 
        const int64_t t = now_us();
        live_ = false; go_end_us_ = t;
        if (pending_ && pending_->gen == gen) finish_unstarted_locked(Final::dropped_unstarted);
        if (running_ && running_req_.gen == gen) cancel_running_locked(t);
    }

    void invalidate() {
        std::lock_guard<std::mutex> lk(m_);
        ++epoch_;
        if (pending_) finish_unstarted_locked(Final::epoch_changed);
        cancel_running_locked(now_us());
    }

    void set_canceller(std::function<void()> c) {
        std::lock_guard<std::mutex> lk(m_);
        canceller_ = std::move(c);
        if (canceller_ && cancel_.load(std::memory_order_relaxed)) call_canceller_locked();
    }

    std::optional<Report> report(uint64_t id) const {
        std::lock_guard<std::mutex> lk(m_);
        for (const auto& r : reports_) if (r.id == id) return r;
        return std::nullopt;
    }
    Totals totals() const { std::lock_guard<std::mutex> lk(m_); return t_; }
    bool busy() const { std::lock_guard<std::mutex> lk(m_); return running_; }

private:
    void keep_locked(Report rep) {
        reports_.push_back(std::move(rep));
        while (reports_.size() > 32) reports_.pop_front();
    }
    Report* find_locked(uint64_t id) {
        for (auto& r : reports_) if (r.id == id) return &r;
        return nullptr;
    }
    void finish_unstarted_locked(Final f) {
        if (Report* r = find_locked(pending_->id)) { r->final = f; r->t_done_us = now_us(); }
        if (f == Final::replaced) ++t_.replaced;
        else if (f == Final::shutdown) ++t_.shutdown_dropped;
        else if (f == Final::epoch_changed) ++t_.epoch_changed;
        else ++t_.dropped_unstarted;
        pending_.reset();
    }
    void call_canceller_locked() {
        if (!canceller_) return;
        canceller_(); ++t_.canceller_calls;
        if (Report* r = find_locked(running_req_.id)) r->canceller_called = true;
    }
    void cancel_running_locked(int64_t t) {
        if (!running_ || cancel_.load(std::memory_order_relaxed)) return;
        cancel_.store(true, std::memory_order_relaxed); ++t_.cancel_requests;
        if (Report* r = find_locked(running_req_.id)) { r->cancel_requested = true; r->t_cancel_us = t; }
        call_canceller_locked();
    }
    void loop() {
        std::unique_lock<std::mutex> lk(m_);
        for (;;) {
            cv_.wait(lk, [&] { return stop_ || pending_.has_value(); });
            if (stop_) break;
            Request r = std::move(*pending_); pending_.reset();
            running_ = true; running_req_ = r; cancel_.store(false, std::memory_order_relaxed);
            ++t_.started;
            Report local;
            if (Report* rr = find_locked(r.id)) { rr->t_start_us = now_us(); local = *rr; }
            lk.unlock();
            Payload p;
            const bool ok = job_(r, local, p, [this] { return cancel_.load(std::memory_order_relaxed); });
            lk.lock();
            canceller_ = nullptr;
            running_ = false;
            Report* rr = find_locked(r.id);
            if (!rr) { keep_locked(local); rr = find_locked(r.id); }
            local.cancel_requested = rr->cancel_requested; local.canceller_called = rr->canceller_called;
            local.t_cancel_us = rr->t_cancel_us; local.t_start_us = rr->t_start_us;
            local.t_done_us = now_us();
            Final f;
            if (!ok) f = local.final != Final::none ? local.final
                       : cancel_.load(std::memory_order_relaxed) ? Final::terminated : Final::run_error;
            else if (stop_) f = Final::shutdown;
            else if (r.epoch != epoch_) f = Final::epoch_changed;
            else if (!live_) f = Final::expired;
            else if (live_gen_ != r.gen) f = Final::gen_changed;
            else if (live_key_ != r.key) f = Final::key_changed;
            else if (published_gen_ == r.gen) f = Final::duplicate;
            else {
                publish_(std::move(p), r);
                published_gen_ = r.gen; local.t_publish_us = now_us();
                f = Final::published;
            }
            local.final = f;
            ++t_.finished;
            if (go_end_us_ >= 0 && local.t_done_us > go_end_us_ && !live_) ++t_.finished_after_go_end;
            switch (f) {
            case Final::published: ++t_.published; break;      case Final::terminated: ++t_.terminated; break;
            case Final::expired: ++t_.expired; break;          case Final::epoch_changed: ++t_.epoch_changed; break;
            case Final::gen_changed: ++t_.gen_changed; break;  case Final::key_changed: ++t_.key_changed; break;
            case Final::duplicate: ++t_.duplicate; break;      case Final::run_error: ++t_.run_error; break;
            case Final::snapshot_error: ++t_.snapshot_error; break; default: break;
            }
            *rr = std::move(local);
            if (stop_) break;
        }
        exited_ = true;
        cv_exit_.notify_all();
        lk.unlock();
        job_ = nullptr; publish_ = nullptr;
    }

    mutable std::mutex m_;
    std::condition_variable cv_, cv_exit_;
    std::thread th_;
    Job job_; Publish publish_;
    std::function<void()> canceller_;
    std::optional<Request> pending_;
    Request running_req_;
    bool running_ = false, stop_ = false, exited_ = false, detached_ = false;
    bool live_ = false;
    uint64_t live_gen_ = 0, live_key_ = 0, published_gen_ = 0, epoch_ = 1, next_id_ = 0;
    int64_t go_end_us_ = -1;
    std::atomic<bool> cancel_{false};
    std::deque<Report> reports_;
    Totals t_;
};

template <class Payload>
class Core {
    using State = WorkerState<Payload>;
    std::shared_ptr<State> state_ = std::make_shared<State>();
public:
    using Cancelled = typename State::Cancelled;
    using Job = typename State::Job;
    using Publish = typename State::Publish;
    using Totals = typename State::Totals;
    Core() = default;
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    ~Core() { state_->shutdown(2000); }
    void start(Job job, Publish publish) { state_->start(std::move(job), std::move(publish)); }
    bool running_thread() const { return state_->running_thread(); }
    bool detached() const { return state_->detached(); }
    bool shutdown(int ms, int64_t* waited = nullptr) { return state_->shutdown(ms, waited); }
    uint64_t epoch() const { return state_->epoch(); }
    void begin_go(uint64_t gen, uint64_t key) { state_->begin_go(gen,key); }
    Submit submit(Request r, uint64_t* id = nullptr) { return state_->submit(std::move(r),id); }
    void end_go(uint64_t gen) { state_->end_go(gen); }
    void invalidate() { state_->invalidate(); }
    void set_canceller(std::function<void()> c) { state_->set_canceller(std::move(c)); }
    std::optional<Report> report(uint64_t id) const { return state_->report(id); }
    Totals totals() const { return state_->totals(); }
    bool busy() const { return state_->busy(); }
};

} 
