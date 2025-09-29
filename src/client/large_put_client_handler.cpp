#include "large_put_client_handler.h"
#include "common/debug.h"
#include "types/message.h"
#include "utils/hash_utils.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <thread>

using json = nlohmann::json;

static constexpr size_t LARGE_THRESHOLD = 4ull * 1024ull * 1024ull; // 4MB

void LargePutClientHandler::handle() {
    if (command_["params"].empty()) {
        std::cerr << "Usage: put <file_path>" << std::endl;
        state_ = State::ERROR; return;
    }
    file_path_ = command_["params"][0].get<std::string>();
    if (!std::filesystem::exists(file_path_) || !std::filesystem::is_regular_file(file_path_)) {
        std::cerr << "File not found or not regular: " << file_path_ << std::endl; state_ = State::ERROR; return;
    }
    file_name_ = std::filesystem::path(file_path_).filename().string();
    file_size_ = std::filesystem::file_size(file_path_);
    file_hash_ = utils::HashUtils::calculateFileSHA1(file_path_);
    if (file_hash_.empty()) { std::cerr << "Failed to compute file hash" << std::endl; state_ = State::ERROR; return; }
    if (file_size_ < LARGE_THRESHOLD) {
        std::cerr << "File below large threshold; use normal put handler." << std::endl; state_ = State::ERROR; return;
    }

    request_ = {
        {"command", "put"},
        {"params", {
            {"file_name", file_name_},
            {"file_size", file_size_},
            {"file_hash", file_hash_}
        }}
    };
    state_ = State::WAIT_TOKEN;
    send(MessageType::REQUEST, request_);
}

void LargePutClientHandler::receive() {
    if (state_ == State::WAIT_TOKEN) {
        Handler::receive();
        if (response_.empty()) return;
        std::string status = response_.value("status", "");
        if (status == "large_put_init") {
            upload_token_ = response_.value("uploadToken", "");
            if (upload_token_.empty()) {
                std::cerr << "Missing upload token" << std::endl;
                state_ = State::ERROR;
                return;
            }
            if (!connectDataChannel()) {
                state_ = State::ERROR;
                return;
            }
            if (!sendDataChannelInit()) {
                state_ = State::ERROR;
                return;
            }
            if (!sendFileSplice()) {
                state_ = State::ERROR;
                return;
            }
            state_ = State::SENDING;
            // wait for completion
            for (int i=0;i<200 && state_==State::SENDING;i++) {
                Handler::receive();
                if (!response_.empty()) {
                    auto st = response_.value("status", "");
                    if (st == "large_put_complete") {
                        bool ok = response_.value("hashOk", false);
                        std::cout << "\n✓ Large upload complete (hashOk=" << (ok?"true":"false") << ")" << std::endl;
                        state_ = ok?State::COMPLETED:State::ERROR;
                        break;
                    } else if (st == "error") {
                        std::cerr << "✗ Error: " << response_.value("errorMessage","unknown") << std::endl;
                        state_ = State::ERROR; break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            closeData();
        } else if (status == "completed") {
            // instant upload (already exists)
            std::cout << "⚡ File already exists on server (instant)." << std::endl;
            state_ = State::COMPLETED;
        } else if (status == "error") {
            std::cerr << "✗ Server error: " << response_.value("errorMessage","unknown") << std::endl;
            state_ = State::ERROR;
        }
    }
}

bool LargePutClientHandler::connectDataChannel() {
    // For simplicity reuse same server/port assumed; TODO: parameterize host/port if different
    // We need original control socket peer address
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(fd_, (sockaddr*)&addr, &len) != 0) {
        perror("getpeername"); return false;
    }
    data_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (data_fd_ < 0) {
        perror("socket");
        return false;
    }
    // set blocking temporarily for connect simplicity
    int flags = fcntl(data_fd_, F_GETFL, 0);
    fcntl(data_fd_, F_SETFL, flags & ~O_NONBLOCK);
    if (::connect(data_fd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("connect");
        close(data_fd_);
        data_fd_=-1;
        return false;
    }
    // restore nonblocking
    fcntl(data_fd_, F_SETFL, flags);
    return true;
}

bool LargePutClientHandler::sendDataChannelInit() {
    // Send JSON frame: {command: put_data_channel, params:{token, file_hash}}
    json init = {
        {"command","put_data_channel"},
        {"params", {
            {"token", upload_token_},
            {"file_hash", file_hash_}
        }
    }};
    std::string dump = init.dump();
    Message msg;
    msg.header.type = static_cast<uint8_t>(MessageType::REQUEST);
    if (dump.size() > sizeof(msg.body)) {
        std::cerr << "Init JSON too large" << std::endl;
        return false;
    }
    msg.header.length = dump.size();
    memcpy(msg.body, dump.data(), dump.size());
    ssize_t sent = ::send(data_fd_, &msg, sizeof(msg.header)+dump.size(), 0);
    if (sent < 0) {
        perror("send init");
        return false;
    }
    return true;
}

bool LargePutClientHandler::sendFileSplice() {
    int file_fd = ::open(file_path_.c_str(), O_RDONLY);
    if (file_fd < 0) {
        perror("open file");
        return false;
    }
    uint64_t remaining = file_size_;
    uint64_t last_report = 0;
    while (remaining > 0) {
        ssize_t s = ::sendfile(data_fd_, file_fd, nullptr, remaining);
        if (s < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            perror("sendfile");
            close(file_fd);
            return false;
        }
        if (s == 0) break;
        remaining -= s;
        bytes_sent_ += s;
        if (bytes_sent_ - last_report > 512*1024 || remaining==0) {
            double progress = (double)bytes_sent_ / file_size_ * 100.0;
            std::cout << "\r📤 Large Upload: " << std::fixed << std::setprecision(1) << progress << "% (" << bytes_sent_ << "/" << file_size_ << ")" << std::flush;
            last_report = bytes_sent_;
        }
    }
    close(file_fd);
    std::cout << "\nAwaiting server finalize..." << std::endl;
    return remaining == 0;
}

void LargePutClientHandler::closeData() {
    if (data_fd_ != -1) { ::close(data_fd_); data_fd_ = -1; }
}
