#pragma once

#include <string>
#include <vector>
#include <optional>

#include "mysql_pool.h"
#include "types/user.h"
#include "common/debug.h"

namespace db {

class UserRepository {
public:
    static void init(MySQLPool& pool) {
        if (is_initialized_) return;
        pool_ = &pool;
        is_initialized_ = true;
    }

    static UserRepository& getInstance() {
        static UserRepository instance{};
        return instance;
    }

    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    // createUser expects user.password_hash already server processed (client_hash+salt) and user.salt set
    bool createUser(const User& user);
    bool deleteUser(int userId);
    bool updateUser(const User& user);              // Updates username/password/email by id
    std::optional<User> getUserById(int userId);
    std::optional<User> getUserByName(const std::string& username);
    std::vector<User> getAllUsers();
    bool usernameExists(const std::string& username);

private:
    UserRepository();
    ~UserRepository() = default;

    static inline MySQLPool* pool_{nullptr};
    static inline bool is_initialized_{false};
};

} // namespace db
