#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <concepts>
#include <optional>
#include "detail/backoff.hpp"
#include "detail/queue_concept.hpp"

namespace lf {

struct AdaptiveConfig {
    float full_high {0.90f};     // enter block if fullness >= full_high
    float full_low  {0.70f};     // exit block only if fullness <= full_low
    float fail_high {0.60f};     // enter block if failure rate >= fail_high
    float fail_low  {0.30f};     // exit block if failure rate <= fail_low

    unsigned base_spin {64};     // base spin attempts when light contention
    unsigned min_spin  {4};      // minimal spin attempts under heavy contention
    unsigned max_spin  {256};    // optional upper bound

    std::chrono::nanoseconds block_grace { std::chrono::microseconds(50) }; // grace after leaving block

    // Exponential moving average smoothing factor (alpha)
    float ema_alpha {0.05f};

    // How many local attempts between publishing local stats into global EMA
    unsigned local_publish_period {64};
};

enum class AdaptiveMode : uint8_t { Spin, Block };

// Helper to scale floats to integer for atomic updates (avoid fp atomics)
inline uint32_t scale_float(float v) noexcept { return static_cast<uint32_t>(v * 100000.0f); }
inline float unscale_uint(uint32_t v) noexcept { return static_cast<float>(v) / 100000.0f; }

template <typename Queue, typename Backoff = ExponentialBackoff>
class AdaptiveBlockingQueue {
public:
    template<typename... Args>
    AdaptiveBlockingQueue(Args... args)
        : q_(std::forward<Args>(args)...) {}

    
    void set_config(AdaptiveConfig& config) {
        cfg_ = config;
    }

    template <typename T>
    bool push(T&& v) { return push_impl(std::forward<T>(v)); }

    template <typename T, typename Duration = std::chrono::nanoseconds>
    bool push_until(T&& v, const Duration& timeout) {
        return push_impl(std::forward<T>(v), std::optional<Duration>(timeout));
    }

    template <typename T>
    bool pop(T& out) { return pop_impl(out); }

    template <typename T, typename Duration = std::chrono::nanoseconds>
    bool pop_until(T& out, const Duration& timeout) {
        return pop_impl(out, std::optional<Duration>(timeout));
    }

    bool empty() const noexcept { return q_.empty(); }

    AdaptiveMode mode() const noexcept { return mode_.load(std::memory_order_relaxed); }
    float failure_rate_ema() const noexcept { return unscale_uint(fail_ema_.load(std::memory_order_relaxed)); }
    float fullness_last() const noexcept { return unscale_uint(fullness_last_.load(std::memory_order_relaxed)); }

    
private:
    template <typename T, typename Duration = std::chrono::nanoseconds>
    bool push_impl(T&& v, std::optional<Duration> timeout = std::nullopt) {
        if (q_.try_push(std::forward<T>(v))) {
            on_success();
            notify_not_empty();
            return true;
        }
        return adaptive_push(std::forward<T>(v), timeout);
    }

    template <typename T, typename Duration = std::chrono::nanoseconds>
    bool pop_impl(T& out, std::optional<Duration> timeout = std::nullopt) {
        if (q_.try_pop(out)) {
            on_success_pop();
            notify_not_full();
            return true;
        }
        return adaptive_pop(out, timeout);
    }

    template <typename T, typename Duration>
    bool adaptive_push(T&& v, std::optional<Duration> timeout) {
        auto start = std::chrono::steady_clock::now();
        auto maybe_deadline = timeout ? std::optional<std::chrono::steady_clock::time_point>(start + *timeout) : std::nullopt;

        for (;;) {
            AdaptiveMode mode = mode_.load(std::memory_order_relaxed);
            if (mode == AdaptiveMode::Block && !should_exit_block()) {
                return block_push(std::forward<T>(v), maybe_deadline);
            }
            unsigned spins = decide_spin_budget();
            Backoff bk;
            bk.reset();
            for (unsigned i = 0; i < spins; ++i) {
                if (q_.try_push(std::forward<T>(v))) {
                    on_success();
                    notify_not_empty();
                    return true;
                }
                bk();
            }
            // still failed
            on_failure();
            if (should_enter_block()) {
                switch_to_block();
                return block_push(std::forward<T>(v), maybe_deadline);
            }
            // Non-blocking behavior if no timeout specified
            if (!maybe_deadline) return false;
            if (std::chrono::steady_clock::now() >= *maybe_deadline) return false;
        }
    }

