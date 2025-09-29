#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include "lockfreequeue/list_mpmc_queue.hpp"

struct BenchConfig {
    int producers = 4;
    int consumers = 4;
    int pushes_per_producer = 200000; // total = producers * pushes_per_producer
    int batch_push = 1; // (future extension)
    bool pin_threads = false; // placeholder (not implemented)
};

static BenchConfig parse_args(int argc, char** argv) {
    BenchConfig cfg;
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        auto need = [&](int i){ if (i+1>=argc) { std::cerr << "Missing value after " << a << "\n"; std::exit(1);} }; 
        if (a == "--producers") { need(i); cfg.producers = std::atoi(argv[++i]); }
        else if (a == "--consumers") { need(i); cfg.consumers = std::atoi(argv[++i]); }
        else if (a == "--pushes") { need(i); cfg.pushes_per_producer = std::atoi(argv[++i]); }
        else if (a == "--batch") { need(i); cfg.batch_push = std::atoi(argv[++i]); }
        else if (a == "--pin") { cfg.pin_threads = true; }
        else if (a == "--help") {
            std::cout << "Usage: lockfreequeue_bench [options]\n"
                      << "  --producers N          number of producer threads (default 4)\n"
                      << "  --consumers N          number of consumer threads (default 4)\n"
                      << "  --pushes N             pushes per producer (default 200000)\n"
                      << "  --batch N              (reserved) batch size per push (default 1)\n"
                      << "  --pin                  (reserved) attempt to pin threads (not implemented)\n"
                      << "  --help                 show this help\n";
            std::exit(0);
        }
    }
    return cfg;
}

int main(int argc, char** argv) {
    using lf::ListMPMCQueue;

    BenchConfig cfg = parse_args(argc, argv);

    ListMPMCQueue<int> q;
    const int64_t total_pushes = int64_t(cfg.producers) * cfg.pushes_per_producer;
    std::atomic<int64_t> consumed{0};

    std::vector<std::thread> threads;
    threads.reserve(cfg.producers + cfg.consumers);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int p=0; p<cfg.producers; ++p) {
        threads.emplace_back([&,p]{
            int base = p * cfg.pushes_per_producer;
            for (int i=0; i < cfg.pushes_per_producer; ++i) {
                q.try_push(base + i);
            }
        });
    }

    for (int c=0; c<cfg.consumers; ++c) {
        threads.emplace_back([&,c]{
            int value;
            while (consumed.load(std::memory_order_relaxed) < total_pushes) {
                if (q.try_pop(value)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double sec = ns / 1e9;
    double mops = (double)total_pushes / 1e6 / sec; // million pushes per second (pops same scale)

    std::cout << "LOCKFREEQUEUE BENCH RESULT\n"
              << " producers=" << cfg.producers
              << " consumers=" << cfg.consumers
              << " pushes_per_producer=" << cfg.pushes_per_producer
              << " total_pushes=" << total_pushes
              << " time_sec=" << std::fixed << std::setprecision(6) << sec
              << " throughput_Mpushes_per_sec=" << std::setprecision(3) << mops
              << " total_ops_M/s=" << std::setprecision(3) << (mops*2)
              << "\n";

    if (consumed.load() != total_pushes) {
        std::cerr << "ERROR: consumed=" << consumed.load() << " expected=" << total_pushes << "\n";
        return 1;
    }
    return 0;
}
