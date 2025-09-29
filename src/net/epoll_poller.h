#pragma once

#include <sys/epoll.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <optional>

namespace net {

class EpollPoller {
public:
    EpollPoller();
    ~EpollPoller();

    bool add_fd(int fd, uint32_t events);
    bool modify_fd(int fd, uint32_t events);
    bool remove_fd(int fd);
    int epoll_fd() const { return epoll_fd_; }
    std::optional<std::vector<epoll_event>> poll(int timeout);

private:
    int epoll_fd_;
    struct epoll_event events_[1024];
};

} // namespace net
