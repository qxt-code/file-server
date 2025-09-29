#pragma once
#include "handler.h"
#include "auth/rsa_key_manager.h"

class GetPubKeyHandler : public Handler {
public:
    GetPubKeyHandler(int fd, json&& cmd): Handler(fd, std::move(cmd)) {}
    void handle() override {
        request_ = { {"command", "pubkey"} };
        send(MessageType::REQUEST, request_);
    }
    void receive() override {
        Handler::receive();
        // Server currently uses responseBuilder.build(data.dump()) -> field name is responseMessage containing JSON string
        if (response_.contains("responseMessage")) {
            std::string inner = response_["responseMessage"].get<std::string>();
            try {
                auto j = json::parse(inner);
                if (j.contains("pubkey")) {
                    std::string pem = j["pubkey"].get<std::string>();
                    RSAKeyManager::getInstance().loadPublicKeyPEM(pem);
                }
            } catch (...) {
                // ignore parse errors
            }
        }
    }
};
