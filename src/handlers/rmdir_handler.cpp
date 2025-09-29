#include "rmdir_handler.h"
#include "storage/file_manager.h"

namespace handlers {

void RMDIRHandler::handle() {
    if (jsonRequest.find("params") == jsonRequest.end()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "Missing params");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    std::string dir_name = jsonRequest["params"].value("dir_name", "");
    if (dir_name.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "Missing dir_name");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    auto* fm = &storage::FileManager::getInstance();
    if (!fm->removeDirectory(connection_context_->session_context->user_id, dir_name)) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "rmdir failed (not exists or not empty)");
        sendResponse(MessageType::ERROR);
    } else {
        jsonResponse = responseBuilder.build("Removed dir: " + dir_name);
        sendResponse(MessageType::RESPONSE);
    }
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
