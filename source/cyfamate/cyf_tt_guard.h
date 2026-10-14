#pragma once
#include <atomic>
#include <bit>
#include <cstdint>
#include <thread>

// One lock per TT cluster. With TT_CLUSTER_SIZE 3 the lock byte lives in the
// cluster's former padding, on the cache line the probe already loads; entry
// size, cluster size and hash capacity stay unchanged. Other cluster sizes have
// no spare byte and use an external stripe chosen by the cluster address.
// Table allocation, clearing and destruction still require stopped searchers.
namespace CyfTT {
using Lock = std::atomic<std::uint8_t>;
static_assert(sizeof(Lock) == 1 && Lock::is_always_lock_free, "TT lock must fit one padding byte");

struct alignas(64) Stripe { Lock busy{0}; };
inline Stripe stripes[8192];
inline Lock& stripe_for(const void* cluster) {
    return stripes[(reinterpret_cast<std::uintptr_t>(cluster) >> 5) & 8191].busy;
}

#if defined(__GNUC__) || defined(__clang__)
#define CYF_TT_COLD_PATH __attribute__((noinline, cold))
#elif defined(_MSC_VER)
#define CYF_TT_COLD_PATH __declspec(noinline)
#else
#define CYF_TT_COLD_PATH
#endif
// [NPS 2026-10-09] clang on x86-64 (System V and Win64): the contended spin uses the preserve_most calling
//   convention, so it saves every general register it touches itself. Without it the callers (TT probe,
//   TTWriter::write, TTEntry::save ...) still saved six call-preserved registers on every call, uncontended or
//   not, to keep their live values across the never-taken call. The attribute changes only the register
//   contract of this one call; the atomic operation sequence and memory orders are unchanged.
#if defined(__clang__) && (defined(__x86_64__) || defined(_M_X64))
#define CYF_TT_PRESERVE_MOST __attribute__((preserve_most))
#else
#define CYF_TT_PRESERVE_MOST
#endif

class Guard {
    Lock& busy;
    // [NPS 2026-10-08] The contended spin (with its yield call) lives out of line, so callers of an
    // uncontended Guard need no call-preserved registers. Same atomic operation sequence as the former
    // single loop: exchange; while it returned 1: spin on relaxed loads (yield every 64th), exchange again.
    CYF_TT_COLD_PATH CYF_TT_PRESERVE_MOST static void acquire_contended(Lock& busy) noexcept {
        unsigned spins = 0;
        do {
            while (busy.load(std::memory_order_relaxed))
                if (++spins % 64 == 0) std::this_thread::yield();
        } while (busy.exchange(1, std::memory_order_acquire));
    }
public:
    explicit Guard(Lock& lock) : busy(lock) {
        if (busy.exchange(1, std::memory_order_acquire)) [[unlikely]]
            acquire_contended(busy);
    }
    ~Guard() { busy.store(0, std::memory_order_release); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};
}
