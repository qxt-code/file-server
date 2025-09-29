#pragma once

#include <vector>
#include <thread>
#include <cstdio>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

namespace concurrency {

inline bool set_current_thread_affinity(int core_id) {
    if (core_id < 0) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_id, &set);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
        std::fprintf(stderr, "[affinity] failed to set core %d rc=%d\n", core_id, rc);
        return false;
    }
    return true;
}

inline bool set_current_thread_affinity(const std::vector<int>& cores) {
    if (cores.empty()) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int c : cores) {
        if (c >= 0) CPU_SET(c, &set);
    }
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
        std::fprintf(stderr, "[affinity] failed to set cores rc=%d\n", rc);
        return false;
    }
    return true;
}

inline int current_cpu() {
#ifdef __linux__
    return sched_getcpu();
#else
    return -1;
#endif
}

} // namespace concurrency
