#include "rm_handler.h"

#include "storage/file_manager.h"

namespace handlers {

void RMHandler::handle() {
    int fd = connection_context_->connection_id;
    if (fd < 0) {
        error_cpp20("Invalid connection ID");
        connection_context_->close_callback();
        return;
    }
    if (jsonRequest.find("params") == jsonRequest.end()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "Missing params");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    std::string file_name = jsonRequest["params"].value("file_name", "");
    if (file_name.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "Missing file_name");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }

    auto* file_manager = &storage::FileManager::getInstance();
    if (!file_manager->deleteFile(connection_context_->session_context->user_id, file_name)) {
        jsonResponse = responseBuilder.buildErrorResponse(404, "File not found or delete failed");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    jsonResponse = responseBuilder.build("Deleted: " + file_name);
    sendResponse(MessageType::RESPONSE);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
