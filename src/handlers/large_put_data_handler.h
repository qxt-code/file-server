#pragma once

#include "request_handler.h"
#include "types/pending_large_upload.h"
#include "storage/file_manager.h"
#include <optional>

namespace handlers {

// Handler for the second (data) connection of a large PUT upload.
// First JSON frame: {"command":"put_data_channel","token":"...","file_hash":"..."}
// After validation it will prepare for splice-based reception (implemented in next step).
class LargePutDataHandler : public RequestHandler {
public:
    LargePutDataHandler() = default;
    ~LargePutDataHandler() override = default;

    void handle() override;              // process initial JSON command
    void recvRequest() override;         // after INIT will switch to raw splice loop (future)

private:
    enum class State { INIT, READY, RECEIVING, COMPLETED, ERROR } state_{State::INIT};
    PendingLargeUpload plu_{};           // consumed token metadata
    int file_fd_{-1};
    bool file_opened_{false};
    int pipe_in_{-1};
    int pipe_out_{-1};
    uint64_t received_{0};
    bool hash_verified_{false};

    void tryStartReceiving();
    void receiveLoop();
    bool computeAndVerifyHash();
    void finalize(bool success, const std::string& err="");
    void closeResources();

    bool consumeToken();
    bool prepareFile();
    void sendReady();
};

} // namespace handlers
