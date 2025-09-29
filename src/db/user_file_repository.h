#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "mysql_pool.h"
#include "types/user_file.h"

namespace db {

using db::MySQLPool;

class UserFileRepository {
public:
    static void init(MySQLPool& pool) {
        if (is_initialized_) return;
        pool_ = &pool;
        is_initialized_ = true;
    }

    static UserFileRepository& getInstance() {
        static UserFileRepository instance{};
        return instance;
    }

    UserFileRepository(const UserFileRepository&) = delete;
    UserFileRepository& operator=(const UserFileRepository&) = delete;

    bool insertUserFile(const UserFile& userFile);

    bool deleteUserFile(const size_t fileId);
    std::vector<UserFile> listFiles(const std::string& userId);
    std::vector<UserFile> getFilesByParentID(int userId, int parentId);
    UserFile getFile(const std::string& fileId);
    // New: fetch single file in a directory by name (virtual path resolution)
    std::optional<UserFile> getFileByParentAndName(int userId, int parentId, const std::string& name);
    // New: fetch by full virtual path for a user (simple implementation scanning components)
    std::optional<UserFile> getFileByPath(int userId, const std::string& virtualPath);

private:
    UserFileRepository();
    ~UserFileRepository();

    inline static MySQLPool* pool_{nullptr};
    inline static bool is_initialized_{false};
};

} // namespace db