#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include "types/user.h"
#include "global_open_table.h"
#include "user_file_handle.h"
#include "types/context.h"

namespace storage {

class UserOpenTable {
public:
    UserOpenTable();
    ~UserOpenTable();

    UserOpenTable(const UserOpenTable&) = delete;
    UserOpenTable& operator=(const UserOpenTable&) = delete;

    UserOpenTable(UserOpenTable&&) = default;
    UserOpenTable& operator=(UserOpenTable&&) = default;

    /// @brief Open a file for the user.
    /// @param file_name In the storage context, this is the file hash.
    /// @return 
    bool isFileOpen(const std::string& file_name) const;
    std::optional<int> openFile(const std::string& file_name);
    int getFD(const std::string& file_name) const;
    UserFileHandle::Ptr getFileHandle(int user_fd) const;
    void closeFile(int user_fd);

private:

    GlobalOpenTable* global_open_table_;
    std::vector<UserFileHandle::Ptr> open_files_;
    mutable std::mutex mutex_;

    ConnectionContext::Ptr context_{nullptr};
};

} // namespace storage