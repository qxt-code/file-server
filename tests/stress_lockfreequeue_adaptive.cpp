#include "lockfreequeue/array_mpmc_queue.hpp"
#include "lockfreequeue/adaptive_blocking_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdio>

// A simple stress program (not a gtest) that can be compiled into test binary if desired.
// It runs multiple producers and consumers for a fixed duration and prints basic stats.
int main() {
    lf::ArrayMPMCQueue<int> q(1024);
    lf::AdaptiveConfig cfg;
    cfg.local_publish_period = 16;
    cfg.base_spin = 64;
    cfg.min_spin = 4;
    lf::AdaptiveBlockingQueue<lf::ArrayMPMCQueue<int>> adapter(1024);
    adapter.set_config(cfg);

    const int producers = 4;
    const int consumers = 4;
    const auto duration = std::chrono::seconds(3);
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> produced{0}, consumed{0};

    std::vector<std::thread> threads;
    for (int p=0;p<producers;++p) {
        threads.emplace_back([&, p]{
            int value = p * 1000000;
            while(!stop.load(std::memory_order_relaxed)) {
                if (adapter.push(value)) {
                    ++produced; ++value;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (int c=0;c<consumers;++c) {
        threads.emplace_back([&]{
            int v;
            while(!stop.load(std::memory_order_relaxed) || consumed.load() < produced.load()) {
                if (adapter.pop(v)) {
                    ++consumed;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(duration);
    stop.store(true);
    for (auto& t: threads) t.join();

    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    printf("Produced: %llu, Consumed: %llu, Throughput: %.2f ops/s, Mode=%s, FailureEMA=%.3f Fullness=%.3f\n",
        (unsigned long long)produced.load(),
        (unsigned long long)consumed.load(),
        produced.load() / secs,
        adapter.mode() == lf::AdaptiveMode::Spin ? "Spin" : "Block",
        adapter.failure_rate_ema(),
        adapter.fullness_last());
    return 0;
}
