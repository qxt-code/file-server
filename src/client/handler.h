#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "types/enums.h"
#include "types/message.h"

using nlohmann::json;

enum class ReceiveState {
    READ_HEADER,
    READ_BODY,
    PROCESSING
};


class Handler
{
public:
    Handler(int fd, json&& command) : fd_(fd), command_(std::move(command)) {}
    virtual ~Handler() = default;
    
    virtual void handle(){}
    virtual void send(const MessageType type, const json& json_msg);
    virtual void receive();
protected:
    int fd_;
    Message receive_msg_;
    ssize_t total_received_{0};
    ReceiveState state_{ReceiveState::READ_HEADER};
    json command_;
    json request_;
    json response_;
};


class PWDHandler : public Handler
{
public:
    PWDHandler(int fd, json&& command) : Handler(fd, std::move(command)) {}
    ~PWDHandler() override = default;
    void handle() override;
};