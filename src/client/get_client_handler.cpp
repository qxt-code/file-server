// Removed UTF-8 BOM if present
#include "get_client_handler.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include "common/debug.h"
#include "types/message.h" // for MAX_MESSAGE_SIZE

// Define a local alias consistent with server side naming to avoid magic numbers
#ifndef MESSAGE_BODY_MAX
#define MESSAGE_BODY_MAX (MAX_MESSAGE_SIZE - sizeof(MessageHeader))
#endif

void GetClientHandler::handle() {
    if (command_["params"].empty()) {
        std::cerr << "Usage: get <file_name>" << std::endl;
        return;
    }
    file_name_ = command_["params"][0];
    json req = {
        {"command", "get"},
        {"params", { {"file_name", file_name_} }}
    };
    request_ = req;
    send(MessageType::REQUEST, request_);
}

void GetClientHandler::receive() {
  
    MessageHeader hdr{};
    ssize_t n = ::recv(fd_, &hdr, sizeof(hdr), MSG_DONTWAIT);
    if (n <= 0) {
        return; // try later
    }
    if (hdr.type == static_cast<uint8_t>(MessageType::GET_DATA)) {
        if (hdr.length == 0) { // EOF
            if (ofs_.is_open()) ofs_.close();
            std::string actual = sha1_.final();
            bool ok = (expected_hash_.empty() || actual == expected_hash_);
            std::cout << (ok ? "Download OK" : "Hash mismatch: " + actual + " != " + expected_hash_) << std::endl;
            state_ = ok?State::DONE:State::ERROR;
            return;
        }
        if (hdr.length > MESSAGE_BODY_MAX) {
            std::cerr << "Chunk too large" << std::endl;
            state_ = State::ERROR; return;
        }
        std::vector<char> buf(hdr.length);
        size_t got = 0; while (got < hdr.length) {
            ssize_t r = ::recv(fd_, buf.data()+got, hdr.length-got, MSG_DONTWAIT); if (r <=0) return; got += r;
        }
        if (!ofs_.is_open()) {
            ofs_.open(file_name_, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!ofs_) { std::cerr << "Failed open local file for write" << std::endl; state_ = State::ERROR; return; }
        }
        ofs_.write(buf.data(), buf.size());
        sha1_.update(buf.data(), buf.size());
        received_ += buf.size();
        return;
    }

    if (hdr.length > MESSAGE_BODY_MAX) return;
    std::string body; body.resize(hdr.length);
    size_t got = 0; while (got < hdr.length) {
        ssize_t r = ::recv(fd_, body.data()+got, hdr.length-got, MSG_DONTWAIT); if (r<=0) return; got += r; }
    try { response_ = json::parse(body); } catch(...) { return; }
    if (response_.contains("responseMessage")) {
        auto innerStr = response_["responseMessage"].get<std::string>();
        try { auto inner = json::parse(innerStr); 
            if (inner.value("status", "") == "get_init") {
                expected_hash_ = inner.value("fileHash", "");
                expected_size_ = inner.value<unsigned long long>("fileSize", 0ULL);
            }
        } catch(...) {}
    }
}
