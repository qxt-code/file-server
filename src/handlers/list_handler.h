#pragma once

#include <string>
#include <vector>

#include "request_handler.h"

namespace handlers {

class ListHandler : public RequestHandler {
public:
    ListHandler() = default;
    ~ListHandler() override = default;
    
    void handle() override;

private:
    
    std::vector<std::string> getUserFiles(const std::string& userId);
};

} // namespace handlers