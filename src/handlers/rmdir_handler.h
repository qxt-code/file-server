#pragma once

#include "request_handler.h"

namespace handlers {

class RMDIRHandler : public RequestHandler {
public:
    RMDIRHandler() = default;
    ~RMDIRHandler() override = default;
    void handle() override;
};

} // namespace handlers
