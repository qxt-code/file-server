// Listener.cpp
#include "listener.h"
#include <sys/socket.h>
#include <unistd.h>     // for close()
#include <cstdio>       // for perror()
#include <cstring>      // for memset()
#include <arpa/inet.h>  // for inet_addr()

#include "common/debug.h"

namespace net {

Listener::Listener() : listen_fd_(-1) {}

Listener::~Listener() {
    if (listen_fd_ != -1) {
        ::close(listen_fd_);
    }
}

Listener::Listener(Listener&& other) noexcept : listen_fd_(other.listen_fd_) {
    if (this != &other)
    other.listen_fd_ = -1;
}

Listener& Listener::operator=(Listener&& other) noexcept {
    if (this != &other) {
        if (listen_fd_ != -1) {
            ::close(listen_fd_);
        }
        listen_fd_ = other.listen_fd_;
        other.listen_fd_ = -1;
    }
    return *this;
}

bool Listener::listen_on(uint16_t port, std::string ip) {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        RUNTIME_ERROR("socket() failed: %s", strerror(errno));
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        RUNTIME_ERROR("setsockopt() failed: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    struct sockaddr_in server_addr;
    ::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    server_addr.sin_port = htons(port);

    if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        RUNTIME_ERROR("bind() failed: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        RUNTIME_ERROR("listen() failed: %s", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    log_cpp20("Listening on " + ip + std::to_string(port));
    return true;
}

int Listener::accept_connection(struct sockaddr_in* client_addr) {
    if (listen_fd_ < 0) {
        return -1;
    }

    socklen_t client_addr_len = sizeof(struct sockaddr_in);
    int client_fd = -1;

    if (client_addr) {
        client_fd = accept(listen_fd_, (struct sockaddr*)client_addr, &client_addr_len);
    } else {
        client_fd = accept(listen_fd_, nullptr, nullptr);
    }

    if (client_fd < 0) {
        perror("accept() failed");
    }
    
    return client_fd;
}

int Listener::get_fd() const {
    return listen_fd_;
}

} // namespace net