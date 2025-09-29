#pragma once
#include "handler.h"
#include <string>
#include <filesystem>
#include <optional>
#include <nlohmann/json.hpp>

// Handler for large file uploads using the optimized two-connection path
class LargePutClientHandler : public Handler {
public:
    LargePutClientHandler(int control_fd, nlohmann::json command)
        : Handler(control_fd, std::move(command)) {}

    void handle() override;
    void receive() override; // handle responses on control channel

private:
    enum class State { INIT, WAIT_TOKEN, DATA_CHANNEL_CONNECT, SENDING, COMPLETED, ERROR } state_ = State::INIT;

    std::string file_path_;
    std::string file_name_;
    uint64_t file_size_ = 0;
    std::string file_hash_;

    std::string upload_token_;
    int data_fd_ = -1;
    uint64_t bytes_sent_ = 0;

    bool openLocalFile();
    bool connectDataChannel();
    bool sendDataChannelInit();
    bool sendFileSplice();
    void closeData();
};
