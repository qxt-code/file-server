#pragma once

#include <string>
#include <vector>

#include "request_handler.h"

namespace handlers {

class PWDHandler : public RequestHandler {
public:
    PWDHandler() = default;
    ~PWDHandler() override = default;
    
    void handle() override;

private:
    
};

} // namespace handlers