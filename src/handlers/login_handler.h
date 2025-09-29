#pragma once
#include "request_handler.h"
#include "auth/password_hash.h"
#include "auth/rsa_key_manager.h"
#include "auth/auth_manager.h"
#include "session/session_store.h"

namespace handlers {
class LoginHandler : public RequestHandler {
public:
    void handle() override;
};
}
