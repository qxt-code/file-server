#pragma once

#include <functional>
#include <memory>

namespace concurrency {
    class LFThreadPool;
}

namespace net {
    class MainReactor;
    class IOReactor;
    class Connection;
    class ResponseQueue;
}

namespace handlers {
    class RequestHandler;
}

namespace session {
    class Session;
}

namespace storage {
    class FileManager;
}

class Server;


struct ServerContext {
    using Ptr = std::shared_ptr<ServerContext>;

    ServerContext() = default;
    ServerContext(Server* srv) : server(srv) {}

    Server* server{nullptr};
    std::shared_ptr<concurrency::LFThreadPool> thread_pool{nullptr};
};


struct ReactorContext {
    using Ptr = std::shared_ptr<ReactorContext>;

    ReactorContext() = default;
    ReactorContext(ServerContext::Ptr ctx) : server_context(ctx) {}

    int reactor_id;
    net::MainReactor* main_reactor{nullptr};
    net::IOReactor* io_reactor{nullptr};
    std::shared_ptr<net::ResponseQueue> response_queue{nullptr};

    std::function<void(int)> connection_close_callback{nullptr};

    ServerContext::Ptr server_context{nullptr};
};

struct SessionContext {
    using Ptr = std::shared_ptr<SessionContext>;

    SessionContext() = default;
    SessionContext(int uid) : user_id(uid) {}

    int user_id{-1};
    int session_id{-1};
    std::vector<int> connection_fds; // All connection fds associated with this session
    std::string session_token;
    time_t last_active_time{0};
};


struct ConnectionContext {
    using Ptr = std::shared_ptr<ConnectionContext>;

    ConnectionContext() = default;
    ConnectionContext(ReactorContext::Ptr ctx) : reactor_context(ctx) {}

    int connection_id{-1};

    net::Connection* connection{nullptr};

    std::function<void()> close_callback{nullptr};
    
    std::function<void(const std::shared_ptr<handlers::RequestHandler>&)> change_handler_callback{};

    ReactorContext::Ptr reactor_context{nullptr};
    SessionContext::Ptr session_context{nullptr};

    // 标记当前连接是否处于PUT文件上传数据流阶段，避免基础RequestHandler再次尝试解析后续数据块为JSON
    bool in_put_upload{false};
};
