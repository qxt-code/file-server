#include "user_file_handle.h"

#include "common/debug.h"

namespace storage {

int UserFileHandle::writeBuffer(const void* buffer, size_t size) {
    int n = write(fd_, buffer, size);
    if (n < 0) {
        error_cpp20("Failed to write to file " + file_name_ + ": " + std::string(strerror(errno)));
        return -1;
    }
    advancePosition(size);
    return n;
}

int UserFileHandle::readBuffer(void* buffer, size_t size) {
    int n = read(fd_, buffer, size);
    if (n < 0) {
        error_cpp20("Failed to read from file " + file_name_ + ": " + std::string(strerror(errno)));
        return -1;
    }
    advancePosition(size);
    return n;
}


} // namespace storage