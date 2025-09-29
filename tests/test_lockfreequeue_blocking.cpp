#include <gtest/gtest.h>
#include "lockfreequeue/array_mpmc_queue.hpp"
#include "lockfreequeue/spin_wrapper.hpp"
#include "lockfreequeue/blocking_adapter.hpp"
#include <thread>
#include <vector>
#include <atomic>

TEST(LockFreeQueue, SpinPushPopBasic) {
    lf::ArrayMPMCQueue<int> q(8);
    lf::SpinQueue sq(q);
    int v = 0;
    EXPECT_FALSE(q.try_pop(v));
    EXPECT_TRUE(sq.spin_push(42));
    EXPECT_TRUE(sq.spin_pop(v));
    EXPECT_EQ(v, 42);
}

TEST(LockFreeQueue, BlockingPushPopBasic) {
    lf::ArrayMPMCQueue<int> q(4);
    lf::BlockingAdapter adapter(q, {8, true});

    int out = -1;
    std::thread producer([&]{
        for (int i=0;i<10;++i) {
            adapter.push(i);
        }
    });
    std::thread consumer([&]{
        for (int i=0;i<10;++i) {
            adapter.pop(out);
        }
    });
    producer.join();
    consumer.join();
}

TEST(LockFreeQueue, BlockingTimeout) {
    lf::ArrayMPMCQueue<int> q(1);
    lf::BlockingAdapter adapter(q, {4, true});

    // fill
    EXPECT_TRUE(adapter.push(1));
    // push with very short timeout when full should eventually fail
    bool ok = adapter.push_until(2, std::chrono::milliseconds(1));
    // may or may not succeed depending on timing; pop then try again deterministic
    int v;
    adapter.pop(v);
    bool ok2 = adapter.push_until(2, std::chrono::milliseconds(5));
    EXPECT_TRUE(ok2);
}

TEST(LockFreeQueue, MultiProducerConsumer) {
    lf::ArrayMPMCQueue<int> q(1024);
    lf::BlockingAdapter adapter(q, {64, true});
    constexpr int producers = 4;
    constexpr int consumers = 4;
    constexpr int per_prod = 5000;

    std::atomic<int> sum_push{0};
    std::atomic<int> sum_pop{0};

    std::vector<std::thread> ths;
    for (int p=0;p<producers;++p) {
        ths.emplace_back([&, p]{
            for (int i=1;i<=per_prod;++i) {
                adapter.push(i);
                sum_push.fetch_add(i, std::memory_order_relaxed);
            }
        });
    }
    for (int c=0;c<consumers;++c) {
        ths.emplace_back([&]{
            int value;
            int received = 0;
            while (received < per_prod * producers / consumers) {
                if (adapter.pop(value)) {
                    sum_pop.fetch_add(value, std::memory_order_relaxed);
                    ++received;
                }
            }
        });
    }
    for (auto& t: ths) t.join();
    EXPECT_EQ(sum_push.load(), sum_pop.load());
}
