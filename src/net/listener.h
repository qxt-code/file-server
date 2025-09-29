#pragma once

#include <cstdint> // For uint16_t
#include <netinet/in.h> // For sockaddr_in
#include <string>

namespace net {

class Listener {
public:
    Listener();
    ~Listener();

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    Listener(Listener&& other) noexcept;
    Listener& operator=(Listener&& other) noexcept;

    bool listen_on(uint16_t port, std::string ip);

    int accept_connection(struct sockaddr_in* client_addr = nullptr);

    int get_fd() const;

private:
    int listen_fd_;
};

} // namespace net