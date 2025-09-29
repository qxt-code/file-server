#pragma once

#include <string>
#include <vector>
#include <chrono>

struct User {
    int id{0};
    std::string username;              // unique username
    std::string password_hash;         // server-stored hash = H(client_hash + salt)
    std::string salt;                  // per-user random salt (hex)
    std::string email;                 // optional email
    std::chrono::system_clock::time_point createdTime{};  // TODO: convert from DB string
    std::chrono::system_clock::time_point modifiedTime{}; // TODO: convert from DB string

    User() = default;
    User(int user_id,
         const std::string& user_name,
         const std::string& password,
         const std::string& user_email,
         const std::string& user_salt)
        : id(user_id), username(user_name), password_hash(password), salt(user_salt), email(user_email) {}
};
