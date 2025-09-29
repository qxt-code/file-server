#include "put_handler.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <iomanip>
#include <sys/socket.h>
#include <cstring>
#include <thread>

#include "common/debug.h"
#include "types/message.h"
#include "utils/hash_utils.h"

void PUTHandler::handle() {
    if (command_["params"].empty()) {
        std::cerr << "✗ Error: Missing file path parameter" << std::endl;
        std::cerr << "Usage: put <file_path>" << std::endl;
        return;
    }
    
    std::string file_path = command_["params"][0];
    log_cpp20("PUT: Uploading file: " + file_path);
    
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "✗ Error: File does not exist: " << file_path << std::endl;
        return;
    }
    
    // Check if it's a regular file
    if (!std::filesystem::is_regular_file(file_path)) {
        std::cerr << "✗ Error: Path is not a regular file: " << file_path << std::endl;
        return;
    }
    
    // Get file information
    std::filesystem::path path(file_path);
    std::string file_name = path.filename().string();
    size_t file_size = std::filesystem::file_size(file_path);
    
    // Calculate file hash
    std::cout << "🔍 Calculating file hash..." << std::endl;
    std::string file_hash = utils::HashUtils::calculateFileSHA1(file_path);
    if (file_hash.empty()) {
        std::cerr << "✗ Error: Failed to calculate file hash" << std::endl;
        return;
    }
    
    // Prepare PUT request
    request_ = {
        {"command", "put"},
        {"params", {
            {"file_name", file_name},
            {"file_size", static_cast<int>(file_size)},
            {"file_hash", file_hash}
        }}
    };
    
    // Store file information for upload
    file_path_ = file_path;
    file_size_ = file_size;
    bytes_sent_ = 0;
    upload_state_ = UploadState::INIT;
    
    log_cpp20("PUT: File info - name: " + file_name + ", size: " + std::to_string(file_size) + ", hash: " + file_hash);
    
    // Send initial request
    send(MessageType::REQUEST, request_);
}

void PUTHandler::receive() {
    // Call parent's receive method to handle the response (only RESPONSE / ERROR types set response_)
    Handler::receive();
    // If response_ is empty or lacks status we may have received a non-response frame (ignore)
    if (response_.empty()) {
        return;
    }
    
    // Process the response for PUT operation
    if (response_.contains("status")) {
        std::string status = response_["status"];
        if (status == "receiving") {
            log_cpp20("PUT: Server ready to receive file, starting upload...");
            std::cout << "Starting file upload..." << std::endl;
            // resume upload
            size_t server_received = 0;
            if (response_.contains("fileSize")) {
                server_received = response_["fileSize"].get<size_t>();
            }
            bytes_sent_ = server_received;
            
            if (bytes_sent_ >= file_size_) {
                std::cout << "\n⚡ Instant upload: server already has full file." << std::endl;
                upload_state_ = UploadState::COMPLETED;
                return;
            }
            startFileUpload();
        } else if (status == "completed") {

            std::string srv_name = response_.value("fileName", "");
            size_t srv_size = response_.value("fileSize", 0);
            std::string srv_hash = response_.value("fileHash", "");
            bool first_phase = (upload_state_ == UploadState::INIT);
            if (first_phase && bytes_sent_ == 0 && file_path_.empty() && command_.contains("params") && command_["params"].size() > 0) {
                // 
            }

            if (first_phase) {
                std::cout << "\n⚡ File already exists on server (instant upload)." << std::endl;
                if (!file_path_.empty() && std::filesystem::exists(file_path_)) {
                    std::string local_hash = utils::HashUtils::calculateFileSHA1(file_path_);
                    if (!srv_hash.empty() && !local_hash.empty() && local_hash != srv_hash) {
                        std::cout << "⚠ Hash mismatch: local(" << local_hash << ") != server(" << srv_hash << ")" << std::endl;
                        std::cout << "  You may re-upload with --force (not implemented) or rename the file." << std::endl;
                    }
                }
            } else {
                std::cout << "\n✓ File upload completed successfully!" << std::endl;
            }
            std::cout << "  File: " << srv_name << std::endl;
            std::cout << "  Size: " << srv_size << " bytes" << std::endl;
            if (!srv_hash.empty()) {
                std::cout << "  Hash: " << srv_hash << std::endl;
            }
            upload_state_ = UploadState::COMPLETED;
        } else if (status == "error") {
            std::string error_msg = response_.value("errorMessage", "Unknown error");
            int error_code = response_.value("errorCode", 0);
            std::cerr << "✗ Upload failed [" << error_code << "]: " << error_msg << std::endl;

            if (error_msg.find("hash verification failed") != std::string::npos) {
                std::cerr << "自动重试上传..." << std::endl;
                bytes_sent_ = 0;
                upload_state_ = UploadState::INIT;
                startFileUpload();
            } else {
                upload_state_ = UploadState::ERROR;
            }
        }
    } else if (response_.contains("errorMessage")) {
        // Handle general error response
        std::string error_msg = response_["errorMessage"].get<std::string>();
        std::cerr << "✗ Server error: " << error_msg << std::endl;
        upload_state_ = UploadState::ERROR;
    }
}

