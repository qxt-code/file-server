#include "handler.h"

#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

#include "types/message.h"
#include "common/debug.h"
#include "client.h"



void Handler::send(const MessageType type, const json& json_msg) {
    std::string msg_str = json_msg.dump();
    Message msg;
    msg.header.type = static_cast<uint8_t>(type);
    msg.header.length = msg_str.size();
    memcpy(msg.body, msg_str.c_str(), msg.header.length);
    if (::send(fd_, reinterpret_cast<const char*>(&msg), sizeof(msg.header) + msg.header.length, 0) < 0) {
        error_cpp20("Failed to send message: " + std::string(strerror(errno)));
    } else {
        log_cpp20("Sent message: " + msg_str);
    }
}

void Handler::receive() {
    if (state_ == ReceiveState::READ_HEADER) {
        ssize_t n = ::recv(fd_, reinterpret_cast<char*>(&receive_msg_.header), sizeof(receive_msg_.header), MSG_DONTWAIT);
        if (n == 0) {
            error_cpp20("Connection closed by peer while receiving header");
            return;
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return; // try later
            }
            error_cpp20("Failed to receive message header: " + std::string(strerror(errno)));
            return;
        } else if (n < static_cast<ssize_t>(sizeof(receive_msg_.header))) {
            error_cpp20("Partial header read; deferring until full header available");
            return;
        }
        log_cpp20("Received message header: length=" + std::to_string(receive_msg_.header.length) + ", type=" + std::to_string(receive_msg_.header.type));
        if (receive_msg_.header.length > sizeof(receive_msg_.body)) {
            error_cpp20("Message body length exceeds buffer size");
            return;
        }
        total_received_ = 0;
        state_ = ReceiveState::READ_BODY;
    }

    if (state_ == ReceiveState::READ_BODY) {
        ssize_t body_length = receive_msg_.header.length;
        while (total_received_ < body_length) {
            ssize_t n = ::recv(fd_, reinterpret_cast<char*>(receive_msg_.body) + total_received_, body_length - total_received_, MSG_DONTWAIT);
            if (n == 0) {
                error_cpp20("Connection closed by peer while receiving body");
                return;
            } else if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    // Not all body data available yet; wait for next event
                    return;
                }
                error_cpp20("Receive body error: " + std::string(strerror(errno)));
                return;
            }
            total_received_ += n;
        }
        // Full body received
        state_ = ReceiveState::PROCESSING;
    }

    if (state_ != ReceiveState::PROCESSING) {
        return; // Not ready yet
    }

    std::string body_str(reinterpret_cast<char*>(receive_msg_.body), receive_msg_.header.length);
    log_cpp20("Received message body (once): " + body_str);
    try {
        response_ = json::parse(body_str);
    } catch (const std::exception& e) {
        error_cpp20(std::string("JSON parse error: ") + e.what());
        // Reset state machine to avoid lockup
        state_ = ReceiveState::READ_HEADER;
        return;
    }

    if (receive_msg_.header.type == static_cast<uint8_t>(MessageType::RESPONSE)) {
        log_cpp20("Received RESPONSE: " + response_.dump());
        if (response_.contains("responseMessage")) {
            std::string msgOut = response_["responseMessage"].get<std::string>();
            std::cout << msgOut << std::endl;
            // Heuristic: if this handler was a pwd or cd command then update path if looks like path
            if (!msgOut.empty() && msgOut[0] == '/') {
                if (auto c = Client::instance()) {
                    c->setPath(msgOut);
                }
            }
        }
    } else if (receive_msg_.header.type == static_cast<uint8_t>(MessageType::ERROR)) {
        log_cpp20("Received ERROR: " + response_.dump());
        if (response_.contains("errorMessage")) {
            std::cout << response_["errorMessage"].get<std::string>() << std::endl;
        }
    } else {
        log_cpp20("Received UNKNOWN message type: " + std::to_string(receive_msg_.header.type));
    }

    // Reset for next message
    state_ = ReceiveState::READ_HEADER;
    total_received_ = 0;
    memset(&receive_msg_, 0, sizeof(receive_msg_));
}


void PWDHandler::handle() {
    log_cpp20("Handling PWD command");
    request_ = command_;
    send(MessageType::REQUEST, request_);
}