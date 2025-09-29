#pragma once

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <memory>

#include "common/debug.h"
#include "i_event_handler.h"
#include "types/message.h"
#include "lockfreequeue/array_mpmc_queue.hpp"

namespace net {

class ResponseQueue {

public:
    using Ptr = std::shared_ptr<ResponseQueue>;
    ResponseQueue(size_t capacity = 1024) : queue_(capacity) {
        eventfd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (eventfd_ < 0) {
            error_cpp20("eventfd create failed: " + std::string(strerror(errno)));
            return;
        }
    }
    ~ResponseQueue() {
        if (eventfd_ >= 0) {
            ::close(eventfd_);
        }
    }

    int getEventFd() const { return eventfd_; }

    bool push(int fd, Message&& response) {
        for (int i = 0; i < 3; ++i) {
            if (queue_.try_push(ResponseTask{fd, std::move(response)})) {
                return true;
            }
        }
        return false;
    }

    

    bool submit(int fd, Message&& response);

    void on_readable();

private:
    struct ResponseTask {
        int fd{-1};
        Message message;
    };

    bool pop(ResponseTask& response) {
        for (int i = 0; i < 3; ++i) {
            if (queue_.try_pop(response)) {
                return true;
            }
        }
        return false;
    }

    int eventfd_{-1};
    lf::ArrayMPMCQueue<ResponseTask> queue_;
};
} // namespace net