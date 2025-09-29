#include "client.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <termios.h>

#include "common/debug.h"
#include "list_handler.h"
#include "mkdir_handler.h"
#include "cd_handler.h"
#include "put_handler.h"
#include "large_put_client_handler.h"
#include "handler.h"
#include "get_pubkey_handler.h"
#include "auth/password_hash.h"
#include "auth/rsa_key_manager.h"
#include "register_client_handler.h"
#include "login_client_handler.h"
#include "get_client_handler.h"

class RMClientHandler : public Handler {
public:
    RMClientHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override {
        if (command_["params"].empty()) {
            std::cerr << "Usage: rm <file_name>" << std::endl;
            return;
        }
        request_ = {
            {"command", "rm"},
            {"params", { {"file_name", command_["params"][0]} }}
        };
        send(MessageType::REQUEST, request_);
    }
};

class RMDIRClientHandler : public Handler {
public:
    RMDIRClientHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override {
        if (command_["params"].empty()) {
            std::cerr << "Usage: rmdir <dir_name>" << std::endl;
            return;
        }
        request_ = {
            {"command", "rmdir"},
            {"params", { {"dir_name", command_["params"][0]} }}
        };
        send(MessageType::REQUEST, request_);
    }
};

static Client* g_client_instance = nullptr;

Client* Client::instance() { return g_client_instance; }

Client::Client() {
    g_client_instance = this;
    poller_.add_fd(STDIN_FILENO, EPOLLIN | EPOLLET);
}

Client::~Client() {
    ::close(fd_);
}

bool Client::connect(const std::string& ip, uint16_t port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        error_cpp20(strerror(errno));
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    if (::connect(fd_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error_cpp20(strerror(errno));
        ::close(fd_);
        return false;
    }
    int opt = 1;
    setsockopt(fd_, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt));

    poller_.add_fd(fd_, EPOLLIN | EPOLLET);
    log_cpp20("Connected to server " + ip + ":" + std::to_string(port) + " on fd " + std::to_string(fd_));
    if (!fetchPublicKeyBlocking()) {
        error_cpp20("Public key fetch failed; register will not encrypt correctly.");
    }
    return true;
}

