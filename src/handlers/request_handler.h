#pragma once

#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <functional>


#include "protocol/request_parser.h"
#include "protocol/response_builder.h"
#include "types/message.h"

#include "nlohmann/json.hpp"
#include "common/debug.h"
#include "net/response_queue.h"
#include "types/context.h"
#include "types/enums.h"

namespace net {
class Connection;
}

namespace handlers {

using json = nlohmann::json;
using ConnectionPtr = std::shared_ptr<net::Connection>;
using protocol::RequestParser;
using protocol::ResponseBuilder;
using net::ResponseQueue;
using net::Connection;

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Ptr = std::shared_ptr<RequestHandler>;

    RequestHandler() {}
    RequestHandler(ConnectionContext::Ptr connection_context)
        : connection_context_(connection_context) {
        log_cpp20("[RequestHandler] constructed for fd=" + (connection_context ? std::to_string(connection_context->connection_id) : std::string("-1")));
    }
    virtual ~RequestHandler() {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    RequestHandler(RequestHandler&& other);
    RequestHandler& operator=(RequestHandler&& other);


    virtual void recvRequest();
    virtual void sendResponse(MessageType type);
    virtual void handle();

    virtual void onSuccess(const std::string& message);
    virtual void onFailed(int error_code, const std::string& error_message);

protected:
    ConnectionContext::Ptr connection_context_{nullptr};

    json jsonRequest;
    json jsonResponse;
    RequestParser requestParser;
    ResponseBuilder responseBuilder;

    // Handle a fully received REQUEST frame (decoupled from recvRequest framing logic)
    void onFrame(const Message &msg);

private:
};


} // namespace handlers