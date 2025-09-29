#include "mkdir_handler.h"

#include <iostream>

#include "common/debug.h"

void MKDIRHandler::handle() {
    std::cout << "mkdir handler handling" << std::endl;
    request_["command"] = "mkdir";
    if (command_.contains("params") && command_["params"].size() > 0) {
        request_["params"] = command_["params"][0];
    } else {
        std::cout << "mkdir command requires a directory name parameter" << std::endl;
        return;
    }
    send(MessageType::REQUEST, request_);
}