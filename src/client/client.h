#pragma once

#include <unordered_map>
#include <memory>

#include "command_parser.h"
#include "net/epoll_poller.h"
#include "handler.h"

using namespace net;
class Client
{
public:
    Client();
    ~Client();
    // Global accessor (lightweight) so individual handlers can update client state
    static Client* instance();

    bool connect(const std::string& ip, uint16_t port);
    void run();
    void stop();
    // Ensure got the server public key (blocking). Returns true on success.
    bool fetchPublicKeyBlocking(int timeout_ms = 3000);

    void setToken(const std::string& t) { auth_token_ = t; }
    const std::string& token() const { return auth_token_; }
    void setUsername(const std::string& u) { current_username_ = u; }
    const std::string& username() const { return current_username_; }
    void setPath(const std::string& p) { current_path_ = p; }
    const std::string& path() const { return current_path_; }
private:
    int fd_{-1};
    bool stop_{false};
    CommandParser parser_{};
    EpollPoller poller_{};
    std::string auth_token_{}; // JWT token stored after login
    std::string current_username_{}; // set after login
    std::string current_path_{"/"}; // updated via pwd/cd responses

    std::unordered_map<int, std::shared_ptr<Handler>> handlers_;
};