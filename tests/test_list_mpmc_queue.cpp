#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>

#include "lockfreequeue/list_mpmc_queue.hpp"

using lf::ListMPMCQueue;

struct NonTrivial {
    static std::atomic<int> constructed;
    static std::atomic<int> destroyed;
    int v;
    explicit NonTrivial(int x=0) : v(x) { constructed.fetch_add(1, std::memory_order_relaxed); }
    NonTrivial(const NonTrivial& o) : v(o.v) { constructed.fetch_add(1, std::memory_order_relaxed); }
    NonTrivial(NonTrivial&& o) noexcept : v(o.v) { constructed.fetch_add(1, std::memory_order_relaxed); }
    NonTrivial& operator=(const NonTrivial& o) { v=o.v; return *this; }
    NonTrivial& operator=(NonTrivial&& o) noexcept { v=o.v; return *this; }
    ~NonTrivial() { destroyed.fetch_add(1, std::memory_order_relaxed); }
};
std::atomic<int> NonTrivial::constructed{0};
std::atomic<int> NonTrivial::destroyed{0};

TEST(ListMPMCQueueBasic, EmptyInitially) {
    ListMPMCQueue<int> q;
    int out;
    EXPECT_FALSE(q.try_pop(out));
}

TEST(ListMPMCQueueBasic, SinglePushPop) {
    ListMPMCQueue<int> q;
    ASSERT_TRUE(q.try_push(42));
    int out=-1;
    ASSERT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 42);
    EXPECT_FALSE(q.try_pop(out));
}

TEST(ListMPMCQueueBasic, FIFOOrder) {
    ListMPMCQueue<int> q;
    for (int i=0;i<100;i++) q.try_push(i);
    for (int i=0;i<100;i++) { int out; ASSERT_TRUE(q.try_pop(out)); EXPECT_EQ(out,i); }
    int dummy; EXPECT_FALSE(q.try_pop(dummy));
}

TEST(ListMPMCQueueBasic, NonTrivialLifecycle) {
    NonTrivial::constructed.store(0); NonTrivial::destroyed.store(0);
    {
        ListMPMCQueue<NonTrivial> q;
        for (int i=0;i<50;i++) q.try_push(NonTrivial(i));
        for (int i=0;i<50;i++) { NonTrivial out; ASSERT_TRUE(q.try_pop(out)); EXPECT_EQ(out.v, i); }
        NonTrivial tmp; EXPECT_FALSE(q.try_pop(tmp));
    }
    // After queue destruction, all constructed instances should be eventually destroyed.
    EXPECT_EQ(NonTrivial::constructed.load() , NonTrivial::destroyed.load());
}

