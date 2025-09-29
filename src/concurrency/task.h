#pragma once
#include <cstdint>
#include <vector>

namespace concurrency {

enum class TaskClass : uint8_t {
    Flexible,        // Can run on any worker (default)
    PreferPinned,    // Prefer pinned workers for locality
    PinnedOnly       // Must run on pinned worker (if none exist -> fallback to flexible)
};

template<typename Task>
class SeriesTask {


private:
    std::vector<Task> tasks_;
};




}; // namespace concurrency