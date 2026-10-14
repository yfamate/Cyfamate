#pragma once
// Root-only ordering from current-go opponent-mate results. Nomate proves absence
// of a checking mate, not a good evaluation. Cyfnet ranks only proven-safe moves;
// the search still evaluates them and owns the final choice.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace CyfDefense {
struct Candidate {
    uint16_t move = 0;
    int verdict = -1; // unknown=-1, no checking mate=0, opponent mate=1
    int search_rank = 0;
    bool has_cyfnet = false;
    int32_t cyfnet = 0;
};
struct Table {
    uint64_t gen = 0, root_key = 0;
    std::vector<Candidate> safe;
    std::vector<uint16_t> mated;   // [ZYF-FIX 2]
};
inline std::shared_ptr<const Table>& storage() {
    static std::shared_ptr<const Table> t;
    return t;
}
inline std::atomic<uint64_t>& picks() { static std::atomic<uint64_t> n{0}; return n; }
inline void clear() {
    std::atomic_store_explicit(&storage(), std::shared_ptr<const Table>{}, std::memory_order_release);
    picks().store(0, std::memory_order_relaxed);
}
inline void publish(uint64_t gen, uint64_t root_key, std::vector<Candidate> candidates) {
    auto t = std::make_shared<Table>(); t->gen=gen; t->root_key=root_key;
    for (const auto& c : candidates) {
        if (c.move && c.verdict==0) t->safe.push_back(c);
        else if (c.move && c.verdict==1) t->mated.push_back(c.move);
    }
    std::stable_sort(t->safe.begin(), t->safe.end(), [](const auto& a, const auto& b) {
        if (a.has_cyfnet != b.has_cyfnet) return a.has_cyfnet;
        if (a.has_cyfnet && a.cyfnet != b.cyfnet) return a.cyfnet > b.cyfnet;
        return a.search_rank < b.search_rank;
    });
    std::shared_ptr<const Table> immutable=std::move(t);
    std::atomic_store_explicit(&storage(), std::move(immutable), std::memory_order_release);
}
template<class Eligible>
inline uint16_t preferred(uint64_t gen, uint64_t root_key, Eligible eligible) {
    const auto t=std::atomic_load_explicit(&storage(), std::memory_order_acquire);
    if (!t || t->gen!=gen || t->root_key!=root_key) return 0;
    for (const auto& c : t->safe) if (eligible(c.move)) return c.move;
    return 0;
}
inline bool mated(uint64_t gen, uint64_t root_key, uint16_t move) {   // [ZYF-FIX 2]
    const auto t=std::atomic_load_explicit(&storage(), std::memory_order_acquire);
    if (!t || t->gen!=gen || t->root_key!=root_key) return false;
    return std::find(t->mated.begin(), t->mated.end(), move) != t->mated.end();
}
} // namespace CyfDefense
