#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace CyfIter {

constexpr int kMaxSnaps = 128;
constexpr int kMaxCandPerSnap = 600;
constexpr int kMaxCandTotal = 65536;
constexpr int kMaxPasses = 1024;
constexpr int kMaxAcceptPerPass = 600;
constexpr int kMaxAcceptTotal = 131072;

struct Cand { uint16_t move; int32_t score; char bound; char status; };   
struct Snap {
    int depth = 0, sel = 0, pass = -1;   
    uint64_t nodes = 0;
    int64_t t_us = 0, tm_ms = 0;
    uint16_t head = 0;
    int nroot = 0, k = 0;                
    uint32_t off = 0;                    
    bool trunc = false;
};
struct Pass {
    int depth = 0, adj = 0, pv = 0, att = 0;
    int32_t alpha = 0, beta = 0, best = 0;
    int64_t t0_us = 0, t1_us = 0;
    uint64_t nodes0 = 0, nodes1 = 0;
    char res = '-';                      
    uint32_t off = 0, n = 0;             
    uint32_t seen = 0;                   
    uint32_t gid = 0;                    
};
struct Accept { uint16_t move; char stage; };

struct State {
    std::atomic<bool> enabled{false};
    bool recording = false, threads_gt1 = false, ponder = false;
    uint64_t gen = 0, serial = 0, key = 0;
    int ply = 0, nroot = 0, threads = 1;
    char stm = 'b';
    std::chrono::steady_clock::time_point t0;
    std::vector<Snap> snaps;
    std::vector<Cand> cands;
    std::vector<Pass> passes;
    std::vector<Accept> accepts;
    std::vector<uint32_t> touch;         
    uint32_t gid = 0;                    
    int cur = -1;                        
    uint64_t snaps_trunc = 0, passes_trunc = 0, accepts_trunc = 0, cands_trunc = 0;
    uint64_t cyc_accept = 0, cyc_score = 0, cyc_pass = 0, cyc_snap = 0, n_accept = 0, n_score = 0;
    uint64_t muted_skipped = 0, lines_total = 0, bytes_total = 0;
};
inline State& state() { static State s; return s; }

inline int64_t now_us(const State& s) {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - s.t0).count();
}

inline void begin_search(uint64_t gen, bool ponder, uint64_t key, int ply, char stm, int nroot, int threads) {
    auto& s = state();
    s.recording = false;
    if (!s.enabled.load(std::memory_order_relaxed)) return;
    if (s.touch.empty()) {   
        s.touch.assign(65536, 0);
        s.snaps.reserve(kMaxSnaps); s.cands.reserve(kMaxCandTotal);
        s.passes.reserve(kMaxPasses); s.accepts.reserve(kMaxAcceptTotal);
    }
    s.snaps.clear(); s.cands.clear(); s.passes.clear(); s.accepts.clear();
    s.cur = -1; s.snaps_trunc = s.passes_trunc = s.accepts_trunc = s.cands_trunc = 0;
    s.cyc_accept = s.cyc_score = s.cyc_pass = s.cyc_snap = s.n_accept = s.n_score = 0;
    s.gen = gen; s.ponder = ponder; s.key = key; s.ply = ply; s.stm = stm; s.nroot = nroot; s.threads = threads;
    ++s.serial;
    s.t0 = std::chrono::steady_clock::now();
    s.threads_gt1 = threads != 1;
    s.recording = !s.threads_gt1;
}
inline bool recording() { return state().recording; }

inline void begin_pass(int depth, int adj, int pvIdx, int att, int32_t alpha, int32_t beta, uint64_t nodes) {
    auto& s = state(); if (!s.recording) return;
    const uint64_t c0 = __rdtsc();
    ++s.gid;
    if (int(s.passes.size()) >= kMaxPasses) { ++s.passes_trunc; s.cur = -1; s.cyc_pass += __rdtsc() - c0; return; }
    Pass p; p.depth = depth; p.adj = adj; p.pv = pvIdx; p.att = att; p.alpha = alpha; p.beta = beta;
    p.t0_us = now_us(s); p.nodes0 = nodes; p.off = uint32_t(s.accepts.size()); p.gid = s.gid;
    s.passes.push_back(p); s.cur = int(s.passes.size()) - 1;
    s.cyc_pass += __rdtsc() - c0;
}

