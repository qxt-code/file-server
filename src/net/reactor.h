#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <sys/epoll.h>
#include <unistd.h>

#include "i_event_handler.h"
#include "epoll_poller.h"
#include "concurrency/cpu_affinity.h"
#include "common/debug.h"

// Basic Reactor interface and main/io reactor skeleton.
// This is a lightweight framework demonstrating integration with lock-free queues.
// It does not yet implement full production error handling.

namespace net {

struct FdEvent {
    int fd{-1};
    uint32_t events{0};
};

class ReactorBase {
public:
    ReactorBase(int core_id = -1): core_id_(core_id) {}
    virtual ~ReactorBase() { stop(); }

    // Usage: create reactor with a desired core id.
    // Example: auto r = std::make_unique<IOReactor>(idx, core_mapping[idx]); r->start();
    // Pass -1 to disable pinning.

    bool start() {
        running_.store(true, std::memory_order_release);

        thread_ = std::thread(&ReactorBase::run_with_affinity, this);
        return true;
    }

    void stop() {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false)) {
            if (thread_.joinable()) thread_.join();
        }
    }

    int epoll_fd() const { return epoll_poller_.epoll_fd(); }

protected:
    virtual void loop() = 0;
    void run_with_affinity() {
        if (core_id_ >= 0) {
            if (!concurrency::set_current_thread_affinity(core_id_)) {
                RUNTIME_ERROR("[reactor] failed to bind core %d\n", core_id_);
            }
        }
        loop();
    }
protected:
    std::atomic<bool> running_{false};
    EpollPoller epoll_poller_{};
    std::thread thread_{};
    int core_id_{-1};
};

// Forward declaration
class IOReactor;

using TaskFn = std::function<void()>;

} // namespace net