void PUTHandler::startFileUpload() {
    if (upload_state_ != UploadState::INIT && upload_state_ != UploadState::UPLOADING) {
        return;
    }
    upload_state_ = UploadState::UPLOADING;

    std::ifstream file(file_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file for reading: " << file_path_ << std::endl;
        upload_state_ = UploadState::ERROR;
        return;
    }

    // 断点续传：跳转到断点位置
    if (bytes_sent_ > 0) {
        file.seekg(bytes_sent_, std::ios::beg);
        if (!file) {
            std::cerr << "Error: Failed to seek to resume position: " << bytes_sent_ << std::endl;
            upload_state_ = UploadState::ERROR;
            file.close();
            return;
        }
        std::cout << "Resuming upload from offset " << bytes_sent_ << "..." << std::endl;
    }

    // Send file data in chunks
    const size_t CHUNK_SIZE = 32 * 1024; // 32KB chunks
    std::vector<char> buffer(CHUNK_SIZE);

    while (bytes_sent_ < file_size_ && upload_state_ == UploadState::UPLOADING) {
        size_t to_read = std::min(CHUNK_SIZE, static_cast<size_t>(file_size_ - bytes_sent_));

        file.read(buffer.data(), to_read);
        std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0) {
            std::cerr << "Error: Failed to read file data" << std::endl;
            upload_state_ = UploadState::ERROR;
            break;
        }

        // Send data chunk
        if (!sendDataChunk(buffer.data(), bytes_read)) {
            std::cerr << "Error: Failed to send data chunk" << std::endl;
            upload_state_ = UploadState::ERROR;
            break;
        }

        bytes_sent_ += bytes_read;

        // Show progress
        double progress = (double)bytes_sent_ / file_size_ * 100.0;
        std::cout << "\r📤 Progress: " << std::fixed << std::setprecision(1) << progress << "% (" 
                  << bytes_sent_ << "/" << file_size_ << " bytes)" << std::flush;
    }

    file.close();

    if (upload_state_ == UploadState::UPLOADING && bytes_sent_ == file_size_) {
        std::cout << "\n📤 File data sent, waiting for server confirmation..." << std::endl;
        // 主动等待一个完成或错误响应，避免用户立即输入下一个命令把响应混在其他 handler 流程里
        for (int i = 0; i < 100 && upload_state_ == UploadState::UPLOADING; ++i) { // 最多轮询100次（可调）
            Handler::receive();
            if (response_.contains("status")) {
                std::string st = response_["status"].get<std::string>();
                if (st == "completed") {
                    upload_state_ = UploadState::COMPLETED;
                    std::cout << "✓ Upload confirmed by server." << std::endl;
                    break;
                } else if (st == "receiving") {
                    // 仍在处理中，短暂等待
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
}

bool PUTHandler::sendDataChunk(const char* data, size_t size) {
    Message msg;
    msg.header.type = static_cast<uint8_t>(MessageType::PUT_DATA);
    msg.header.length = size;
    
    if (size > sizeof(msg.body)) {
        std::cerr << "Error: Data chunk too large: " << size << std::endl;
        return false;
    }
    
    memcpy(msg.body, data, size);
    
    ssize_t sent = ::send(fd_, reinterpret_cast<const char*>(&msg), sizeof(msg.header) + size, 0);
    if (sent < 0) {
        error_cpp20("Failed to send data chunk: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