    template <typename T, typename Duration>
    bool adaptive_pop(T& out, std::optional<Duration> timeout) {
        auto start = std::chrono::steady_clock::now();
        auto maybe_deadline = timeout ? std::optional<std::chrono::steady_clock::time_point>(start + *timeout) : std::nullopt;

        for (;;) {
            AdaptiveMode mode = mode_.load(std::memory_order_relaxed);
            if (mode == AdaptiveMode::Block && !should_exit_block()) {
                return block_pop(out, maybe_deadline);
            }
            unsigned spins = decide_spin_budget();
            Backoff bk;
            bk.reset();
            for (unsigned i = 0; i < spins; ++i) {
                if (q_.try_pop(out)) {
                    on_success_pop();
                    notify_not_full();
                    return true;
                }
                bk();
            }
            on_failure();
            if (should_enter_block()) {
                switch_to_block();
                return block_pop(out, maybe_deadline);
            }
            if (!maybe_deadline) return false;
            if (std::chrono::steady_clock::now() >= *maybe_deadline) return false;
        }
    }

    template <typename TimePoint>
    bool block_push(auto&& v, std::optional<TimePoint> deadline) {
        std::unique_lock<std::mutex> lk(m_);
        while (!q_.try_push(std::forward<decltype(v)>(v))) {
            if (deadline) {
                if (cv_not_full_.wait_until(lk, *deadline) == std::cv_status::timeout) return false;
            } else {
                cv_not_full_.wait(lk);
            }
        }
        lk.unlock();
        notify_not_empty();
        return true;
    }

    template <typename TimePoint>
    bool block_pop(auto& out, std::optional<TimePoint> deadline) {
        std::unique_lock<std::mutex> lk(m_);
        while (!q_.try_pop(out)) {
            if (deadline) {
                if (cv_not_empty_.wait_until(lk, *deadline) == std::cv_status::timeout) return false;
            } else {
                cv_not_empty_.wait(lk);
            }
        }
        lk.unlock();
        notify_not_full();
        return true;
    }

    void on_success() noexcept {
        tls_stats().failures_in_period = 0;
        maybe_publish();
    }
    void on_success_pop() noexcept { on_success(); }

    void on_failure() noexcept {
        auto& ls = tls_stats();
        ++ls.failures_in_period;
        if (++ls.attempts_in_period >= cfg_.local_publish_period) {
            maybe_publish();
        }
    }

    struct LocalStats {
        uint32_t failures_in_period {0};
        uint32_t attempts_in_period {0};
    };

    static LocalStats& tls_stats() {
        thread_local LocalStats ls;
        return ls;
    }

    // Sharded aggregator for EMA to reduce CAS contention
    struct Shard {
        std::atomic<uint32_t> ema { scale_float(0.0f) }; // scaled EMA
    };

    static Shard* shard_array() {
        static Shard shards[kShards];
        
        return shards;
    }
    
    static Shard& shard_for_thread() {
        Shard* shards = shard_array();
        thread_local size_t idx = reinterpret_cast<uintptr_t>(&tls_stats()) & (kShards - 1);

        return shards[idx];
    }

    float aggregated_failure_ema() const noexcept {
        Shard* shards = shard_array();
        uint64_t sum = 0;
        for (size_t i = 0;i < kShards; ++i) {
            sum += shards[i].ema.load(std::memory_order_relaxed);
        }
        return unscale_uint(static_cast<uint32_t>(sum / kShards));
    }

    

