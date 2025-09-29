#pragma once

#include <string>
#include <optional>

#include "types/user.h"
#include "db/user_repository.h"
#include "password_hash.h"

// Simple AuthManager bridging to UserRepository. Registration expects client already hashed password
// (client_hash). We compute stored hash = H(client_hash + salt) and persist.
class AuthManager {
public:
    AuthManager() = default;
    ~AuthManager() = default;

    bool registerUser(const std::string& username, const std::string& client_hash, std::string* error_message = nullptr);
    std::optional<User> loginUser(const std::string& username, const std::string& client_hash, std::string* error_message = nullptr);
};