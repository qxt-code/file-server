#include "put_handler.h"

#include <vector>
#include <string>

#include "db/user_file_repository.h"
#include "types/user_file.h"
#include "storage/file_manager.h"
#include "storage/storage_error.h"
#include "utils/socket_transfer.h"
#include "utils/hash_utils.h"
#include "concurrency/lf_thread_pool.h"
#include "types/pending_large_upload.h"

namespace handlers {

void PUTHandler::recvRequest() {
    if (receiveCompleteMessage()) {
        std::lock_guard<std::mutex> lock(mutex_);
        msg_queue_.push(temp_msg_);
        log_cpp20("[PUTHandler] queued message length=" + std::to_string(temp_msg_.header.length) + " fd=" + std::to_string(connection_context_->connection_id));
        
        // Reset for next message
        total_received_ = 0;
        memset(&temp_msg_, 0, sizeof(temp_msg_));

        int submit_fd = connection_context_->connection_id;
        connection_context_->reactor_context->server_context->thread_pool->submit([self = shared_from_this(), submit_fd]() {
            log_cpp20("[PUTHandler] thread_pool executing handle fd=" + std::to_string(submit_fd));
            self->handle();
        });
    }
}

bool PUTHandler::receiveCompleteMessage() {
    int fd = connection_context_->connection_id;
    if (fd < 0) {
        error_cpp20("Invalid connection ID");
        connection_context_->close_callback();
        return false;
    }
    
    // First, receive the header if we haven't
    if (total_received_ == 0) {
        int nrecv = SocketTransfer::recvAll(fd, (char*)&temp_msg_.header, sizeof(temp_msg_.header));
        if (nrecv == 0 || nrecv == -3) {
            connection_context_->close_callback();
            return false;
        } else if (nrecv < 0) {
            error_cpp20("Failed to receive message header");
            connection_context_->close_callback();
            return false;
        }

        MessageType mtype = static_cast<MessageType>(temp_msg_.header.type);
        if (state_ == PUT_STATE::INIT) {
            if (mtype != MessageType::REQUEST) {
                error_cpp20("PUTHandler expected REQUEST header in INIT, got type=" + std::to_string(temp_msg_.header.type));
                connection_context_->close_callback();
                return false;
            }
        } else if (state_ == PUT_STATE::RECEIVING) {
            if (mtype != MessageType::PUT_DATA) {
                error_cpp20("PUTHandler expected PUT_DATA in RECEIVING, got type=" + std::to_string(temp_msg_.header.type));
                connection_context_->close_callback();
                return false;
            }
        }
        
        if (temp_msg_.header.length > sizeof(temp_msg_.body)) {
            error_cpp20("Message body too large: " + std::to_string(temp_msg_.header.length));
            connection_context_->close_callback();
            return false;
        }
    }

    // Then receive the body
    ssize_t toRecv = temp_msg_.header.length;
    while (total_received_ < toRecv) {
        int n = SocketTransfer::recvNonBlocking(fd, (char*)temp_msg_.body + total_received_, toRecv - total_received_);
        if (n == -1) {
            // Would block, return false to try again later
            return false;
        } else if (n == -2) {
            break;
        } else if (n <= 0) {
            connection_context_->close_callback();
            return false;
        }
        total_received_ += n;
    }
    
    return total_received_ == toRecv;
}

    
void PUTHandler::handle() {
    int fd = connection_context_->connection_id;
    if (fd < 0) {
        error_cpp20("Invalid connection ID");
        connection_context_->close_callback();
        return;
    }
    log_cpp20("[PUTHandler] handle state=" + std::to_string(static_cast<int>(state_)) + " fd=" + std::to_string(fd));
    if (state_ == PUT_STATE::INIT) {
        prepareToReceive();
    } else if (state_ == PUT_STATE::RECEIVING) {
        processMessages();
    } else if (state_ == PUT_STATE::COMPLETED) {
        onSuccess("File upload already completed");
    } else {
        onFailed(400, "Invalid state");
    }

}


void PUTHandler::prepareToReceive() {
    auto file_manager = &storage::FileManager::getInstance();
    if (!file_manager) {
        error_cpp20("FileManager is null in ConnectionContext");
        onFailed(500, "Internal server error");
        return;
    }
    // try {
        if (jsonRequest.find("params") == jsonRequest.end()) {
            onFailed(400, "Missing 'params' field");
            return;
        }
        std::string file_name = jsonRequest["params"].value("file_name", "");
        std::string file_hash = jsonRequest["params"].value("file_hash", "");
        int file_size = jsonRequest["params"].value("file_size", 0);
        if (file_name.empty()) {
            onFailed(400, "Missing file_name parameter");
            return;
        }
        if (file_hash.empty()) {
            onFailed(400, "Missing file_hash parameter");
            return;
        }
        if (file_size <= 0) {
            onFailed(400, "Invalid file_size parameter");
            return;
        }
        // Large file threshold 4MB
        constexpr uint64_t LARGE_THRESHOLD = 4ull * 1024ull * 1024ull;
        log_cpp20("[PUTHandler] evaluate large upload branch file_size=" + std::to_string(file_size) + " threshold=" + std::to_string(LARGE_THRESHOLD));
        if (static_cast<uint64_t>(file_size) > LARGE_THRESHOLD) {
            log_cpp20("[PUTHandler] large file path selected, issuing token for '" + file_name + "'");
            // Defer actual file creation to data channel after token validation.
            auto token = LargeUploadRegistry::instance().create(connection_context_->session_context->user_id, file_name, file_hash, file_size);
            log_cpp20("[PUTHandler] large upload token=" + token);
            jsonResponse = responseBuilder.buildLargePutInit(file_name, file_size, file_hash, token, "splice", 64*1024);
            sendResponse(MessageType::RESPONSE);
            // Roll back to base handler immediately (no in_put_upload flag set)
            rollbackToBaseHandler();
            return;
        } else {
            log_cpp20("[PUTHandler] small/normal file path selected (<= threshold)");
        }

        if (!file_manager->isFileExists(connection_context_->session_context->user_id, file_name)) {
            file_manager->createFile(connection_context_->session_context->user_id, file_name, file_hash, file_size);
        }
        int fd = file_manager->openFile(connection_context_->session_context->user_id, file_name, file_hash);
        if (fd < 0) {
            onFailed(500, "Internal server error");
            return;
        }
        file_handle_ = file_manager->getFileHandle(connection_context_->session_context->user_id, fd);
        if (!file_handle_) {
            onFailed(500, "Internal server error");
            return;
        }

        uint64_t current_pos = file_handle_->getPosition();
        if (current_pos >= static_cast<uint64_t>(file_size)) {

            state_ = PUT_STATE::COMPLETED;
            log_cpp20("[PUTHandler] file already complete file='" + file_name + "' size=" + std::to_string(current_pos) + " fd=" + std::to_string(connection_context_->connection_id));
            jsonResponse = responseBuilder.buildPutResponse("completed", file_name, current_pos, file_hash);
            sendResponse(MessageType::RESPONSE);
            rollbackToBaseHandler();
            return;
        }
        state_ = PUT_STATE::RECEIVING;
        connection_context_->in_put_upload = true;
        log_cpp20("[PUTHandler] prepared receiving file='" + file_name + "' position=" + std::to_string(current_pos) + " fd=" + std::to_string(connection_context_->connection_id));
        jsonResponse = responseBuilder.buildPutResponse("receiving", file_name, current_pos, file_hash);
        sendResponse(MessageType::RESPONSE);
    // } catch (const storage::FileError& e) {
    //     onFailed(400, e.what());
    //     return;
    // } catch (const std::exception& e) {
    //     onFailed(500, "Internal server error");
    //     return;
    // }
}

void PUTHandler::processMessages() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!msg_queue_.empty()) {
        Message msg = msg_queue_.front();
        msg_queue_.pop();
        lock.unlock();
        if (state_ != PUT_STATE::RECEIVING) {
            error_cpp20("Invalid state for receiving data");
            onFailed(400, "Invalid state for receiving data");
            return;
        }
        if (!file_handle_) {
            error_cpp20("File handle is null");
            onFailed(500, "Internal server error");
            return;
        }

        if (!incremental_sha1_) {
            incremental_sha1_ = std::make_unique<utils::IncrementalSHA1>();
        }
        uint64_t pos_before = file_handle_->getPosition();
        incremental_sha1_->update(msg.body, msg.header.length);
        // 写入文件
        int wn = file_handle_->writeBuffer(msg.body, msg.header.length);
        if (wn < 0) {
            state_ = PUT_STATE::ERROR;
            jsonResponse = responseBuilder.buildErrorResponse(500, "Write file chunk failed");
            sendResponse(MessageType::ERROR);
            incremental_sha1_.reset();
            rollbackToBaseHandler();
            return;
        }
        uint64_t pos_after = file_handle_->getPosition();
        log_cpp20("[PUTHandler] received chunk size=" + std::to_string(msg.header.length) + " pos_before=" + std::to_string(pos_before) + " pos_after=" + std::to_string(pos_after) + " fd=" + std::to_string(connection_context_->connection_id));
        ssize_t current_position = pos_after;
        int expected_size = jsonRequest["params"].value("file_size", 0);
        if (current_position >= expected_size) {

            std::string expected_hash = jsonRequest["params"].value("file_hash", "");
            std::string actual_hash = incremental_sha1_->final();
            std::string file_name = jsonRequest["params"].value("file_name", "");
            bool hash_ok = (!expected_hash.empty() && actual_hash == expected_hash);
            if (hash_ok) {
                state_ = PUT_STATE::COMPLETED;
                log_cpp20("[PUTHandler] upload completed file='" + file_name + "' size=" + std::to_string(current_position) + " hash_ok=1 fd=" + std::to_string(connection_context_->connection_id));
                jsonResponse = responseBuilder.buildPutResponse("completed", 
                    file_name, 
                    current_position, 
                    expected_hash);
                sendResponse(MessageType::RESPONSE);
                incremental_sha1_.reset();
                rollbackToBaseHandler();
            } else {
                error_cpp20("Hash verification failed: expected " + expected_hash + ", got " + actual_hash);
                state_ = PUT_STATE::ERROR;
                log_cpp20("[PUTHandler] upload failed hash mismatch file='" + file_name + "' size=" + std::to_string(current_position) + " fd=" + std::to_string(connection_context_->connection_id));
                jsonResponse = responseBuilder.buildErrorResponse(400, "File hash verification failed, please retry upload.");
                sendResponse(MessageType::ERROR);
                incremental_sha1_.reset();
                rollbackToBaseHandler();
            }
            return;
        }
        lock.lock();
    }
}

void PUTHandler::rollbackToBaseHandler() {
    // 保存当前上下文指针（本对象仍有权访问）
    auto old_ctx = connection_context_;
    if (!old_ctx) {
        error_cpp20("rollbackToBaseHandler: old_ctx null");
        return;
    }
    log_cpp20("[PUTHandler] initiating rollback state=" + std::to_string(static_cast<int>(state_)) + " pending_msgs=" + std::to_string(msg_queue_.size()) + " fd=" + std::to_string(old_ctx->connection_id));
    auto base = std::make_shared<RequestHandler>();
    // move基类部分
    dynamic_cast<RequestHandler&>(*base) = std::move(*this);
    // 由于当前对象已move，使用old_ctx执行回调
    if (old_ctx->change_handler_callback) {
        old_ctx->change_handler_callback(base);
        log_cpp20("[PUTHandler] rollbackToBaseHandler success fd=" + std::to_string(old_ctx->connection_id));
        old_ctx->in_put_upload = false; // 清除上传标记
    } else {
        error_cpp20("rollbackToBaseHandler: change_handler_callback missing");
    }
}



} // namespace handlers
