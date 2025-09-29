#pragma once

#include <string>
#include <memory>

#include "net/main_reactor.h"
#include "types/context.h"

using namespace net;

class Server {
public:
    Server(int port, const std::string& address = "127.0.0.1");
    ~Server();

    void start();
    void stop();

private:
    std::string address_;
    int port_;

    MainReactor::Ptr main_reactor_{nullptr};

    ServerContext::Ptr server_context_{nullptr};

};
