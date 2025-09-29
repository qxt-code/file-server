#include "io_reactor.h"
#include "reactor.h"
#include <unordered_map>
#include <functional>
#include <system_error>
#include <cstring>
#include <cerrno>
#include <vector>
#include <chrono>
#include <sys/eventfd.h>

#include "common/debug.h"
#include "response_queue.h"
#include "connection.h"

namespace net {


IOReactor::IOReactor(int id, int core_id, ReactorContext::Ptr reactor_context)
        : ReactorBase(core_id), id_(id), reactor_context_(reactor_context) {
        ResponseQueue::Ptr response_queue = std::make_shared<ResponseQueue>(1024);
        if (!response_queue) {
            error_cpp20("Failed to create response queue");
        }
        reactor_context_->io_reactor = this;
        reactor_context_->response_queue = response_queue;
        reactor_context_->reactor_id = id_;
        reactor_context_->connection_close_callback = [this](int fd) {
            this->del_fd(fd);
        };
        int event_fd = response_queue->getEventFd();
        if (!add_fd(event_fd, EPOLLIN)) {
            ::close(event_fd);
            error_cpp20("eventfd add to epoll failed: " + std::string(strerror(errno)));
        }
    }

bool IOReactor::addConnection(int fd) {
    auto conn = std::make_shared<Connection>(fd, reactor_context_);
    if (!conn) {
        error_cpp20("Failed to create connection for fd "+ std::to_string(fd));
        ::close(fd);
        return false;
    } else {
        log_cpp20("IOReactor " + std::to_string(id_) + ": New connection created for fd " + std::to_string(fd));
    }
    connections_[fd] = conn;

    return add_fd(fd, EPOLLIN | EPOLLET);
}

bool IOReactor::add_fd(int fd, uint32_t events) {
    if (epoll_poller_.add_fd(fd, events) == false) {
        error_cpp20("Failed to add fd to epoll: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}

bool IOReactor::mod_fd(int fd, uint32_t events) {
    if (epoll_poller_.modify_fd(fd, events) == false) {
        RUNTIME_ERROR("%s", strerror(errno));
        return false;
    }
    return true;
}

bool IOReactor::del_fd(int fd) {
    if (epoll_poller_.remove_fd(fd) == false) {
        RUNTIME_ERROR("%s", strerror(errno));
        return false;
    }
    connections_.erase(fd);
    return true;
}


void IOReactor::loop() {
    while (running_.load(std::memory_order_acquire)) {
        auto events_opt = epoll_poller_.poll(1000);
        if (!events_opt) {
            if (errno == EINTR) {
                RUNTIME_ERROR("epoll_wait interrupted by signal: %s", strerror(errno));
                continue;
            }
            else break;
        }
        auto events = *events_opt;
        if (events.empty()) {
            // log_cpp20("IOReactor " + std::to_string(id_) + ": epoll_wait timeout with no events");
            continue;
        }

        for (int i = 0; i < events.size(); ++i) {
            if (events[i].data.fd == reactor_context_->response_queue->getEventFd()) {
                reactor_context_->response_queue->on_readable();
                continue;
            }
            int fd = events[i].data.fd;
            log_cpp20("IOReactor " + std::to_string(id_) + ": event on fd " + std::to_string(fd));
            auto it = connections_.find(fd);
            if (it == connections_.end()) continue;
            uint32_t ev = events[i].events;
            auto conn = it->second;
            if (ev & (EPOLLERR|EPOLLHUP)) {
                conn->on_error(0);
                continue;
            }
            if (ev & EPOLLIN) conn->on_readable();
            if (ev & EPOLLOUT) conn->on_writable();
            // TODO submit to thread pool
            // pool_->submit([handler, fd, ev]{
        }
    }
}

} // namespace net
