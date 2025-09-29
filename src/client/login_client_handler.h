#pragma once
#include "handler.h"
#include <termios.h>
#include <unistd.h>
#include "auth/password_hash.h"
#include "auth/rsa_key_manager.h"

class LoginClientHandler : public Handler {
public:
    LoginClientHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override;
    void receive() override;
};
