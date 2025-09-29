#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <cassert>
#include <cstdint>
#include <string>
#include <chrono>
#include "lockfreequeue/array_mpmc_queue.hpp"
#include "cpu_affinity.h"
#include "task.h"

namespace concurrency {

class LFThreadPool {
public:
    using Task = std::function<void()>;
    using Ptr = std::shared_ptr<LFThreadPool>;

    struct Stats {
        std::atomic<uint64_t> submitted_flexible{0};
        std::atomic<uint64_t> submitted_pinned{0};
        std::atomic<uint64_t> executed_flexible{0};
        std::atomic<uint64_t> executed_pinned{0};
    };

    // simple constructor: all workers are flexible, single queue.
    explicit LFThreadPool(std::size_t threads,
                          std::size_t queue_capacity = 1024,
                          std::vector<int> core_ids = {})
        : pinned_queue_(queue_capacity), flex_queue_(queue_capacity), stop_(false) {
        // If core ids provided -> treat all as pinned workers.
        if (!core_ids.empty()) pinned_core_ids_ = core_ids;
        for (std::size_t i = 0; i < threads; ++i) {
            if (!pinned_core_ids_.empty()) {
                pinned_workers_.emplace_back([this, i]{
                    pinned_worker_loop(i);
                });
            }
            else {
                flexible_workers_.emplace_back([this]{
                    flexible_worker_loop();
                });
            }
        }
    }

    // Advanced hybrid constructor.
    LFThreadPool(std::size_t pinned_threads,
                 std::size_t flexible_threads,
                 std::size_t queue_capacity_pinned,
                 std::size_t queue_capacity_flexible,
                 std::vector<int> pinned_core_ids = {})
        : pinned_queue_(queue_capacity_pinned),
          flex_queue_(queue_capacity_flexible),
          stop_(false),
          pinned_core_ids_(std::move(pinned_core_ids)) {
        for (std::size_t i = 0; i < pinned_threads; ++i) {
            pinned_workers_.emplace_back([this, i]{
                pinned_worker_loop(i);
            });
        }
        for (std::size_t i = 0; i < flexible_threads; ++i) {
            flexible_workers_.emplace_back([this]{
                flexible_worker_loop();
            });
        }
    }

    ~LFThreadPool() { shutdown(); }

    bool submit(Task t, TaskClass cls = TaskClass::Flexible) {
        if (stop_.load(std::memory_order_relaxed)) return false;
        lf::ArrayMPMCQueue<Task>* q = choose_queue(cls);
        auto& stat_counter = (q == &pinned_queue_) ? stats_.submitted_pinned : stats_.submitted_flexible;
        stat_counter.fetch_add(1, std::memory_order_relaxed);
        for (int i = 0; i < 64; ++i) {
            if (q->try_push(std::move(t))) return true;
        }
        return q->try_push(std::move(t));
    }

    bool submit_pinned(Task t) { return submit(std::move(t), TaskClass::PinnedOnly); }
    bool submit_flexible(Task t) { return submit(std::move(t), TaskClass::Flexible); }

    void shutdown() {
        bool expected=false;
        if (!stop_.compare_exchange_strong(expected, true)) return;
        for (auto & th: pinned_workers_) if (th.joinable()) th.join();
        for (auto & th: flexible_workers_) if (th.joinable()) th.join();
        // Drain remaining tasks
        Task task;
        while (pinned_queue_.try_pop(task)) task();
        while (flex_queue_.try_pop(task)) task();
    }

    const Stats& stats() const { return stats_; }
    std::size_t pinned_worker_count() const { return pinned_workers_.size(); }
    std::size_t flexible_worker_count() const { return flexible_workers_.size(); }

private:
    lf::ArrayMPMCQueue<Task>* choose_queue(TaskClass cls) {
        switch (cls) {
            case TaskClass::PinnedOnly:
                if (!pinned_workers_.empty()) return &pinned_queue_;
                return &flex_queue_;
            case TaskClass::PreferPinned:
                if (!pinned_workers_.empty() && pinned_queue_.size() <= flex_queue_.size()*2) return &pinned_queue_;
                return &flex_queue_;
            case TaskClass::Flexible:
            default:
                return &flex_queue_;
        }
    }

    void pinned_worker_loop(std::size_t index) {
        if (!pinned_core_ids_.empty()) {
            int core = pinned_core_ids_[index % pinned_core_ids_.size()];
            set_current_thread_affinity(core);
        }
        main_worker_loop(true /*pinned*/);
    }

    void flexible_worker_loop() { main_worker_loop(false /*pinned*/); }

    void main_worker_loop(bool pinned) {
        auto* primary = pinned ? &pinned_queue_ : &flex_queue_;
         // pinned worker can fallback; flexible normally not steal pinned unless backlog large
        auto* secondary = pinned ? &flex_queue_ : &pinned_queue_;
        while (!stop_.load(std::memory_order_relaxed)) {
            Task task;
            if (primary->try_pop(task)) {
                execute_task(task, pinned);
            } else if (pinned && secondary->try_pop(task)) {
                execute_task(task, false);
            } else if (!pinned && should_help_pinned() && secondary->try_pop(task)) {
                execute_task(task, true);
            } else {
                std::this_thread::yield();
            }
        }
    }

    inline bool should_help_pinned() const {
        // Help condition: pinned backlog much larger than flexible backlog
        std::size_t pin_sz = pinned_queue_.size();
        std::size_t flex_sz = flex_queue_.size();
        return pin_sz > flex_sz * 2 + 8; // heuristic
    }

    void execute_task(Task& t, bool counted_as_pinned) {
        if (counted_as_pinned) stats_.executed_pinned.fetch_add(1, std::memory_order_relaxed);
        else stats_.executed_flexible.fetch_add(1, std::memory_order_relaxed);
        t();
    }

    lf::ArrayMPMCQueue<Task> pinned_queue_;
    lf::ArrayMPMCQueue<Task> flex_queue_;
    std::vector<std::thread> pinned_workers_;
    std::vector<std::thread> flexible_workers_;
    std::atomic<bool> stop_;
    std::vector<int> pinned_core_ids_;
    Stats stats_{};
};

} // namespace concurrency
