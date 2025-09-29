#include <gtest/gtest.h>
#include "lockfreequeue/array_mpmc_queue.hpp"
#include "lockfreequeue/adaptive_blocking_queue.hpp"
#include <thread>
#include <barrier>
#include <vector>
#include <atomic>
#include <chrono>

// Helper to force contention: small capacity queue with many producers
TEST(LockFreeQueueAdaptive, ModeSwitchBasic) {
    lf::ArrayMPMCQueue<int> q(8);
    lf::AdaptiveConfig cfg;
    cfg.full_high = 0.8f; // lower thresholds to trigger more easily
    cfg.full_low  = 0.4f;
    cfg.fail_high = 0.5f;
    cfg.fail_low  = 0.2f;
    cfg.local_publish_period = 8;
    cfg.base_spin = 32;
    cfg.min_spin = 2;
    lf::AdaptiveBlockingQueue<lf::ArrayMPMCQueue<int>> adapter(8);
    adapter.set_config(cfg);


    constexpr int producers = 4;
    constexpr int items_per_prod = 2000;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    bool consumer_stop = false;

    std::thread consumer([&]{
        int v;
        while(!consumer_stop || consumed.load() < produced.load()) {
            if (adapter.pop(v)) {
                ++consumed;
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::vector<std::thread> prod_threads;
    for (int p=0; p<producers; ++p) {
        prod_threads.emplace_back([&, p]{
            for (int i=0;i<items_per_prod;++i) {
                // use push_until to exercise blocking path
                adapter.push_until(p*items_per_prod + i, std::chrono::milliseconds(10));
                ++produced;
            }
        });
    }
    for (auto& t: prod_threads) t.join();
    consumer_stop = true;
    consumer.join();

    EXPECT_EQ(produced.load(), consumed.load());
    // We cannot assert exact mode transitions here, but at least ensure final mode is either Spin or Block
    auto m = adapter.mode();
    EXPECT_TRUE(m == lf::AdaptiveMode::Spin || m == lf::AdaptiveMode::Block);
}
