#pragma once

#include <stdint.h>
#include <memory>
#include <string>

namespace storage {

class UserFileHandle {
public:
    using Ptr = std::shared_ptr<UserFileHandle>;

    UserFileHandle(int fd, std::string file_name) : fd_(fd), file_name_(file_name), position_(0) {}
    ~UserFileHandle() = default;

    int getFD() const { return fd_; }
    std::string getFileName() const { return file_name_; }
    
    uint64_t getPosition() const { return position_; }
    void setPosition(uint64_t pos) {
        lseek(fd_, pos, SEEK_SET);
        position_ = pos;
    }
    void advancePosition(uint64_t offset) { position_ += offset; }

    int writeBuffer(const void* buffer, size_t size);
    int readBuffer(void* buffer, size_t size);

    ssize_t spliceFromSocket(int socket_fd, size_t size);

private:
    int fd_;
    std::string file_name_;
    uint64_t position_;
};

} // namespace storage