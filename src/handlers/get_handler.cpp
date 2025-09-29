#include "get_handler.h"
#include "protocol/response_builder.h"
#include "common/debug.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utils/hash_utils.h"
#include "db/user_file_repository.h"
#include "db/file_repository.h" // kept for other potential uses
#include "cache/file_meta_cache.h"

// #ifdef ERROR
// #undef ERROR
// #endif

namespace handlers {

void GETHandler::handle() {
    auto params = jsonRequest.value("params", nlohmann::json::object());
    std::string file_name = params.value("file_name", "");
    uint64_t offset = params.value("offset", 0ULL);
    if (file_name.empty()) {
    jsonResponse = responseBuilder.buildErrorResponse(400, "missing file_name");
    sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    // Validate session/user
    if (!connection_context_->session_context) {
    jsonResponse = responseBuilder.buildErrorResponse(401, "unauthorized");
    sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    int user_id = connection_context_->session_context->user_id;

    std::string virtual_path = file_name;
    if (virtual_path.find('/') == std::string::npos) {
        virtual_path = "/" + virtual_path;
    }
    auto& userFileRepo = db::UserFileRepository::getInstance();
    auto user_file_opt = userFileRepo.getFileByPath(user_id, virtual_path);
    if (!user_file_opt) {
        jsonResponse = responseBuilder.buildErrorResponse(404, "virtual path not found");
        sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    if (user_file_opt->fileType != FileType::FILE) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "not a regular file");
        sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    size_t file_id = user_file_opt->fileId;
    auto meta_opt = FileMetaCache::instance().getById(file_id);
    if (!meta_opt) {
        jsonResponse = responseBuilder.buildErrorResponse(404, "file metadata not found");
        sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    uint64_t file_size = meta_opt->fileSize;
    if (offset > file_size) offset = file_size;
    std::string hash_code = meta_opt->hashCode;

    auto& got = storage::GlobalOpenTable::getInstance();
    auto fd_opt = got.openFile(hash_code);
    if (!fd_opt) {
        jsonResponse = responseBuilder.buildErrorResponse(500, "open physical file failed");
        sendResponse(static_cast<MessageType>(3));
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    int fd = *fd_opt;
    ::lseek(fd, offset, SEEK_SET);
    std::string file_hash = meta_opt->hashCode;
    jsonResponse = responseBuilder.buildGetInitResponse(file_name, offset, file_size, file_hash);
    sendResponse(MessageType::RESPONSE);

    const size_t CHUNK = 64*1024;
    std::vector<char> buf(CHUNK);
    while (true) {
        ssize_t rn = ::read(fd, buf.data(), buf.size());
        if (rn < 0) {
            error_cpp20("read error during get: " + std::string(strerror(errno)));
            break;
        }
        Message msg{};
        msg.header.type = static_cast<uint8_t>(MessageType::GET_DATA);
        if (rn == 0) {
            msg.header.length = 0;
            if (::send(connection_context_->connection_id, &msg, sizeof(msg.header), 0) < 0) {
                error_cpp20("send EOF frame failed: " + std::string(strerror(errno)));
            }
            break;
        }
        if (rn > (ssize_t)sizeof(msg.body)) rn = sizeof(msg.body);
        msg.header.length = rn;
        memcpy(msg.body, buf.data(), rn);
        if (::send(connection_context_->connection_id, &msg, sizeof(msg.header)+msg.header.length, 0) < 0) {
            error_cpp20("send chunk failed: " + std::string(strerror(errno)));
            break;
        }
    }
    got.closeFile(hash_code);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
