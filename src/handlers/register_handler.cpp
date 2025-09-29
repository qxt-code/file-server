#include "register_handler.h"
#include "protocol/response_builder.h"
#include "common/debug.h"

namespace handlers {

void RegisterHandler::handle() {
    // Expected jsonRequest params: { username_enc, passhash_enc }
    auto params = jsonRequest.value("params", nlohmann::json::object());
    std::string username_enc = params.value("username", "");
    std::string passhash_enc = params.value("passhash", "");
    if (username_enc.empty() || passhash_enc.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "missing username or passhash");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }

    auto& userRepo = db::UserRepository::getInstance();
    auto& rsa = RSAKeyManager::getInstance();

    std::string username = rsa.decrypt(username_enc);
    std::string client_hash = rsa.decrypt(passhash_enc);
    if (username.empty() || client_hash.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "decrypt failed");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }

    if (userRepo.usernameExists(username)) {
        jsonResponse = responseBuilder.buildErrorResponse(409, "username exists");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }

    std::string salt = PasswordHash::generateSalt();
    std::string stored_hash = PasswordHash::serverHash(client_hash, salt);

    User newUser; // default id=0
    newUser.username = username;
    newUser.password_hash = stored_hash;
    newUser.salt = salt;

    newUser.email = username + "@local"; // simply append domain for now

    log_cpp20("[RegisterHandler] attempting create user username=" + username +
              " salt_len=" + std::to_string(salt.size()) +
              " stored_hash_len=" + std::to_string(stored_hash.size()) +
              " email=" + newUser.email);

    if (!userRepo.createUser(newUser)) {
        jsonResponse = responseBuilder.buildErrorResponse(500, "create user failed");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }

    jsonResponse = responseBuilder.build("register success");
    sendResponse(MessageType::RESPONSE);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}

} // namespace handlers
