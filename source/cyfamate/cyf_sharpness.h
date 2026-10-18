#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CyfSharp {
struct Observation {
    double instability = .5, tightness = .5, closeness = .5, disagree = .5, scalar = .5;
    double policy_max = -1;
    int cp = 0;
    unsigned valid = 0; 
};
inline double sigmoid(double cp, double scale) { return 1.0 / (1.0 + std::exp(-cp / scale)); }
inline Observation observe(int cp, bool cp_ok, bool dl_ok, double policy_max, bool agree) {
    Observation r;
    r.cp = cp;
    if (cp_ok) {
        r.closeness = 1.0 - std::abs(2.0 * sigmoid(std::abs(double(cp)), 285.0) - 1.0);
        r.valid |= 4;
    }
    if (dl_ok && std::isfinite(policy_max) && policy_max >= 0 && policy_max <= 1) {
        r.policy_max = policy_max;
        r.instability = 2.0 * sigmoid(policy_max * 600.0, 200.0) - 1.0;
        r.tightness = 2.0 * (1.0 - sigmoid(policy_max * 600.0, 300.0));
        r.disagree = agree ? 0.0 : 1.0;
        r.valid |= 11;
    }
    r.scalar = std::clamp(.30 * r.instability + .25 * r.tightness + .25 * r.closeness + .20 * r.disagree, 0.0, 1.0);
    return r;
}
struct State {
    uint64_t gen = 0, key = 0, policy_gen = 0, policy_key = 0;
    int ply = -1;
    uint64_t tm_uses = 0;
    Observation current;
    void begin(uint64_t generation, uint64_t root_key, int root_ply) {
        gen = generation; key = root_key; ply = root_ply;
        policy_gen = policy_key = 0; tm_uses = 0; current = {};
    }
    bool bind_policy(uint64_t generation, uint64_t root_key) {
        if (generation != gen || root_key != key || gen == 0) return false;
        policy_gen = generation; policy_key = root_key;
        return true;
    }
    bool policy_current() const { return gen != 0 && policy_gen == gen && policy_key == key; }
};
inline State& state() { static State s; return s; }
} 
