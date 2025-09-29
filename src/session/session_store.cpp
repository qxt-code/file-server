#include "session_store.h"
#include <algorithm>

SessionStore& SessionStore::instance() {
    static SessionStore store;
    return store;
}

SessionContext::Ptr SessionStore::create_session(int user_id) {
    std::unique_lock lock(mutex_);
    // If user already has a session, reuse it
    if (auto it = by_user_.find(user_id); it != by_user_.end()) {
        auto existing_sid = it->second;
        auto existing = by_session_[existing_sid];
        return existing; // reuse existing
    }
    uint64_t sid = next_session_id_++;
    auto ctx = std::make_shared<SessionContext>(user_id);
    ctx->session_id = sid;
    ctx->last_active_time = ::time(nullptr);
    by_session_[sid] = ctx;
    by_user_[user_id] = sid;
    log_cpp20("[SessionStore] create session user=" + std::to_string(user_id) + " sid=" + std::to_string(sid));
    return ctx;
}

SessionContext::Ptr SessionStore::get_by_session(uint64_t sid) {
    std::shared_lock lock(mutex_);
    auto it = by_session_.find(sid);
    return it == by_session_.end() ? nullptr : it->second;
}

SessionContext::Ptr SessionStore::get_by_user(int user_id) {
    std::shared_lock lock(mutex_);
    auto it = by_user_.find(user_id);
    if (it == by_user_.end()) return nullptr;
    auto sit = by_session_.find(it->second);
    return sit == by_session_.end() ? nullptr : sit->second;
}

void SessionStore::attach_connection(uint64_t sid, int fd) {
    std::unique_lock lock(mutex_);
    auto it = by_session_.find(sid);
    if (it == by_session_.end()) return;
    auto& vec = it->second->connection_fds;
    if (std::find(vec.begin(), vec.end(), fd) == vec.end()) {
        vec.push_back(fd);
    }
    by_fd_[fd] = sid;
    it->second->last_active_time = ::time(nullptr);
    log_cpp20("[SessionStore] attach fd=" + std::to_string(fd) + " sid=" + std::to_string(sid));
}

void SessionStore::detach_connection(int fd) {
    std::unique_lock lock(mutex_);
    auto fit = by_fd_.find(fd);
    if (fit == by_fd_.end()) return;
    auto sid = fit->second;
    by_fd_.erase(fit);
    auto sit = by_session_.find(sid);
    if (sit != by_session_.end()) {
        auto& vec = sit->second->connection_fds;
        vec.erase(std::remove(vec.begin(), vec.end(), fd), vec.end());
        log_cpp20("[SessionStore] detach fd=" + std::to_string(fd) + " sid=" + std::to_string(sid));
    }
}

void SessionStore::remove_session(uint64_t sid) {
    std::unique_lock lock(mutex_);
    auto it = by_session_.find(sid);
    if (it == by_session_.end()) return;
    int uid = it->second->user_id;
    for (int fd : it->second->connection_fds) {
        by_fd_.erase(fd);
    }
    by_session_.erase(it);
    by_user_.erase(uid);
    log_cpp20("[SessionStore] remove session sid=" + std::to_string(sid) + " user=" + std::to_string(uid));
}