    // incorporate local stats into one shard EMA; update global fail_ema_ lazily
    void maybe_publish() noexcept {
        auto& ls = tls_stats();
        // compute sample failure rate in this period
        float sample = 0.0f;
        if (ls.attempts_in_period > 0) {
            sample = static_cast<float>(ls.failures_in_period / static_cast<float>(ls.attempts_in_period));
        }

        Shard& sh = shard_for_thread();
        uint32_t oldv = sh.ema.load(std::memory_order_relaxed);
        for (;;) {
            float oldf = unscale_uint(oldv);
            float newf = oldf * (1.0f - cfg_.ema_alpha) + sample * cfg_.ema_alpha;
            uint32_t newv = scale_float(newf);
            if (sh.ema.compare_exchange_weak(oldv, newv, std::memory_order_relaxed)) break;
        }
        // Reset local counters for next period
        ls.attempts_in_period = 0;
        ls.failures_in_period = 0;
        // Occasionally update global snapshot fail_ema_ (cheap read of shard average)
        static thread_local unsigned publish_counter = 0;
        if ((++publish_counter & 0x3F) == 0) { // every 64 periods
            // (simplify: just copy this shard's EMA into global to avoid O(k) loop cost on every publish)
            fail_ema_.store(sh.ema.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        // update fullness snapshot if bounded
        if constexpr (HasCapacity<Queue>) {
            auto cap = q_.capacity();
            if (cap) {
                float fullness = static_cast<float>(q_.size()) / static_cast<float>(cap);
                fullness_last_.store(scale_float(fullness), std::memory_order_relaxed);
            }
        }
    }

    bool should_enter_block() const noexcept {
        float fr = failure_rate_ema();
        float full = fullness_last();
        return (fr >= cfg_.fail_high) || (full >= cfg_.full_high);
    }

    bool should_exit_block() const noexcept {
        float fr = failure_rate_ema();
        float full = fullness_last();
        auto now = std::chrono::steady_clock::now();
        bool grace_ok = (now - last_block_exit_ts_.load(std::memory_order_relaxed)) > cfg_.block_grace;
        return (fr <= cfg_.fail_low && full <= cfg_.full_low && grace_ok);
    }

    void switch_to_block() noexcept { mode_.store(AdaptiveMode::Block, std::memory_order_relaxed); }
    void switch_to_spin() noexcept {
        last_block_exit_ts_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        mode_.store(AdaptiveMode::Spin, std::memory_order_relaxed);
    }

    unsigned decide_spin_budget() const noexcept {
        float fr = failure_rate_ema();
        float full = fullness_last();
        if (mode_.load(std::memory_order_relaxed) == AdaptiveMode::Block) {
            if (should_exit_block()) const_cast<AdaptiveBlockingQueue*>(this)->switch_to_spin();
            else return cfg_.min_spin; // still block mode
        }
        // scale spin between min_spin and base_spin depending on fr & fullness (simple heuristic)
        float penalty = 0.0f; // 0..1
        if constexpr (HasCapacity<Queue>) {
            penalty = (fr + full) * 0.5f;
        } else {
            penalty = fr;
        }
        float span = static_cast<float>(cfg_.base_spin - cfg_.min_spin);
        unsigned dynamic_spins = cfg_.base_spin - static_cast<unsigned>(span * penalty);
        if (dynamic_spins < cfg_.min_spin) dynamic_spins = cfg_.min_spin;
        if (dynamic_spins > cfg_.max_spin) dynamic_spins = cfg_.max_spin;
        return dynamic_spins;
    }

    void notify_not_empty() noexcept { cv_not_empty_.notify_one(); }
    void notify_not_full() noexcept { cv_not_full_.notify_one(); }

private:
    Queue q_;
    AdaptiveConfig cfg_ {};

    // Mode state
    std::atomic<AdaptiveMode> mode_ { AdaptiveMode::Spin };
    std::atomic<uint32_t> fail_ema_ { scale_float(0.0f) };
    std::atomic<uint32_t> fullness_last_ { scale_float(0.0f) };
    std::atomic<std::chrono::steady_clock::time_point> last_block_exit_ts_{ std::chrono::steady_clock::now() };

    // tls
    static constexpr size_t kShards = 16;

    mutable std::mutex m_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
};

} // namespace lf
