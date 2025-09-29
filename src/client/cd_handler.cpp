#include "cd_handler.h"

#include <iostream>

void CDHandler::handle() {
    std::cout << "cd handler handling" << std::endl;
    request_["command"] = "cd";
    if (command_.contains("params") && command_["params"].size() > 0) {
        request_["params"] = command_["params"][0];
    } else {
        request_["params"] = "/";
    }
    send(MessageType::REQUEST, request_);
}