#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>

#include "types/context.h"
#include "common/debug.h"

// store mapping session_id and user_id.
class SessionStore {
public:
    static SessionStore& instance();

    SessionContext::Ptr create_session(int user_id);
    SessionContext::Ptr get_by_session(uint64_t sid);
    SessionContext::Ptr get_by_user(int user_id);
    void attach_connection(uint64_t sid, int fd);
    void detach_connection(int fd);
    void remove_session(uint64_t sid);

private:
    SessionStore() = default;
    std::shared_mutex mutex_;
    std::unordered_map<uint64_t, SessionContext::Ptr> by_session_;
    std::unordered_map<int, uint64_t> by_user_; // user_id -> session_id
    std::unordered_map<int, uint64_t> by_fd_;   // connection fd -> session_id
    uint64_t next_session_id_{1};
};
