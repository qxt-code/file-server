#pragma once
#include "request_handler.h"
#include "db/user_repository.h"
#include "auth/password_hash.h"
#include "auth/rsa_key_manager.h"

namespace handlers {

class RegisterHandler : public RequestHandler {
public:
    void handle() override;
};

} // namespace handlers
