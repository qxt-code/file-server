#include "pubkey_handler.h"
#include "common/debug.h"

namespace handlers {
void PubKeyHandler::handle() {
    auto& rsa = RSAKeyManager::getInstance();
    std::string pub = rsa.getPublicKeyPEM();
    if (pub.empty()) {
        jsonResponse = responseBuilder.buildErrorResponse(500, "no pubkey");
        sendResponse(MessageType::ERROR);
    } else {
        nlohmann::json data = { {"pubkey", pub} };
        jsonResponse = responseBuilder.build(data.dump());
        sendResponse(MessageType::RESPONSE);
    }
    connection_context_->change_handler_callback(RequestHandler::Ptr(new RequestHandler(connection_context_)));
}
}
