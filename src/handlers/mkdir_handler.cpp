#include "mkdir_handler.h"

#include <vector>
#include <string>

#include "db/user_file_repository.h"
#include "types/user_file.h"
#include "storage/file_manager.h"
#include "storage/storage_error.h"

namespace handlers {


    
void MKDIRHandler::handle() {
    int fd = connection_context_->connection_id;
    if (fd < 0) {
        error_cpp20("Invalid connection ID");
        connection_context_->close_callback();
        return;
    }

    auto file_manager = &storage::FileManager::getInstance();
    if (!file_manager) {
        error_cpp20("FileManager is null in ConnectionContext");
        jsonResponse = responseBuilder.buildErrorResponse(500, "Internal server error");
        sendResponse(MessageType::ERROR);
        return;
    }
    std::string ret;
    // try {
        if (!jsonRequest.contains("params") || jsonRequest["params"].is_null()) {
            error_cpp20("No directory name provided for mkdir");
            jsonResponse = responseBuilder.buildErrorResponse(400, "No directory name provided");
            sendResponse(MessageType::ERROR);
            connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
            return;

        }
        std::string path = jsonRequest["params"];
        file_manager->mkdir(connection_context_->session_context->user_id, path);
        ret = path + " created";
    // } catch (const storage::FileError& e) {
    //     jsonResponse = responseBuilder.buildErrorResponse(400, e.what());
    //     sendResponse(MessageType::ERROR);
    //     connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
    //     return;
    // }
    jsonResponse = responseBuilder.build(ret);
    sendResponse(MessageType::RESPONSE);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
