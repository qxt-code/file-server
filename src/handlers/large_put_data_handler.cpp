#include "large_put_data_handler.h"
#include "common/debug.h"
#include "protocol/response_builder.h"
#include "storage/storage_error.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <vector>
#include <openssl/sha.h>
#include "storage/file_manager.h"

namespace {
constexpr size_t SPLICE_CHUNK = 64 * 1024; // 64KB
}

namespace handlers {

void LargePutDataHandler::recvRequest() {
    // For now just reuse base behavior (JSON message). After READY -> will be replaced by splice loop in next task.
    if (state_ == State::INIT) {
        RequestHandler::recvRequest();
    } else if (state_ == State::READY) {
        tryStartReceiving();
    } else if (state_ == State::RECEIVING) {
        receiveLoop();
    }
}

bool LargePutDataHandler::consumeToken() {
    std::string token = jsonRequest["params"].value("token", "");
    std::string file_hash = jsonRequest["params"].value("file_hash", "");
    if (token.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "missing token");
        sendResponse(MessageType::ERROR);
        state_ = State::ERROR; return false;
    }
    PendingLargeUpload tmp;
    if (!LargeUploadRegistry::instance().consume(token, tmp)) {
        jsonResponse = responseBuilder.buildErrorResponse(403, "invalid or used token");
        sendResponse(MessageType::ERROR); state_ = State::ERROR; return false;
    }
    // Optional file_hash check
    if (!file_hash.empty() && !tmp.file_hash.empty() && file_hash != tmp.file_hash) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "file_hash mismatch");
        sendResponse(MessageType::ERROR); state_ = State::ERROR; return false;
    }
    plu_ = std::move(tmp);
    return true;
}

bool LargePutDataHandler::prepareFile() {
    auto &fm = storage::FileManager::getInstance();
    try {
        if (!fm.isFileExists(plu_.user_id, plu_.file_name)) {
            fm.createFile(plu_.user_id, plu_.file_name, plu_.file_hash, plu_.file_size);
        }
        int fd = fm.openFile(plu_.user_id, plu_.file_name, plu_.file_hash);
        if (fd < 0) return false;
        file_fd_ = fd; file_opened_ = true; return true;
    } catch (const std::exception &e) {
        error_cpp20(std::string("prepareFile error: ") + e.what());
        return false;
    }
}

void LargePutDataHandler::sendReady() {
    // jsonResponse = responseBuilder.buildSuccessResponse("large_put_channel_ready");
    jsonResponse = responseBuilder.build("large_put_channel_ready");
    sendResponse(MessageType::RESPONSE);
}

void LargePutDataHandler::handle() {
    if (state_ == State::INIT) {
        // Expect command == put_data_channel in jsonRequest
        std::string cmd = jsonRequest.value("command", "");
        if (cmd != "put_data_channel") {
            jsonResponse = responseBuilder.buildErrorResponse(400, "invalid command for data channel");
            sendResponse(MessageType::ERROR); state_ = State::ERROR; return;
        }
        if (!consumeToken()) return; // error already responded
        if (!prepareFile()) {
            jsonResponse = responseBuilder.buildErrorResponse(500, "open/create file failed");
            sendResponse(MessageType::ERROR); state_ = State::ERROR; return;
        }
        state_ = State::READY;
        sendReady();
        // Splice receiving will be implemented in next step (State::RECEIVING)
        return;
    }
}

void LargePutDataHandler::tryStartReceiving() {
    // Setup pipe once
    if (pipe_in_ == -1 || pipe_out_ == -1) {
        int pfd[2];
        if (pipe(pfd) != 0) {
            jsonResponse = responseBuilder.buildErrorResponse(500, "pipe failed");
            sendResponse(MessageType::ERROR); state_ = State::ERROR; return;
        }
        pipe_in_ = pfd[1];
        pipe_out_ = pfd[0];
        // set non-blocking
        fcntl(pipe_in_, F_SETFL, O_NONBLOCK);
        fcntl(pipe_out_, F_SETFL, O_NONBLOCK);
    }
    // Tune SO_RCVLOWAT (best-effort, only once)
    static thread_local bool lowat_set = false;
    if (!lowat_set) {
        long page = ::sysconf(_SC_PAGESIZE);
        if (page <= 0) page = 4096;
        // choose k so that k*page ~= 64KB (or min(file_size, 128KB))
        uint64_t target = std::min<uint64_t>(plu_.file_size, 128ull * 1024ull);
        uint64_t k = target / page; if (k == 0) k = 1; if (k > 32) k = 32; // cap
        int lowat = static_cast<int>(k * page);
        if (setsockopt(connection_context_->connection_id, SOL_SOCKET, SO_RCVLOWAT, &lowat, sizeof(lowat)) != 0) {
            log_cpp20("[LargePutDataHandler] SO_RCVLOWAT set failed: " + std::string(strerror(errno)));
        } else {
            log_cpp20("[LargePutDataHandler] SO_RCVLOWAT set to " + std::to_string(lowat));
        }
        lowat_set = true;
    }
    state_ = State::RECEIVING;
    receiveLoop();
}

