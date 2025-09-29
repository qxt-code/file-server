#include "list_handler.h"

#include <iostream>

void ListHandler::handle() {
    std::cout << "list handler handling" << std::endl;
    request_ = command_;
    send(MessageType::REQUEST, request_);
}