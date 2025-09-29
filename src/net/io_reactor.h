#pragma once

#include "reactor.h"
#include <unordered_map>


#include "types/context.h"
#include "common/debug.h"
#include "epoll_poller.h"

// Forward declare
namespace net {
    class Connection;
}

namespace net {

class IOReactor : public ReactorBase, public std::enable_shared_from_this<IOReactor> {
public:
    using Ptr = std::shared_ptr<IOReactor>;
    explicit IOReactor(int id, int core_id = -1, ::ReactorContext::Ptr reactor_context = nullptr);

    bool addConnection(int fd);

    bool add_fd(int fd, uint32_t events);

    bool mod_fd(int fd, uint32_t events);

    bool del_fd(int fd);

    int id() const { return id_; }

protected:
    void loop() override;

private:
    int id_;
    EpollPoller epoll_poller_{};
    
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;

    ReactorContext::Ptr reactor_context_{nullptr};
};

} // namespace net
