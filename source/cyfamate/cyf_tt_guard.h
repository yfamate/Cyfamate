#pragma once
#include <atomic>
#include <bit>
#include <cstdint>
#include <thread>

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
#if defined(__clang__) && (defined(__x86_64__) || defined(_M_X64))
#define CYF_TT_PRESERVE_MOST __attribute__((preserve_most))
#else
#define CYF_TT_PRESERVE_MOST
#endif

class Guard {
    Lock& busy;
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
