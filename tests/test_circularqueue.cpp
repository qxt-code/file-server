#include <gtest/gtest.h>
#include <thread>
#include <string>
#include <vector>
#include <memory>
#include "circularqueue/CircularQueue.hpp" // Your updated queue header

// Test fixture for basic setup
class CircularQueueTest : public ::testing::Test {
protected:
    // You can add setup/teardown logic here if needed
};

TEST_F(CircularQueueTest, BasicEnqueueDequeue) {
    CircularQueue<int> q(2);
    ASSERT_EQ(q.size(), 0);

    int val;
    ASSERT_TRUE(q.try_enqueue(100));
    ASSERT_EQ(q.size(), 1);

    ASSERT_TRUE(q.try_dequeue(val));
    ASSERT_EQ(q.size(), 0);
    ASSERT_EQ(val, 100);
}

TEST_F(CircularQueueTest, LValueAndRValueEnqueue) {
    CircularQueue<std::string> q(4);

    // 1. Enqueue an lvalue
    std::string lvalue_str = "hello";
    ASSERT_TRUE(q.try_enqueue(lvalue_str));
    ASSERT_EQ(lvalue_str, "hello"); // lvalue should be unchanged (copied)

    // 2. Enqueue an rvalue (literal)
    ASSERT_TRUE(q.try_enqueue("world"));

    // 3. Enqueue a moved lvalue
    std::string moved_str = "move me";
    ASSERT_TRUE(q.try_enqueue(std::move(moved_str)));
    // The state of moved_str is valid but unspecified, often empty.
    // ASSERT_TRUE(moved_str.empty()); // This is a common but not guaranteed outcome

    std::string out;
    ASSERT_TRUE(q.try_dequeue(out));
    EXPECT_EQ(out, "hello");

    ASSERT_TRUE(q.try_dequeue(out));
    EXPECT_EQ(out, "world");

    ASSERT_TRUE(q.try_dequeue(out));
    EXPECT_EQ(out, "move me");
}

TEST_F(CircularQueueTest, QueueFull) {
    CircularQueue<int> q(2);
    ASSERT_TRUE(q.try_enqueue(1));
    ASSERT_TRUE(q.try_enqueue(2));

    // Queue is now full, capacity is 2
    ASSERT_EQ(q.size(), 2);
    ASSERT_FALSE(q.try_enqueue(3)); // Should fail
    ASSERT_EQ(q.size(), 2);
}

TEST_F(CircularQueueTest, QueueEmpty) {
    CircularQueue<int> q(2);
    int val;

    ASSERT_EQ(q.size(), 0);
    ASSERT_FALSE(q.try_dequeue(val)); // Dequeue from empty queue should fail

    // Enqueue then dequeue to make it empty again
    q.try_enqueue(1);
    q.try_dequeue(val);

    ASSERT_EQ(q.size(), 0);
    ASSERT_FALSE(q.try_dequeue(val)); // Should fail again
}

