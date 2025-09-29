#include "cd_handler.h"

#include <vector>
#include <string>

#include "db/user_file_repository.h"
#include "types/user_file.h"
#include "storage/file_manager.h"
#include "storage/storage_error.h"

namespace handlers {


    
void CDHandler::handle() {
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
    // try {
        file_manager->cd(connection_context_->session_context->user_id,jsonRequest.value("params", "/"));
    // } catch (const storage::FileError& e) {
    //     jsonResponse = responseBuilder.buildErrorResponse(400, e.what());
    //     sendResponse(MessageType::ERROR);
    //     connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
    //     return;
    // }
    auto ret = file_manager->pwd(connection_context_->session_context->user_id);
    jsonResponse = responseBuilder.build(ret);
    sendResponse(MessageType::RESPONSE);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