void LargePutDataHandler::receiveLoop() {
    if (state_ != State::RECEIVING) return;
    int sockfd = connection_context_->connection_id;
    while (received_ < plu_.file_size) {
        size_t to_read = std::min<uint64_t>(SPLICE_CHUNK, plu_.file_size - received_);
        ssize_t moved = splice(sockfd, nullptr, pipe_in_, nullptr, to_read, SPLICE_F_MOVE | SPLICE_F_MORE);
        if (moved < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // wait for next EPOLLIN
                return; 
            }
            finalize(false, std::string("splice read error: ") + strerror(errno));
            return;
        }
        if (moved == 0) { // peer closed early
            finalize(false, "peer closed before expected size");
            return;
        }
        ssize_t written_total = 0;
        while (written_total < moved) {
            ssize_t w = splice(pipe_out_, nullptr, file_fd_, nullptr, moved - written_total, SPLICE_F_MOVE | SPLICE_F_MORE);
            if (w < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // allow epoll to wake again
                    return;
                }
                finalize(false, std::string("splice write error: ") + strerror(errno));
                return;
            }
            if (w == 0) {
                finalize(false, "unexpected zero write splice");
                return;
            }
            written_total += w;
        }
        received_ += moved;
        if (received_ >= plu_.file_size) break;
        // loop continues while data available (edge-trigger scenario) else return to epoll
    }
    if (received_ >= plu_.file_size) {
        bool ok = computeAndVerifyHash();
        finalize(ok, ok?"":"hash mismatch");
    }
}

bool LargePutDataHandler::computeAndVerifyHash() {
    // Map file and compute SHA1 (fallback: read in blocks if mmap undesired). Simple block read.
    ::lseek(file_fd_, 0, SEEK_SET);
    SHA_CTX ctx; SHA1_Init(&ctx);
    constexpr size_t BUF_SZ = 256 * 1024;
    std::vector<unsigned char> buf(BUF_SZ);
    uint64_t left = plu_.file_size;
    while (left > 0) {
        size_t chunk = left > BUF_SZ ? BUF_SZ : static_cast<size_t>(left);
        ssize_t rn = ::read(file_fd_, buf.data(), chunk);
        if (rn <= 0) return false;
        SHA1_Update(&ctx, buf.data(), rn);
        left -= rn;
    }
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1_Final(digest, &ctx);
    static const char* hex = "0123456789abcdef";
    std::string hexstr; hexstr.resize(SHA_DIGEST_LENGTH*2);
    for (int i=0;i<SHA_DIGEST_LENGTH;i++) { hexstr[i*2] = hex[digest[i]>>4]; hexstr[i*2+1] = hex[digest[i]&0xF]; }
    hash_verified_ = (hexstr == plu_.file_hash);
    return hash_verified_;
}

void LargePutDataHandler::finalize(bool success, const std::string& err) {
    if (state_ == State::COMPLETED || state_ == State::ERROR) return;
    if (success) {
        jsonResponse = responseBuilder.buildLargePutComplete(plu_.file_name, plu_.file_size, plu_.file_hash, true);
        sendResponse(MessageType::RESPONSE);
        state_ = State::COMPLETED;
    } else {
        // On failure attempt to rollback FileManager entry (best-effort)
        try {
            auto &fm = storage::FileManager::getInstance();
            if (fm.isFileExists(plu_.user_id, plu_.file_name)) {
                fm.deleteFile(plu_.user_id, plu_.file_name);
            }
        } catch (...) {
            // ignore
        }
        jsonResponse = responseBuilder.buildErrorResponse(500, err);
        sendResponse(MessageType::ERROR);
        state_ = State::ERROR;
        // On error attempt to remove file (best-effort)
        // (We would need FileManager API for delete; omitted for now)
    }
    closeResources();
}

void LargePutDataHandler::closeResources() {
    if (pipe_in_ != -1) { ::close(pipe_in_); pipe_in_ = -1; }
    if (pipe_out_ != -1) { ::close(pipe_out_); pipe_out_ = -1; }
    if (file_fd_ != -1) { ::close(file_fd_); file_fd_ = -1; }
}

} // namespace handlers
