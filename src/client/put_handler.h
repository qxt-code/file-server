#pragma once

#include "handler.h"
#include <string>

class PUTHandler : public Handler
{
public:
    PUTHandler(int fd, json&& command) : Handler(fd, std::move(command)) {}
    ~PUTHandler() override = default;
    
    void handle() override;
    void receive() override;

private:
    enum class UploadState {
        INIT,
        UPLOADING, 
        COMPLETED,
        ERROR
    };
    
    void startFileUpload();
    bool sendDataChunk(const char* data, size_t size);
    
    std::string file_path_;
    size_t file_size_{0};
    size_t bytes_sent_{0};
    UploadState upload_state_{UploadState::INIT};
};