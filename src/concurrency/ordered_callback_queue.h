#pragma once
#include <cstdint>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <cassert>

// A per-connection ordered callback buffer.
// Single producer per sequence insertion from IO thread for reservation, multiple producers from worker completion can be adapted
// For simplicity here we assume all completions are enqueued via push(seq, fn) possibly from many threads; consumption is single-thread (IO reactor)

namespace concurrency {

class OrderedCallbackQueue {
public:
    using Callback = std::function<void()>;

    explicit OrderedCallbackQueue(std::size_t window_power_of_two = 8)
        : window_(1ull << window_power_of_two), mask_(window_ - 1), slots_(window_) {
        for (auto & s: slots_) {
            s.ready.store(false, std::memory_order_relaxed);
        }
    }

    // returns false if window full (caller should back off)
    bool push(uint64_t seq, Callback cb) {
        if (seq < base_) {
            return false; // already consumed
        }
        if (seq >= base_ + window_) {
            return false; // window overflow
        }
        auto & slot = slots_[seq & mask_];
        // spin until free OR already set (should not happen under correct usage)
        bool expected = false;
        if (!slot.ready.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return false; // collision (window too small)
        }
        slot.cb = std::move(cb);
        return true;
    }

    // Consume available in-order callbacks up to limit (0=unbounded)
    template <typename F>
    size_t drain(F&& consume, size_t limit = 0) {
        size_t processed = 0;
        while (true) {
            if (limit && processed >= limit) break;
            auto seq = base_;
            if (seq >= next_expected_) break; // nothing to do
            auto & slot = slots_[seq & mask_];
            if (!slot.ready.load(std::memory_order_acquire)) break; // gap
            auto cb = std::move(slot.cb);
            slot.cb = nullptr;
            slot.ready.store(false, std::memory_order_release);
            ++base_;
            if (cb) consume(std::move(cb));
            ++processed;
        }
        return processed;
    }

    void expect_until(uint64_t next_seq_exclusive) { next_expected_ = next_seq_exclusive; }

    uint64_t base() const { return base_; }

private:
    struct Slot {
        std::atomic<bool> ready{false};
        Callback cb{};
    };
    uint64_t base_{0};
    uint64_t next_expected_{0};
    const uint64_t window_;
    const uint64_t mask_;
    std::vector<Slot> slots_;
};

} // namespace concurrency
