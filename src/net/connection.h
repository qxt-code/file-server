#pragma once

#include <netinet/in.h>

#include <memory>
#include <string>
#include <map>
#include <functional>

#include "io_reactor.h"
#include "types/context.h"

namespace net {


class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;

    Connection(int socket_fd, ReactorContext::Ptr reactor_context);
    ~Connection();

    void sendData(const std::string& data);
    std::string receiveData();

    void on_readable();
    void on_writable();
    void on_error(int err);

    void handle();

    void change_handler(const std::shared_ptr<handlers::RequestHandler>& new_handler);

    int getSocketFD() const;
    void close();
    bool is_open() const;

private:
    int socket_fd;
    bool is_connected;
    struct sockaddr_in client_address;
    std::shared_ptr<handlers::RequestHandler> handler;

    ConnectionContext::Ptr connection_context_{nullptr};
};

class ConnectionManager {
public:
    void addConnection(int user_id, std::shared_ptr<Connection> connection);
    void removeConnection(int user_id);
    std::shared_ptr<Connection> getConnection(int user_id);
   

private:
    std::map<int, std::shared_ptr<Connection>> connections;
};

}; // namespace net