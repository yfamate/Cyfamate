#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace CyfRootHint {

struct Msg {
    uint64_t gen = 0, key = 0;
    bool has_key = false;
    bool native = false;   
    int apply = 0;
    std::string kind, model;
    std::vector<std::pair<std::string, int>> moves;
    int64_t recv_us = 0;
};
inline bool parse_key(const std::string& v, uint64_t& out) {
    if (v.empty()) return false;
    for (char c : v) if (c < '0' || c > '9') return false;
    try { size_t used=0; out=std::stoull(v,&used,10); return used==v.size(); }
    catch (...) { return false; }
}
enum class KeyCheck { ok, missing, mismatch };
inline KeyCheck key_check(const Msg& m, uint64_t root_key, bool required) {
    if (!m.has_key) return required ? KeyCheck::missing : KeyCheck::ok;
    return m.key==root_key ? KeyCheck::ok : KeyCheck::mismatch;
}
struct Table {
    uint64_t gen = 0;
    int n = 0;
    uint16_t move[64] = {};
    int32_t bonus[64] = {};
};
struct GoStats {
    uint64_t gen = 0, root_key = 0, hint_key = 0;
    bool require_key = false, hint_has_key = false;
    bool matched = false, hint_native = false;
    int collect_depth = 0, legal = 0, excluded_legal = 0, illegal = 0, n = 0, apply = 0, applied = 0;
    int64_t recv_ms = -1, collect_ms = -1;
    std::string kind, model, top1;
    std::vector<uint16_t> order;   
    std::vector<std::pair<uint16_t, int>> scored;   
    uint64_t bonus_scored = 0, trace_frames = 0, trace_compared = 0, trace_changed = 0, trace_changed_frames = 0;
    uint64_t accepted_hint_tt = 0, accepted_hint_capture = 0, accepted_hint_quiet = 0, accepted_hint_evasion = 0;
};
struct State {
    std::mutex mtx;
    bool pending = false;
    Msg msg;
    std::atomic<const Table*> active{nullptr};
    std::vector<std::unique_ptr<Table>> owned;   
    std::atomic<bool> enabled{false};
    std::atomic<bool> require_key{false};
    std::atomic<bool> trace_requested{false}, trace_enabled{false};  
    std::atomic<int> topk{8}, step{8000};
    int64_t go_start_us = 0;
    uint64_t key_missing = 0, key_mismatch = 0;   
    uint64_t received = 0, stale = 0, malformed = 0, disabled_drops = 0;   
    uint64_t native_received = 0;   
    GoStats go;
};
inline State& state() { static State s; return s; }
inline int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void receive(std::istringstream& is) {
    Msg m; m.recv_us = now_us();
    std::string tok; bool moves = false, ok = true, have_gen = false;
    while (is >> tok) {
        if (moves) {
            std::string sc;
            if (!(is >> sc)) { ok = false; break; }
            try { m.moves.emplace_back(tok, std::stoi(sc)); } catch (...) { ok = false; break; }
            continue;
        }
        if (tok == "moves") { moves = true; continue; }
        const auto eq = tok.find('=');
        if (eq == std::string::npos) { ok = false; break; }
        const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
        try {
            if (k == "gen") { m.gen = std::stoull(v); have_gen = true; }
            else if (k == "key") { m.has_key = parse_key(v,m.key); if (!m.has_key) ok = false; }
            else if (k == "apply") m.apply = std::stoi(v);
            else if (k == "kind") m.kind = v;
            else if (k == "model") m.model = v;
        } catch (...) { ok = false; break; }
    }
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mtx);
    ++s.received;
    if (!ok || !have_gen || m.moves.size() > 1024) { ++s.malformed; return; }
    if (!s.enabled.load(std::memory_order_relaxed)) { ++s.disabled_drops; return; }
    if (s.pending) ++s.stale;   
    s.msg = std::move(m); s.pending = true;
}

inline void deliver(Msg&& m) {
    auto& s = state();
    std::lock_guard<std::mutex> lk(s.mtx);
    ++s.received; ++s.native_received;
    if (m.moves.size() > 1024) { ++s.malformed; return; }
    if (!s.enabled.load(std::memory_order_relaxed)) { ++s.disabled_drops; return; }
    if (s.pending) ++s.stale;   
    s.msg = std::move(m); s.pending = true;
}

inline void begin_go(uint64_t gen, bool single_thread = true, uint64_t root_key = 0) {
    auto& s = state();
    s.active.store(nullptr, std::memory_order_release);
    s.owned.clear();
    s.go = GoStats{}; s.go.gen = gen; s.go.root_key = root_key;
    s.go.require_key = s.require_key.load(std::memory_order_relaxed);
    s.go_start_us = now_us();
    s.trace_enabled.store(single_thread && s.trace_requested.load(), std::memory_order_relaxed);
}

inline int32_t bonus(uint16_t raw) {
    const Table* t = state().active.load(std::memory_order_acquire);
    if (!t) return 0;
    for (int i = 0; i < t->n; ++i)
        if (t->move[i] == raw) return t->bonus[i];
    return 0;
}

inline bool tracing() { return state().trace_enabled.load(std::memory_order_relaxed); }
inline int32_t scoring_bonus(uint16_t raw) {
    const int32_t b = bonus(raw);
    if (b && tracing()) ++state().go.bonus_scored;
    return b;
}

} 
