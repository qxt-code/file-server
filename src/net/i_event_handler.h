#pragma once

namespace net {

class IEventHandler {
public:
    using Ptr = std::shared_ptr<IEventHandler>;
    virtual ~IEventHandler() = default;
    virtual void on_readable(int fd) = 0;
    virtual void on_writable(int fd) = 0;
    virtual void on_error(int fd, int err) = 0;
};

};  // namespace net