#pragma once

#include <string>
#include <vector>

#include "request_handler.h"

namespace handlers {

class MKDIRHandler : public RequestHandler {
public:
    MKDIRHandler() = default;
    ~MKDIRHandler() override = default;
    
    void handle() override;

private:
    
};

} // namespace handlers