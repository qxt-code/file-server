#include "request_handler.h"
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include "protocol/request_parser.h"
#include "protocol/response_builder.h"
#include "types/message.h"
#include "session/session.h"
#include "net/connection.h"

#include "nlohmann/json.hpp"
#include "common/debug.h"
#include "concurrency/lf_thread_pool.h"
#include "handlers/list_handler.h"
#include "handlers/cd_handler.h"
#include "handlers/pwd_handler.h"
#include "handlers/mkdir_handler.h"
#include "handlers/put_handler.h"
#include "handlers/rm_handler.h"
#include "handlers/rmdir_handler.h"
#include "handlers/register_handler.h"
#include "handlers/pubkey_handler.h"
#include "handlers/login_handler.h"
#include "handlers/get_handler.h"
#include "handlers/large_put_data_handler.h"

namespace handlers {

RequestHandler::RequestHandler(RequestHandler&& other)
      : connection_context_(std::move(other.connection_context_)),
        jsonRequest(std::move(other.jsonRequest)),
        jsonResponse(std::move(other.jsonResponse)),
        requestParser(std::move(other.requestParser)),
        responseBuilder(std::move(other.responseBuilder)) {}

RequestHandler& RequestHandler::operator=(RequestHandler&& other) {
    if (this != &other) {
        connection_context_ = std::move(other.connection_context_);

        jsonRequest = std::move(other.jsonRequest);
        jsonResponse = std::move(other.jsonResponse);
        requestParser = std::move(other.requestParser);
        responseBuilder = std::move(other.responseBuilder);
    }
    return *this;
}


void RequestHandler::recvRequest() {
    Message msg;
    int fd = connection_context_->connection_id;
    if (fd < 0) {
        error_cpp20("Invalid connection ID");
        connection_context_->close_callback();
        return;
    }
    if (connection_context_->in_put_upload) {
        // 正在处于PUT数据阶段，基础handler不再读取，等待PUTHandler的recvRequest处理
        log_cpp20("[RequestHandler] recvRequest ignored (in_put_upload=1) fd=" + std::to_string(fd));
        return;
    }
    log_cpp20("[RequestHandler] recvRequest begin fd=" + std::to_string(fd));
    ssize_t nrecv = recv(fd, &msg.header, sizeof(msg.header), MSG_WAITALL);
    if (nrecv == 0) {
        log_cpp20("Connection closed by peer on fd " + std::to_string(fd));
        connection_context_->close_callback();
        return;
    } else if (nrecv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, try again later
            log_cpp20("No data available to read on fd " + std::to_string(fd));
            return;
        }
        error_cpp20("Failed to receive data. Error: " + std::string(strerror(errno)));
        connection_context_->close_callback();
        return;
    }
    // 如果是文件数据帧（PUT_DATA），直接返回让当前 active handler (应为 PUTHandler) 的 recvRequest 处理
    if (static_cast<MessageType>(msg.header.type) == MessageType::PUT_DATA) {
        // 如果此时PUT已经结束（回到基础handler），我们就需要把多余的数据体读掉丢弃；否则会不断触发事件
        size_t to_discard = msg.header.length;
        size_t discarded = 0;
        while (discarded < to_discard) {
            uint8_t buf[4096];
            size_t chunk = std::min(sizeof(buf), to_discard - discarded);
            ssize_t n = recv(fd, buf, chunk, 0);
            if (n <= 0) {
                error_cpp20("Failed to discard leftover PUT_DATA body or connection closed");
                connection_context_->close_callback();
                return;
            }
            discarded += n;
        }
        log_cpp20("[RequestHandler] discarded leftover PUT_DATA length=" + std::to_string(msg.header.length) + " fd=" + std::to_string(fd));
        return; // 丢弃后不再继续处理
    }
    ssize_t toRecv = msg.header.length;
    if (toRecv > sizeof(msg.body)) {
        error_cpp20("Message body too large: " + std::to_string(toRecv));
        connection_context_->close_callback();
        return;
    }

    ssize_t totalRecv = 0;
    while (totalRecv < toRecv) {
        ssize_t n = recv(fd, msg.body + totalRecv, toRecv - totalRecv, MSG_WAITALL);
        if (n <= 0) {
            error_cpp20("Failed to receive data or connection closed");
            connection_context_->close_callback();
            return;
        }
        totalRecv += n;
    }

    // Defer parsing / dispatching to a dedicated method so recvRequest only does framing.
    onFrame(msg);
}

