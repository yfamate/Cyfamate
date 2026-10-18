#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace CyfDemand {

#pragma pack(push, 1)
struct Rec {
    uint32_t search;       
    uint32_t seq;          
    uint64_t acc_req;      
    uint64_t t_ns;         
    uint64_t key;          
    uint64_t parent_key;   
    uint64_t state_hash;   
    uint32_t refresh_tsc;  
    uint16_t game_ply;
    uint8_t  forced;       
    uint8_t  dist;         
    uint8_t  king_plies;   
    uint8_t  null_plies;   
    uint8_t  stm;          
    uint8_t  reserved0;
    uint32_t reserved1;
    int32_t  ix[80];       
};
#pragma pack(pop)
static_assert(sizeof(Rec) == 384);

constexpr uint64_t kCap = 131072;

struct State {
    bool enabled = false, active = false;
    std::FILE* file = nullptr;
    std::thread::id owner{};
    uint32_t search = 0, seq = 0;
    uint64_t acc_req = 0, written = 0, dropped = 0, other_thread = 0;
    uint64_t n_computed = 0, n_parent = 0, n_grandparent = 0, n_refresh = 0, n_forced = 0;
    uint64_t refresh_tsc = 0;
    std::chrono::steady_clock::time_point t0{};
    std::vector<Rec> buf;
    State() {
        if (const char* p = std::getenv("CYF_DEMAND_TRACE")) {
            file = std::fopen(p, "ab");
            enabled = file != nullptr;
            buf.reserve(4096);
        }
    }
    ~State() { if (file) std::fclose(file); }
};
inline State& state() { static State s; return s; }

inline uint64_t fnv64(const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

inline void begin_search() {
    auto& s = state();
    if (!s.enabled) return;
    s.active = true; s.owner = std::this_thread::get_id(); ++s.search; s.seq = 0;
    s.acc_req = s.n_computed = s.n_parent = s.n_grandparent = s.n_refresh = s.n_forced = 0;
    s.refresh_tsc = s.dropped = s.other_thread = 0;
    s.buf.clear(); s.t0 = std::chrono::steady_clock::now();
}

inline bool on() {
    auto& s = state();
    if (!s.enabled || !s.active) return false;
    if (std::this_thread::get_id() != s.owner) { ++s.other_thread; return false; }
    return true;
}

inline void push(const Rec& r) {
    auto& s = state();
    if (s.written + s.buf.size() >= kCap) { ++s.dropped; return; }
    s.buf.push_back(r);
}

template<class Print>
inline void end_search(uint64_t nodes, uint64_t model_generation, Print print) {
    auto& s = state();
    if (!s.enabled || !s.active) return;
    s.active = false;
    if (!s.buf.empty()) { std::fwrite(s.buf.data(), sizeof(Rec), s.buf.size(), s.file); std::fflush(s.file); }
    s.written += s.buf.size();
    char line[512];
    std::snprintf(line, sizeof line,
        "info string cyfdemand search=%u nodes=%llu model_gen=%llu acc_req=%llu computed=%llu parent=%llu grandparent=%llu"
        " refresh=%llu forced=%llu refresh_tsc=%llu recorded=%zu dropped=%llu other_thread=%llu total_written=%llu",
        s.search, (unsigned long long)nodes, (unsigned long long)model_generation, (unsigned long long)s.acc_req,
        (unsigned long long)s.n_computed, (unsigned long long)s.n_parent, (unsigned long long)s.n_grandparent,
        (unsigned long long)s.n_refresh, (unsigned long long)s.n_forced, (unsigned long long)s.refresh_tsc, s.buf.size(),
        (unsigned long long)s.dropped, (unsigned long long)s.other_thread, (unsigned long long)s.written);
    s.buf.clear();
    print(line);
}

}  
