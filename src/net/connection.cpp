#include "connection.h"

#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

#include "handlers/handlers.h"
#include "handlers/request_handler.h"
#include "storage/file_manager.h"
#include "session/session_store.h"

namespace net {

Connection::Connection(int socket_fd, ReactorContext::Ptr reactor_context)
    : socket_fd(socket_fd), is_connected(true),
        connection_context_(std::make_shared<ConnectionContext>(reactor_context)) {
    if (socket_fd < 0) {
        error_cpp20("Invalid socket file descriptor");
    }
    if (!reactor_context) {
        error_cpp20("Reactor context is null");
    }
    connection_context_->connection_id = socket_fd;
    connection_context_->connection = this;
    connection_context_->close_callback = [this]() {
        this->close();
    };
    connection_context_->change_handler_callback = [this](const std::shared_ptr<handlers::RequestHandler>& new_handler) {
        this->change_handler(new_handler);
    };
    // TODO use a temporary session ctx
    // connection_context_->session_context = std::make_shared<SessionContext>(1);

    // storage::FileManager::getInstance().setUser(connection_context_->session_context->user_id);

    handler = std::make_shared<handlers::RequestHandler>(connection_context_);
}

Connection::~Connection() {
    this->close();
}

void Connection::on_readable() {
    if (!is_connected) {
        RUNTIME_ERROR("Connection is closed");
        return;
    }
    if (!handler) {
        RUNTIME_ERROR("No handler assigned for this connection");
        close();
        return;
    }
    handler->recvRequest();
}

void Connection::on_writable() {}
void Connection::on_error(int err) {}

void Connection::handle() {
    if (handler) {
        handler->handle();
    } else {
        RUNTIME_ERROR("No handler assigned for this connection");
        close();
    }
}

void Connection::change_handler(const std::shared_ptr<handlers::RequestHandler>& new_handler) {
    int fd = socket_fd;
    log_cpp20("[Connection] change_handler fd=" + std::to_string(fd) + " -> new handler type=" + std::string(typeid(*new_handler).name()));
    handler = new_handler;
}


void Connection::close() {
    if (is_connected) {
        // TODO handler ?
        ::close(socket_fd);
        is_connected = false;
    }
    // detach from session store
    if (connection_context_ && connection_context_->session_context) {
        auto sid = connection_context_->session_context->session_id;
        SessionStore::instance().detach_connection(socket_fd);
        // if session now empty, remove
        auto ctx = SessionStore::instance().get_by_session(sid);
        if (ctx && ctx->connection_fds.empty()) {
            SessionStore::instance().remove_session(sid);
        }
    }
    connection_context_->reactor_context->connection_close_callback(socket_fd);
    connection_context_->connection = nullptr;
}

bool Connection::is_open() const {
    return is_connected;
}

} // namespace net