inline void on_accept(uint16_t raw, char stage) {
    auto& s = state(); if (!s.recording || s.cur < 0) return;
    const uint64_t c0 = __rdtsc();
    Pass& p = s.passes[size_t(s.cur)];
    ++p.seen; ++s.n_accept;
    if (p.n >= uint32_t(kMaxAcceptPerPass) || int(s.accepts.size()) >= kMaxAcceptTotal) ++s.accepts_trunc;
    else { s.accepts.push_back(Accept{raw, stage}); ++p.n; }
    s.cyc_accept += __rdtsc() - c0;
}

inline void on_root_score(uint16_t raw, bool raised) {
    auto& s = state(); if (!s.recording) return;
    const uint64_t c0 = __rdtsc();
    s.touch[raw] = (s.gid << 2) | (raised ? 1u : 2u); ++s.n_score;
    s.cyc_score += __rdtsc() - c0;
}

inline void end_pass(int32_t best, bool stopped, uint64_t nodes) {
    auto& s = state(); if (!s.recording || s.cur < 0) return;
    const uint64_t c0 = __rdtsc();
    Pass& p = s.passes[size_t(s.cur)];
    p.best = best; p.t1_us = now_us(s); p.nodes1 = nodes;
    p.res = stopped ? 'a' : best <= p.alpha ? 'f' : best >= p.beta ? 'h' : 'e';
    s.cur = -1;
    s.cyc_pass += __rdtsc() - c0;
}

template<class RootMovesT>
inline void on_completed(const RootMovesT& rootMoves, int depth, uint64_t nodes, int64_t tm_ms, int32_t neg_inf) {
    auto& s = state(); if (!s.recording || rootMoves.empty()) return;
    const uint64_t c0 = __rdtsc();
    if (int(s.snaps.size()) >= kMaxSnaps) { ++s.snaps_trunc; s.cyc_snap += __rdtsc() - c0; return; }
    Snap sn; sn.depth = depth; sn.sel = rootMoves[0].selDepth; sn.nodes = nodes; sn.t_us = now_us(s); sn.tm_ms = tm_ms;
    sn.head = rootMoves[0].pv.empty() ? 0 : rootMoves[0].pv[0].raw(); sn.nroot = int(rootMoves.size());
    uint32_t last_gid = 0;
    for (int i = int(s.passes.size()) - 1; i >= 0; --i)
        if (s.passes[size_t(i)].depth == depth) { sn.pass = i; last_gid = s.passes[size_t(i)].gid; break; }
    if (s.passes_trunc && last_gid != s.gid) {
        sn.pass = -1; last_gid = s.gid;  
    }
    sn.off = uint32_t(s.cands.size());
    for (const auto& rm : rootMoves) {
        if (sn.k >= kMaxCandPerSnap || int(s.cands.size()) >= kMaxCandTotal) { sn.trunc = true; ++s.cands_trunc; continue; }
        const uint16_t raw = rm.pv[0].raw();
        const uint32_t t = s.touch[raw];
        const char st = (last_gid && (t >> 2) == last_gid) ? ((t & 3u) == 1u ? 'U' : 'F') : 'S';
        const char b = rm.score == neg_inf ? 'n' : rm.scoreLowerbound ? 'l' : rm.scoreUpperbound ? 'u' : 'e';
        s.cands.push_back(Cand{raw, int32_t(rm.score), b, st}); ++sn.k;
    }
    s.snaps.push_back(sn);
    s.cyc_snap += __rdtsc() - c0;
}

inline int latest_before(int64_t t_us, int64_t& age_us) {
    const auto& s = state(); age_us = -1;
    for (int i = int(s.snaps.size()) - 1; i >= 0; --i)
        if (s.snaps[size_t(i)].t_us <= t_us) { age_us = t_us - s.snaps[size_t(i)].t_us; return i; }
    return -1;
}

}  
