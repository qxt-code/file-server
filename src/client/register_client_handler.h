#pragma once
#include "handler.h"
#include <termios.h>
#include <unistd.h>
#include "auth/password_hash.h"
#include "auth/rsa_key_manager.h"

// Handles interactive user registration on client side
class RegisterClientHandler : public Handler {
public:
    RegisterClientHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override;
};
