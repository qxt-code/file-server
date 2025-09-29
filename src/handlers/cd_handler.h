#pragma once

#include <string>
#include <vector>

#include "request_handler.h"

namespace handlers {

class CDHandler : public RequestHandler {
public:
    CDHandler() = default;
    ~CDHandler() override = default;
    
    void handle() override;

private:
    
};

} // namespace handlers