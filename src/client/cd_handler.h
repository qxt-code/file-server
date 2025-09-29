#pragma once

#include "handler.h"

class CDHandler
     : public Handler
{
public:
    CDHandler(int fd, json&& command) : Handler(fd, std::move(command)) {}
    void handle() override;
};