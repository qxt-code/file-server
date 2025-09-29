#include "user_open_table.h"
#include <unordered_map>
#include <mutex>
#include <memory>
#include <ranges>

#include "common/debug.h"

namespace storage {


UserOpenTable::UserOpenTable() {
    global_open_table_ = &GlobalOpenTable::getInstance();
}

UserOpenTable::~UserOpenTable() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& handle : open_files_) {
        if (handle)
            global_open_table_->closeFile(handle->getFileName());
    }
    open_files_.clear();
}

bool UserOpenTable::isFileOpen(const std::string& file_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(open_files_.begin(), open_files_.end(),
                       [&file_name](const UserFileHandle::Ptr& handle) {
                           return handle && handle->getFileName() == file_name;
                       });
}

std::optional<int> UserOpenTable::openFile(const std::string& file_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto fd_opt = global_open_table_->openFile(file_name);
    if (!fd_opt.has_value()) {
        error_cpp20("Failed to open file " + file_name + " in GlobalOpenTable");
        return std::nullopt;
    }
    int fd = fd_opt.value();
    open_files_.emplace_back(std::make_shared<UserFileHandle>(fd, file_name));
    return open_files_.size() - 1; // Return index as file descriptor
}

int UserOpenTable::getFD(const std::string& file_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::ranges::find_if(open_files_, [&file_name](const UserFileHandle::Ptr& handle) {
        return handle && handle->getFileName() == file_name;
    });
    if (it == open_files_.end()) {
        return -1; // Not found
    }
    return std::ranges::distance(open_files_.begin(), it);
}

UserFileHandle::Ptr UserOpenTable::getFileHandle(int user_fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (user_fd < 0 || static_cast<size_t>(user_fd) >= open_files_.size()) {
        RUNTIME_ERROR("Invalid user file descriptor: %d", user_fd);
        return nullptr;
    }
    return open_files_[user_fd];
}

void UserOpenTable::closeFile(int user_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto handle = open_files_[user_fd];
    if (!handle) {
        RUNTIME_ERROR("Attempt to close an already closed file descriptor: %d", user_fd);
        return;
    }
    std::string file_name = handle->getFileName();
    open_files_[user_fd] = nullptr; // Mark as closed
    global_open_table_->closeFile(file_name);
}




} // namespace storage