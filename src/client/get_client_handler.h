#pragma once

#include "handler.h"
#include <fstream>
#include "utils/hash_utils.h"

class GetClientHandler : public Handler {
public:
    GetClientHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override;
    void receive() override;
private:
    enum class State { INIT, STREAMING, DONE, ERROR } state_{State::INIT};
    std::ofstream ofs_;
    utils::IncrementalSHA1 sha1_;
    std::string expected_hash_;
    uint64_t expected_size_{0};
    uint64_t received_{0};
    std::string file_name_;
};