TEST_F(CircularQueueTest, SPSC_ThreadSafety) {
    const size_t num_items = 1000000;
    auto q = std::make_unique<CircularQueue<int>>(1024);

    // Producer thread: enqueues numbers from 0 to num_items-1
    std::thread producer([&]() {
        for (size_t i = 0; i < num_items; ++i) {
            // Spin-wait until enqueue succeeds
            while (!q->try_enqueue(i)) {
                std::this_thread::yield();
            }
        }
    });

    // Consumer thread: dequeues and verifies the numbers are in order
    std::thread consumer([&]() {
        int val;
        for (size_t i = 0; i < num_items; ++i) {
            // Spin-wait until dequeue succeeds
            while (!q->try_dequeue(val)) {
                std::this_thread::yield();
            }
            // Crucial check: verify data integrity and order
            ASSERT_EQ(val, i);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(q->size(), 0);
}

// A helper struct to track object construction/destruction
struct LifecycleTracker {
    static std::atomic<int> creations;
    static std::atomic<int> destructions;
    static std::atomic<int> copies;
    static std::atomic<int> moves;

    static void reset() {
        creations = 0;
        destructions = 0;
        copies = 0;
        moves = 0;
    }

    LifecycleTracker() { creations++; }
    ~LifecycleTracker() { destructions++; }
    LifecycleTracker(const LifecycleTracker&) { creations++; copies++; }
    LifecycleTracker(LifecycleTracker&&) noexcept { creations++; moves++; }
    LifecycleTracker& operator=(const LifecycleTracker&) { copies++; return *this; }
    LifecycleTracker& operator=(LifecycleTracker&&) noexcept { moves++; return *this; }
};

std::atomic<int> LifecycleTracker::creations{0};
std::atomic<int> LifecycleTracker::destructions{0};
std::atomic<int> LifecycleTracker::copies{0};
std::atomic<int> LifecycleTracker::moves{0};


TEST_F(CircularQueueTest, ObjectLifecycle) {
    LifecycleTracker::reset();
    
    { // Scope to control queue lifetime
        CircularQueue<LifecycleTracker> q(5);
        
        // Test lvalue (should copy)
        LifecycleTracker tracker_lvalue;
        q.try_enqueue(tracker_lvalue);
        EXPECT_EQ(LifecycleTracker::copies, 1);
        EXPECT_EQ(LifecycleTracker::moves, 0);

        // Test rvalue (should move)
        q.try_enqueue(LifecycleTracker());
        EXPECT_EQ(LifecycleTracker::copies, 1);
        EXPECT_EQ(LifecycleTracker::moves, 1);

        // Dequeue and check destruction counts
        LifecycleTracker out_tracker;
        q.try_dequeue(out_tracker);
        q.try_dequeue(out_tracker);

    } // Queue is destroyed here

    // When the queue and all local objects are destroyed,
    // creations should equal destructions.
    EXPECT_EQ(LifecycleTracker::creations, LifecycleTracker::destructions);
}

TEST_F(CircularQueueTest, HighContentionMPMC) {
    const size_t num_producers = 4;
    const size_t num_consumers = 4;
    const size_t items_per_producer = 250000; // 25万
    const size_t total_items = num_producers * items_per_producer;
    
    // 使用一个足够大的队列以减少因队列满而导致的自旋等待
    CircularQueue<size_t> q(1024);

    // 原子计数器，用于消费者确认所有数据都已被处理
    std::atomic<size_t> items_consumed_count(0);
    // 消费者本地的计数器，用于验证数据没有重复
    std::vector<std::atomic<size_t>> consumer_received_counts(total_items);
    for (int i = 0; i < total_items; ++i) {
        consumer_received_counts[i].store(0);
    }

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // --- 启动生产者线程 ---
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&q, i, items_per_producer, num_producers]() {
            for (size_t j = 0; j < items_per_producer; ++j) {
                // 每个生产者生产唯一的数字，以便后续验证
                // 生产者0 生产 0, 4, 8, ...
                // 生产者 1 生产 1, 5, 9, ...
                size_t value = i + j * num_producers;
                // 自旋直到入队成功
                while (!q.try_enqueue(value)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // --- 启动消费者线程 ---
    for (size_t i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            // 只要还有未处理的数据就继续循环
            while (items_consumed_count.load(std::memory_order_acquire) < total_items) {
                size_t val;
                if (q.try_dequeue(val)) {
                    // 成功取出一个值
                    // 验证这个值在合法范围内
                    ASSERT_LT(val, total_items);
                    // 在对应的槽位上加1，如果最后结果不为1，则说明有数据重复或损坏
                    consumer_received_counts[val].fetch_add(1, std::memory_order_relaxed);
                    // 更新已处理的总数
                    items_consumed_count.fetch_add(1, std::memory_order_release);
                } else {
                    // 如果队列为空，让出CPU时间片
                    std::this_thread::yield();
                }
            }
        });
    }

    // 等待所有生产者完成
    for (auto& t : producers) {
        t.join();
    }

    // 等待所有消费者完成
    for (auto& t : consumers) {
        t.join();
    }

    // --- 最终验证 ---
    // 1. 验证总数是否正确 (检查数据丢失)
    ASSERT_EQ(items_consumed_count.load(), total_items);
    
    // 2. 验证每个数据是否只被消费了一次 (检查数据重复或损坏)
    for (size_t i = 0; i < total_items; ++i) {
        ASSERT_EQ(consumer_received_counts[i].load(), 1) << "Error on item " << i;
    }
}