bool Client::fetchPublicKeyBlocking(int timeout_ms) {
    auto& rsa = RSAKeyManager::getInstance();
    if (rsa.hasPublicKey()) return true; // already have it

    // Build and send request manually (without adding to handlers_)
    json pubReq = { {"command", "pubkey"} };
    std::string msg_str = pubReq.dump();
    Message msg{};
    msg.header.type = static_cast<uint8_t>(MessageType::REQUEST);
    msg.header.length = msg_str.size();
    memcpy(msg.body, msg_str.data(), msg_str.size());
    if (::send(fd_, reinterpret_cast<const char*>(&msg), sizeof(msg.header) + msg.header.length, 0) < 0) {
        error_cpp20("Failed to send pubkey request: " + std::string(strerror(errno)));
        return false;
    }

    // Switch socket temporarily to blocking recv with small timeouts until done or timeout_ms reached.
    int elapsed = 0;
    const int step = 200; // ms
    while (elapsed < timeout_ms && !rsa.hasPublicKey()) {
        // Use poll() system call for simplicity
        struct epoll_event evs[4];
        auto events = poller_.poll(step);
        if (events.has_value()) {
            for (auto & ev : events.value()) {
                if (ev.data.fd == fd_) {
                    // Minimal inline receive (blocking style): read header fully, then body.
                    MessageHeader hdr{};
                    ssize_t n = recv(fd_, &hdr, sizeof(hdr), MSG_DONTWAIT);
                    if (n <= 0) continue;
                    if (hdr.length > sizeof(msg.body)) {
                        error_cpp20("Pubkey response too large");
                        return false;
                    }
                    size_t got = 0;
                    std::string body;
                    body.resize(hdr.length);
                    while (got < hdr.length) {
                        ssize_t b = recv(fd_, body.data() + got, hdr.length - got, MSG_DONTWAIT);
                        if (b <= 0) break; got += b;
                    }
                    if (got != hdr.length) continue; // incomplete
                    try {
                        auto outer = json::parse(body);
                        if (outer.contains("responseMessage")) {
                            auto innerStr = outer["responseMessage"].get<std::string>();
                            auto inner = json::parse(innerStr);
                            if (inner.contains("pubkey")) {
                                if (rsa.loadPublicKeyPEM(inner["pubkey"].get<std::string>())) {
                                    log_cpp20("Server public key loaded successfully (blocking phase)");
                                    return true;
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        error_cpp20(std::string("Pubkey parse error: ") + e.what());
                        return false;
                    }
                }
            }
        }
        elapsed += step;
    }
    return rsa.hasPublicKey();
}

void Client::run() {
    while (!stop_) {
    std::string user = username().empty() ? "guest" : username();
    std::string path = this->path().empty() ? "/" : this->path();
    // Simple ANSI colors: username in green, path in blue
    std::string prompt = "[";
    prompt += (token().empty() ? user : "\033[32m" + user + "\033[0m");
    prompt += " ";
    prompt += "\033[34m" + path + "\033[0m";
    prompt += "]$ ";
        std::cout << prompt << std::flush;
        auto events = poller_.poll(-1);
        if (!events.has_value()) {
            continue;
        }
        for (const auto& event : events.value()) {
            if (event.data.fd == STDIN_FILENO) {
                char buffer[1024];
                ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead - 1] = '\0';
                    log_cpp20("Input: " + std::string(buffer));
                    auto command = parser_.parseCommand(buffer);
                    log_cpp20("Parsed Command: " + command.dump());
                    std::string cmd_str = command["command"];
                    std::shared_ptr<Handler> handler;
                    if (cmd_str == "q") {
                        stop_ = true;
                        break;
                    } else if (cmd_str == "pwd") {
                        log_cpp20("Creating PWDHandler");
                        handler = std::make_shared<PWDHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "ls") {
                        log_cpp20("Creating ListHandler");
                        handler = std::make_shared<ListHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "cd") {
                        log_cpp20("Creating CDHandler");
                        handler = std::make_shared<CDHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "mkdir") {
                        log_cpp20("Creating MKDIRHandler");
                        handler = std::make_shared<MKDIRHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "put") {
                        bool large = false;
                        try {
                            if (command.contains("params") && !command["params"].empty()) {
                                std::string probe_path = command["params"][0];
                                if (std::filesystem::exists(probe_path) && std::filesystem::is_regular_file(probe_path)) {
                                    auto sz = std::filesystem::file_size(probe_path);
                                    if (sz >= 4ull * 1024ull * 1024ull) large = true;
                                }
                            }
                        } catch (...) {}
                        if (large) {
                            log_cpp20("Creating LargePutClientHandler");
                            handler = std::make_shared<LargePutClientHandler>(fd_, std::move(command));
                        } else {
                            log_cpp20("Creating PUTHandler");
                            handler = std::make_shared<PUTHandler>(fd_, std::move(command));
                        }
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "get") {
                        log_cpp20("Creating GetClientHandler");
                        handler = std::make_shared<GetClientHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "rm") {
                        log_cpp20("Creating RMClientHandler");
                        handler = std::make_shared<RMClientHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "rmdir") {
                        log_cpp20("Creating RMDIRClientHandler");
                        handler = std::make_shared<RMDIRClientHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "register") {
                        log_cpp20("Creating RegisterClientHandler");
                        handler = std::make_shared<RegisterClientHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else if (cmd_str == "login") {
                        log_cpp20("Creating LoginClientHandler");
                        handler = std::make_shared<LoginClientHandler>(fd_, std::move(command));
                        handlers_[fd_] = handler;
                    } else {
                        log_cpp20("Unknown command: " + cmd_str);
                        continue;
                    }
                    if (handler) {
                        handler->handle();
                    }
                }
            } else {
                int fd = event.data.fd;
                if (handlers_.find(fd) != handlers_.end()) {
                    handlers_[fd]->receive();
                } else {
                    error_cpp20("No handler for fd: {}" + std::to_string(fd));
                }
            }
        }
    }
}

void Client::stop() {
    stop_ = true;
}