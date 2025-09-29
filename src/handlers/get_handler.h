#pragma once

#include "request_handler.h"
#include "storage/file_manager.h"
#include <memory>

namespace handlers {

class GETHandler : public RequestHandler {
public:
    GETHandler() = default;
    ~GETHandler() override = default;

    void handle() override;
private:
    void streamFile(const std::string& file_name, uint64_t start_offset);
};

} // namespace handlers
