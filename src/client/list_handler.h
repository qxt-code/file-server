#pragma once

#include "handler.h"

class ListHandler
     : public Handler
{
public:
    ListHandler(int fd, json&& command) : Handler(fd, std::move(command)) {}
    void handle() override;
};