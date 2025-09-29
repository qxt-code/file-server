#include "global_open_table.h"
#include <unordered_map>
#include <mutex>
#include <string>
#include <unistd.h>
#include <fcntl.h>

#include "common/debug.h"

namespace storage {

void GlobalOpenTable::init(const std::string& storage_root) {
    if (is_initialized_) return;
    storage_root_ = storage_root;
    is_initialized_ = true;
}

GlobalOpenTable::GlobalOpenTable() {
    if (!is_initialized_) {
        RUNTIME_ERROR("GlobalOpenTable not initialized. Call GlobalOpenTable::init() first.");
        return;
    }
    if (fs::exists(storage_root_)) {
        if (!fs::is_directory(storage_root_)) {
            RUNTIME_ERROR("Storage root is not a directory: %s", storage_root_.c_str());
            return;
        }
    } else {
        fs::create_directories(storage_root_);
    }
}

GlobalOpenTable::~GlobalOpenTable() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [path, entry] : open_files_) {
        if (entry.fd >= 0) {
            close(entry.fd);
        }
    }
    open_files_.clear();
}

std::optional<int> GlobalOpenTable::openFile(const std::string& file_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = open_files_.find(file_name);
    if (it != open_files_.end()) {
        it->second.refCount++;
        return it->second.fd;
    }

    int fd = ::open((storage_root_ / file_name).c_str(), O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd < 0) {
        RUNTIME_ERROR("Failed to open file %s: %s", file_name.c_str(), strerror(errno));
        return std::nullopt;
    }

    open_files_[file_name] = {1, fd};
    return fd;
}
void GlobalOpenTable::closeFile(const std::string& file_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = open_files_.find(file_name);
    if (it == open_files_.end()) {
        RUNTIME_ERROR("Attempt to close a file not in GlobalOpenTable: %s", file_name.c_str());
        return;
    }

    it->second.refCount--;
    if (it->second.refCount <= 0) {
        if (it->second.fd >= 0) {
            close(it->second.fd);
        }
        open_files_.erase(it);
    }
}

bool GlobalOpenTable::isFileOpen(const std::string& file_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_files_.find(file_name) != open_files_.end();
}

} // namespace storage