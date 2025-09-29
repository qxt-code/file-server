#include "epoll_poller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>
#include <iostream>

#include <common/debug.h>

namespace net {


EpollPoller::EpollPoller() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        RUNTIME_ERROR("Failed to create epoll file descriptor: %s", strerror(errno));
    }
}

EpollPoller::~EpollPoller() {
    if (epoll_fd_ != -1)
        close(epoll_fd_);
}

bool EpollPoller::add_fd(int fd, uint32_t events) {
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
        RUNTIME_ERROR("Failed to add file descriptor to epoll: %s", strerror(errno));
        return false;
    }
    return true;
}

bool EpollPoller::modify_fd(int fd, uint32_t events) {
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1) {
        RUNTIME_ERROR("Failed to modify file descriptor in epoll: %s", strerror(errno));
        return false;
    }
    return true;
}

bool EpollPoller::remove_fd(int fd) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        RUNTIME_ERROR("Failed to remove file descriptor from epoll: %s", strerror(errno));
        return false;
    }
    return true;
}

std::optional<std::vector<epoll_event>> EpollPoller::poll(int timeout = -1) {
    std::vector<epoll_event> active_events;
    struct epoll_event events[1024];
    int num_events = epoll_wait(epoll_fd_, events, 1024, timeout);

    if (num_events < 0) {
        if (errno == EINTR) {
            log_cpp20("epoll_wait interrupted by signal: " + std::string(strerror(errno)));
        } else {
            error_cpp20("epoll_wait failed: " + std::string(strerror(errno)));
            return std::nullopt;
        } 
    }

    for (int i = 0; i < num_events; ++i) {
        active_events.push_back(events[i]);
    }

    return active_events;
}

} // namespace net