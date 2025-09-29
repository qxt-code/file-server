#pragma once

#include "handler.h"

class MKDIRHandler
     : public Handler
{
public:
    MKDIRHandler(int fd, json&& command) : Handler(fd, std::move(command)) {}
    void handle() override;
};