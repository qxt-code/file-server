#include "main_reactor.h"

#include <vector>
#include <atomic>
#include <random>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string>

#include "common/debug.h"
#include "concurrency/lf_thread_pool.h"
#include "reactor.h"
#include "io_reactor.h"
#include "connection.h"

namespace net {



bool MainReactor::listen_on(uint16_t port, std::string ip, int backlog) {
    listener_.listen_on(port, ip);
    
    listen_fd_ = listener_.get_fd();
    epoll_poller_.add_fd(listen_fd_, EPOLLIN | EPOLLET);
    return true;
}



void MainReactor::loop() {
    while (running_.load(std::memory_order_acquire)) {
        auto events_opt = epoll_poller_.poll(1000);
        if (!events_opt) {
            if (errno == EINTR) {
                RUNTIME_ERROR("epoll_wait interrupted by signal: %s", strerror(errno));
                continue;
            }
            else break;
        }
        auto events = *events_opt;
        if (events.empty()) continue;
        for (int i = 0; i < events.size(); ++i) {
            if (events[i].data.fd != listen_fd_) {
                error_cpp20("MainReactor: unexpected event on fd " + std::to_string(events[i].data.fd));
                continue;
            }
            int client_fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more incoming connections to accept
                    log_cpp20("MainReactor: no more incoming connections to accept");
                    break;
                }
                error_cpp20("MainReactor: accept failed: " + std::string(strerror(errno)));
                continue;
            }
            log_cpp20("MainReactor: new connection on fd " + std::to_string(client_fd));
            auto r = pick_reactor();
            if (!r) {
                error_cpp20("No IO reactor available, closing fd " + std::to_string(client_fd));
                ::close(client_fd);
                continue;
            }
            if (r->addConnection(client_fd) == false) {
                error_cpp20("Failed to add connection to IO reactor, closing fd " + std::to_string(client_fd));
                ::close(client_fd);
                continue;
            }
            
        }
    }
}


} // namespace net