// New: decouple frame reception from parsing & dispatch logic
void RequestHandler::onFrame(const Message &msg) {
    int fd = connection_context_->connection_id;
    if (static_cast<MessageType>(msg.header.type) != MessageType::REQUEST) {
        error_cpp20("[RequestHandler] Unexpected non-REQUEST frame in base handler type=" + std::to_string(msg.header.type));
        return;
    }
    std::string rawRequest(reinterpret_cast<const char*>(msg.body), msg.header.length);
    log_cpp20("[RequestHandler] raw request body fd=" + std::to_string(fd) + ": " + rawRequest);
    auto ret = requestParser.parse(rawRequest);
    if (ret == std::nullopt) {
        error_cpp20("Failed to parse request: " + rawRequest);
        return;
    }
    jsonRequest = std::move(*ret);
    std::string command = jsonRequest.value("command", "");
    log_cpp20("[RequestHandler] parsed command='" + command + "' fd=" + std::to_string(fd));

    if (command == "put") {
        auto putHandler = std::make_shared<PUTHandler>();
        dynamic_cast<RequestHandler&>(*putHandler) = std::move(*shared_from_this());
        putHandler->connection_context_->change_handler_callback(putHandler);
        log_cpp20("[RequestHandler] early switched to PUTHandler fd=" + std::to_string(putHandler->connection_context_->connection_id));
        putHandler->handle();
        return;
    }

    // Other commands dispatched asynchronously to keep recv loop lightweight
    connection_context_->reactor_context->server_context->thread_pool->submit(
        [self = shared_from_this()]() {
            log_cpp20("[RequestHandler] thread_pool executing handle for fd=" + std::to_string(self->connection_context_->connection_id));
            self->handle();
        });
}

void RequestHandler::handle() {
    Message msg;
    auto connection = connection_context_->connection;
    if (!connection) {
        error_cpp20("Connection no longer exists");
        return;
    }
    auto response_queue_ = connection_context_->reactor_context->response_queue;
    if (!response_queue_) {
        error_cpp20("Response queue is null");
        return;
    }
    std::string command = jsonRequest.value("command", "");
    log_cpp20("[RequestHandler] handle dispatch command='" + command + "' fd=" + std::to_string(connection_context_->connection_id));
    if (command == "ls") {
        auto handler = std::make_shared<ListHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to ListHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "pwd") {
        auto handler = std::make_shared<PWDHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to PWDHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "cd") {
        auto handler = std::make_shared<CDHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to CDHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "mkdir") {
        auto handler = std::make_shared<MKDIRHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to MKDIRHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "put") {
        auto handler = std::make_shared<PUTHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to PUTHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "rm") {
        auto handler = std::make_shared<RMHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to RMHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "rmdir") {
        auto handler = std::make_shared<RMDIRHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to RMDIRHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "register") {
        auto handler = std::make_shared<RegisterHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to RegisterHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "pubkey") {
        auto handler = std::make_shared<PubKeyHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to PubKeyHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "login") {
        auto handler = std::make_shared<LoginHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to LoginHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "get") {
        auto handler = std::make_shared<GETHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to GETHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else if (command == "put_data_channel") {
        auto handler = std::make_shared<LargePutDataHandler>();
        dynamic_cast<RequestHandler&>(*handler) = std::move(*shared_from_this());
        handler->connection_context_->change_handler_callback(handler);
        log_cpp20("[RequestHandler] switched to LargePutDataHandler fd=" + std::to_string(handler->connection_context_->connection_id));
        handler->handle();
    } else {
        // TODO
        jsonResponse = responseBuilder.buildErrorResponse(400, "Unknown command");
        sendResponse(MessageType::ERROR);
    }
}

void RequestHandler::sendResponse(MessageType type) {
    auto raw_response = jsonResponse.dump();
    Message msg;
    msg.header.length = raw_response.size();
    log_cpp20("[RequestHandler] sendResponse fd=" + std::to_string(connection_context_->connection_id) + " type=" + std::to_string((int)type) + " body=" + raw_response);
    // TODO
    msg.header.type = static_cast<uint8_t>(type); // Assuming 1 is the type for response messages
    if (msg.header.length > sizeof(msg.body)) {
        RUNTIME_ERROR("Response too large to send: %u", msg.header.length);
        return;
    }
    std::memcpy(msg.body, raw_response.data(), msg.header.length);

    connection_context_->reactor_context->response_queue->submit(
        connection_context_->connection_id,
        std::move(msg));

}

void RequestHandler::onSuccess(const std::string& message) {
    jsonResponse = responseBuilder.build(message);
    sendResponse(MessageType::RESPONSE);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
    log_cpp20("[RequestHandler] onSuccess reset to base handler fd=" + std::to_string(connection_context_->connection_id));

}
void RequestHandler::onFailed(int error_code, const std::string& error_message) {
    jsonResponse = responseBuilder.buildErrorResponse(error_code, error_message);
    sendResponse(MessageType::ERROR);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
    log_cpp20("[RequestHandler] onFailed reset to base handler fd=" + std::to_string(connection_context_->connection_id) + " error_code=" + std::to_string(error_code));

}

} // namespace handlers