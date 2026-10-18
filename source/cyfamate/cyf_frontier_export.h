#pragma once
#include <atomic>
#include <cstdint>

namespace CyfFrontier {

constexpr int kMaxSnaps = 2;          
constexpr int kMaxRootChildren = 3;   
constexpr int kMaxPv = 16;            

struct State {
    std::atomic<bool> enabled{false};
    std::atomic<int> d1{6}, d2{12};
    uint64_t serial = 0;    
    bool ponder = false;    
    int emitted = 0;
    uint64_t lines_total = 0, bytes_total = 0;
};

inline State& state() { static State s; return s; }

inline void begin_search(bool ponder) { auto& s = state(); s.emitted = 0; ++s.serial; s.ponder = ponder; }

}  
