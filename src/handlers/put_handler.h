#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

#include "request_handler.h"
#include "storage/file_manager.h"
#include "utils/hash_utils.h"

namespace handlers {

class PUTHandler : public RequestHandler {
public:
    PUTHandler() = default;
    ~PUTHandler() override = default;
    
    void recvRequest() override;
    void handle() override;

private:
    enum class PUT_STATE {
        INIT,
        RECEIVING,
        COMPLETED,
        ERROR
    };

    void prepareToReceive();
    void processMessages();
    ssize_t receiveData(int fd, size_t size);
    bool receiveCompleteMessage();
    void rollbackToBaseHandler();

    PUT_STATE state_{PUT_STATE::INIT};
    storage::UserFileHandle::Ptr file_handle_{nullptr};
    Message temp_msg_{};
    std::mutex mutex_;
    std::queue<Message> msg_queue_;
    ssize_t total_received_{0};

    std::unique_ptr<utils::IncrementalSHA1> incremental_sha1_;
};

} // namespace handlers