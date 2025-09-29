#include "login_handler.h"
#include "protocol/response_builder.h"
#include "common/debug.h"
#include <jwt-cpp/jwt.h>
#include "storage/file_manager.h"

namespace handlers {
void LoginHandler::handle() {
    auto params = jsonRequest.value("params", nlohmann::json::object());
    std::string username_enc = params.value("username", "");
    std::string passhash_enc = params.value("passhash", "");
    if (username_enc.empty() || passhash_enc.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "missing username or passhash");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    auto& rsa = RSAKeyManager::getInstance();
    std::string username = rsa.decrypt(username_enc);
    std::string client_hash = rsa.decrypt(passhash_enc);
    if (username.empty() || client_hash.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(400, "decrypt failed");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    std::string err;
    auto user_opt = AuthManager().loginUser(username, client_hash, &err);
    if (!user_opt) {
        jsonResponse = responseBuilder.buildErrorResponse(401, err.empty()?"login failed":err);
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    auto session_ctx = SessionStore::instance().create_session(user_opt->id);
    SessionStore::instance().attach_connection(session_ctx->session_id, connection_context_->connection_id);
    // Generate JWT token (RS256) with user_id and session_id using existing RSA private key
    std::string priv_pem = rsa.getPrivateKeyPEM();
    if (priv_pem.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(500, "private key unavailable");
        sendResponse(MessageType::ERROR);
        connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
        return;
    }
    
    auto token_obj = jwt::create()
        .set_type("JWT")
        .set_issuer("file-server")
        .set_subject(std::to_string(user_opt->id))
        .set_audience("file-server-clients")
        .set_payload_claim("sid", jwt::claim(std::to_string(session_ctx->session_id)))
        .set_payload_claim("uid", jwt::claim(std::to_string(user_opt->id)))
        .set_payload_claim("uname", jwt::claim(user_opt->username))
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(12))
        .sign(jwt::algorithm::rs256(rsa.getPublicKeyPEM(), priv_pem, "", ""));
    std::string token = token_obj;
    session_ctx->session_token = token;
    nlohmann::json data = {
        {"token", token},
        {"user_id", user_opt->id},
        {"session_id", session_ctx->session_id},
        {"username", user_opt->username}
    };
    jsonResponse = responseBuilder.build(data.dump());
    sendResponse(MessageType::RESPONSE);
    connection_context_->session_context = session_ctx;
    storage::FileManager::getInstance().setUser(session_ctx->user_id);
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}
}
