#pragma once

#include "request_handler.h"

namespace handlers {

class RMHandler : public RequestHandler {
public:
    RMHandler() = default;
    ~RMHandler() override = default;

    void handle() override;
};

} // namespace handlers
