#pragma once
#include <thread>
#include <chrono>
#include <cstdint>
#ifdef __x86_64__
#include <immintrin.h>
#endif

namespace lf {

inline void cpu_relax() noexcept {
#ifdef __x86_64__
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}

class ExponentialBackoff {
public:
    explicit ExponentialBackoff(unsigned ceiling = 64) noexcept : ceiling_(ceiling) {}
    void reset() noexcept { spins_ = 1; }
    void operator()() noexcept {
        for (unsigned i = 0; i < spins_; ++i) cpu_relax();
        if (spins_ < ceiling_) spins_ <<= 1;
    }
private:
    unsigned spins_ {1};
    unsigned ceiling_ {64};
};

class HybridBackoff {
public:
    HybridBackoff(unsigned spin_ceiling = 128, unsigned yield_threshold = 32) noexcept
        : spin_ceiling_(spin_ceiling), yield_threshold_(yield_threshold) {}
    void reset() noexcept { spins_ = 0; }
    void operator()() noexcept {
        if (spins_ < spin_ceiling_) {
            cpu_relax();
        } else if (spins_ < spin_ceiling_ + yield_threshold_) {
            std::this_thread::yield();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            spins_ = yield_threshold_;
        }
        ++spins_;
    }
private:
    unsigned spins_ {0};
    unsigned spin_ceiling_;
    unsigned yield_threshold_;
};

} // namespace lf
