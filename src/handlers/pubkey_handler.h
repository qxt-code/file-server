#pragma once
#include "request_handler.h"
#include "auth/rsa_key_manager.h"

namespace handlers {
class PubKeyHandler : public RequestHandler {
public:
    void handle() override;
};
}
