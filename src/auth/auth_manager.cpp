#include "auth_manager.h"

bool AuthManager::registerUser(const std::string& username, const std::string& client_hash, std::string* error_message) {
    auto& repo = db::UserRepository::getInstance();
    if (repo.usernameExists(username)) {
        if (error_message) *error_message = "username exists";
        return false;
    }
    std::string salt = PasswordHash::generateSalt();
    std::string stored_hash = PasswordHash::serverHash(client_hash, salt);
    User u;
    u.username = username;
    u.password_hash = stored_hash;
    u.salt = salt;
    u.email = "";
    if (!repo.createUser(u)) {
        if (error_message) *error_message = "create user failed";
        return false;
    }
    return true;
}

std::optional<User> AuthManager::loginUser(const std::string& username, const std::string& client_hash, std::string* error_message) {
    auto& repo = db::UserRepository::getInstance();
    auto uopt = repo.getUserByName(username);
    if (!uopt) {
        if (error_message) *error_message = "user not found";
        return std::nullopt;
    }
    if (!PasswordHash::verify(client_hash, uopt->salt, uopt->password_hash)) {
        if (error_message) *error_message = "invalid password";
        return std::nullopt;
    }
    return uopt;
}