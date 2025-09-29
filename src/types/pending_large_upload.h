#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <optional>

struct PendingLargeUpload {
    std::string token;          // one-time upload token
    int user_id{-1};
    std::string file_name;      // virtual name
    std::string file_hash;      // expected hash
    uint64_t file_size{0};
    uint64_t received{0};
    std::chrono::steady_clock::time_point expire_at; // expiry time
};

class LargeUploadRegistry {
public:
    static LargeUploadRegistry& instance() { static LargeUploadRegistry inst; return inst; }

    std::string create(int user_id, const std::string& file_name, const std::string& file_hash, uint64_t file_size, int ttl_seconds = 300);
    std::optional<PendingLargeUpload> get(const std::string& token);
    bool consume(const std::string& token, PendingLargeUpload& out);
    void update_received(const std::string& token, uint64_t bytes);
    void cleanup();
private:
    LargeUploadRegistry() = default;
    std::mutex mtx_;
    std::unordered_map<std::string, PendingLargeUpload> map_;
};
