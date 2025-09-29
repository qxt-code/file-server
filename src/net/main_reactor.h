#pragma once
#include <vector>
#include <atomic>
#include <random>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "common/debug.h"
#include "concurrency/lf_thread_pool.h"
#include "reactor.h"
#include "io_reactor.h"
#include "epoll_poller.h"
#include "listener.h"
#include "types/context.h"

namespace net {

class MainReactor : public ReactorBase {
public:
    using Ptr = std::shared_ptr<MainReactor>;

    explicit MainReactor(int core_id = -1, std::vector<int> cpu_cores = {},
                        ServerContext::Ptr server_context = nullptr)
    : ReactorBase(core_id), 
        reactor_context_(std::make_shared<ReactorContext>(server_context)) {
        reactor_context_->main_reactor = this;
        if (cpu_cores.empty()) {
            io_reactors_.push_back(std::make_shared<IOReactor>(1, -1, reactor_context_));
            io_reactors_.back().get()->start();
        } else {
            for (int i = 0; i < cpu_cores.size(); ++i) {
                io_reactors_.push_back(std::make_shared<IOReactor>(i, cpu_cores[i], reactor_context_));
                io_reactors_.back().get()->start();
            }
        }
    }

    ~MainReactor() override {
        handlers_.erase(listen_fd_);
        ::close(listen_fd_);
    }

    void set_io_reactors(std::vector<IOReactor::Ptr> reactors) {
        io_reactors_ = std::move(reactors);
    }

    bool listen_on(uint16_t port, std::string ip = "127.0.0.1", int backlog = 1024);


protected:
    void loop() override;

    IOReactor::Ptr pick_reactor() {
        if (io_reactors_.empty()) return nullptr;
        // simple round robin
        auto idx = rr_.fetch_add(1,std::memory_order_relaxed) % io_reactors_.size();
        return io_reactors_[idx];
    }

private:
    int listen_fd_{-1};
    Listener listener_;
    std::unordered_map<int, IEventHandler::Ptr> handlers_;
    std::vector<IOReactor::Ptr> io_reactors_;
    std::atomic<uint64_t> rr_{0};

    ReactorContext::Ptr reactor_context_{nullptr};
};

} // namespace net