TEST(ListMPMCQueueConcurrency, MPMCLinearCount) {
    const int producers = 4;
    const int consumers = 4;
    const int per_prod = 5000;
    ListMPMCQueue<int> q;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::vector<std::thread> threads;

    for (int p=0;p<producers;p++) {
        threads.emplace_back([&,p]{
            for (int i=0;i<per_prod;i++) {
                q.try_push(p*per_prod + i);
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    std::vector<int> results;
    results.reserve(producers*per_prod);
    std::mutex res_mtx;
    for (int c=0;c<consumers;c++) {
        threads.emplace_back([&]{
            int value;
            while (consumed.load(std::memory_order_relaxed) < producers*per_prod) {
                if (q.try_pop(value)) {
                    {
                        std::lock_guard<std::mutex> lk(res_mtx);
                        results.push_back(value);
                    }
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t: threads) t.join();

    EXPECT_EQ(produced.load(), producers*per_prod);
    EXPECT_EQ(consumed.load(), producers*per_prod);
    // Optional ordering not checked (MPMC). Validate set equality.
    std::vector<int> sorted = results;
    std::sort(sorted.begin(), sorted.end());
    for (int i=0;i<producers*per_prod;i++) EXPECT_EQ(sorted[i], i);
}

TEST(ListMPMCQueueConcurrency, StressMixed) {
    ListMPMCQueue<int> q;
    const int threads_n = 6;
    const int ops = 20000;
    std::atomic<int> pushes{0}, pops{0};

    auto worker = [&]{
        std::mt19937 rng(std::random_device{}());
        for (int i=0;i<ops;i++) {
            if ((rng() & 1) == 0) {
                q.try_push(i);
                pushes.fetch_add(1, std::memory_order_relaxed);
            } else {
                int v; if (q.try_pop(v)) pops.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> ts;
    for (int i=0;i<threads_n;i++) ts.emplace_back(worker);
    for (auto& t: ts) t.join();

    // Drain remaining
    int v; while (q.try_pop(v)) pops.fetch_add(1, std::memory_order_relaxed);

    EXPECT_LE(pops.load(), pushes.load());
}

// Basic reclamation smoke test (can't deterministically assert counts during run)
TEST(ListMPMCQueueEBR, ReclamationSmoke) {
    ListMPMCQueue<int> q;
    for (int i=0;i<200;i++) q.try_push(i);
    int out; for (int i=0;i<200;i++) ASSERT_TRUE(q.try_pop(out));
    // Force destruction of queue at end which calls drain_all(); we just ensure behavior doesn't crash.
    SUCCEED();
}

// Longer run ensuring no duplicates / losses under higher load
TEST(ListMPMCQueueConcurrency, MPMCLongRunExactCoverage) {
    const int producers = 6;
    const int consumers = 6;
    const int per_prod = 20000; // 120K total items
    const int total = producers * per_prod;
    ListMPMCQueue<int> q;

    std::vector<std::thread> threads;
    for (int p=0; p<producers; ++p) {
        threads.emplace_back([&,p]{
            int base = p * per_prod;
            for (int i=0;i<per_prod;i++) {
                q.try_push(base + i);
            }
        });
    }

    std::vector<char> seen(total, 0);
    std::atomic<int> done{0};
    for (int c=0; c<consumers; ++c) {
        threads.emplace_back([&]{
            int v;
            while (done.load(std::memory_order_relaxed) < total) {
                if (q.try_pop(v)) {
                    if (v >=0 && v < total) {
                        // Detect duplicates
                        char expected = 0;
                        char* slot = &seen[v];
                        if (!__atomic_compare_exchange_n(slot, &expected, 1, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
                            ADD_FAILURE() << "duplicate value: " << v;
                        }
                    } else {
                        ADD_FAILURE() << "out of range value: " << v;
                    }
                    done.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t: threads) t.join();

    // Verify full coverage
    for (int i=0;i<total;i++) {
        if (!seen[i]) ADD_FAILURE() << "missing value: " << i;
    }
    ASSERT_EQ(done.load(), total);
}

// Concurrent test with non-trivial type objects
TEST(ListMPMCQueueConcurrency, MPMCNonTrivialConcurrent) {
    NonTrivial::constructed.store(0); NonTrivial::destroyed.store(0);
    ListMPMCQueue<NonTrivial> q;
    const int producers = 4;
    const int consumers = 4;
    const int per_prod = 4000; // 16K objects
    const int total = producers * per_prod;

    std::atomic<int> popped{0};
    std::vector<std::thread> threads;
    for (int p=0;p<producers;p++) {
        threads.emplace_back([&,p]{
            for (int i=0;i<per_prod;i++) {
                q.try_push(NonTrivial(p*per_prod + i));
            }
        });
    }
    std::atomic<int> max_seen{-1};
    for (int c=0;c<consumers;c++) {
        threads.emplace_back([&]{
            NonTrivial obj;
            while (popped.load(std::memory_order_relaxed) < total) {
                if (q.try_pop(obj)) {
                    // simple monotonic check not strict ordering, just track range
                    int v = obj.v;
                    int cur = max_seen.load(std::memory_order_relaxed);
                    while (v > cur && !max_seen.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
                    popped.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t: threads) t.join();
    EXPECT_EQ(popped.load(), total);
    // Drain ensures all retired nodes deleted
    // destruction of queue triggers drain_all. Then constructed == destroyed
    // (Pop path already destroys values early.)
    EXPECT_EQ(NonTrivial::constructed.load(), NonTrivial::destroyed.load());
}

// Random interleaving with duplicate detection using bitmap; also measures rough throughput
TEST(ListMPMCQueueConcurrency, MPMCRandomInterleave) {
    const int threads_n = 8;
    const int pushes_per_thread = 40000; // 320K total pushes
    const int total_ids = threads_n * pushes_per_thread;
    const int extra_pop_attempts_factor = 2; // additional pop attempts to ensure draining
    ListMPMCQueue<int> q;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::vector<char> seen(total_ids, 0);
    std::mutex seen_mtx;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> ts;
    for (int t=0; t<threads_n; ++t) {
        ts.emplace_back([&,t]{
            std::mt19937 rng((uint32_t)(std::random_device{}() ^ (t*11939)));
            // Pre-generate all IDs for this thread
            std::vector<int> ids(pushes_per_thread);
            for (int i=0;i<pushes_per_thread;i++) ids[i] = t*pushes_per_thread + i;
            // Shuffle to create interleaving diversity
            std::shuffle(ids.begin(), ids.end(), rng);

            size_t pos = 0;
            int pop_attempts = 0;
            int target_pop_attempts = pushes_per_thread * extra_pop_attempts_factor;
            while (pos < ids.size() || pop_attempts < target_pop_attempts) {
                bool do_push = pos < ids.size() && ((rng() & 3) != 0); // biased towards pushes until done
                if (do_push) {
                    q.try_push(ids[pos]);
                    push_count.fetch_add(1, std::memory_order_relaxed);
                    ++pos;
                } else {
                    int v; if (q.try_pop(v)) {
                        if (v>=0 && v < total_ids) {
                            std::lock_guard<std::mutex> lk(seen_mtx);
                            if (seen[v]) ADD_FAILURE() << "duplicate pop: " << v;
                            seen[v] = 1;
                        } else {
                            ADD_FAILURE() << "out of range id: " << v;
                        }
                        pop_count.fetch_add(1, std::memory_order_relaxed);
                    }
                    ++pop_attempts;
                }
            }
        });
    }
    for (auto& th : ts) th.join();

    // Drain remainder
    int v; while (q.try_pop(v)) {
        if (v>=0 && v<total_ids) {
            std::lock_guard<std::mutex> lk(seen_mtx);
            if (seen[v]) ADD_FAILURE() << "duplicate pop(drain): " << v;
            seen[v] = 1;
        } else {
            ADD_FAILURE() << "out of range id(drain): " << v;
        }
        pop_count.fetch_add(1, std::memory_order_relaxed);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();

    // Validate exact coverage
    for (int id=0; id<total_ids; ++id) {
        if (!seen[id]) ADD_FAILURE() << "missing id: " << id;
    }
    ASSERT_EQ(push_count.load(), total_ids);
    ASSERT_EQ(pop_count.load(), total_ids);

    if (const char* env = std::getenv("LF_QUEUE_VERBOSE")) {
        (void)env;
        std::cout << "RandomInterleave total_ids=" << total_ids
                  << " time_ms=" << ms
                  << " pushes=" << push_count.load()
                  << " pops=" << pop_count.load()
                  << " throughput_ops_per_sec=" << ((push_count.load()+pop_count.load())*1000ull/ std::max<int64_t>(1,ms))
                  << "\n";
    }
}
