#include "server.h"

#include <memory>

#include "net/io_reactor.h"
#include "common/debug.h"
#include "db/user_file_repository.h"



Server::Server(int port, const std::string& address)
    : address_(address), port_(port) {
    server_context_ = std::make_shared<ServerContext>();
    server_context_->thread_pool = 
        std::make_shared<concurrency::LFThreadPool>(2, 4, 1024, 1024, std::vector<int>{5, 6});
    log_cpp20("Server thread pool created with 2 pinned and 4 flexible threads.");
    
    main_reactor_ = std::make_shared<MainReactor>(0, std::vector<int>{1, 2, 3, 4}, server_context_);
    if (main_reactor_ == nullptr) {
        error_cpp20("Failed to create main reactor");
        return;
    }
    log_cpp20("Main reactor created on core 0 with 4 IO reactors on cores 1,2,3,4.");

    if (main_reactor_->listen_on(port, address) == false) {
        error_cpp20("Failed to start listening on " + address + ":" + std::to_string(port));
        return;
    }
    log_cpp20("Server listening on " + address + ":" + std::to_string(port));
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (main_reactor_) main_reactor_->start();
}

void Server::stop() {
    if (main_reactor_) main_reactor_->stop();
}