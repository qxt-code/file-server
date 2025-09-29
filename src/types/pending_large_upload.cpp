#include "pending_large_upload.h"
#include <random>

static std::string random_token_hex(size_t bytes = 16) {
    std::random_device rd; std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::string out;
    out.reserve(bytes*2);
    while (bytes > 0) {
        auto v = dis(gen);
        size_t take = std::min<size_t>(bytes, sizeof(v));
        for (size_t i = 0; i < take; i++) {
            unsigned b = (v >> (i*8)) & 0xFF;
            static const char* hex="0123456789abcdef";
            out.push_back(hex[b>>4]);
            out.push_back(hex[b&0xF]);
        }
        bytes -= take;
    }
    return out;
}

std::string LargeUploadRegistry::create(
    int user_id, const std::string& file_name, 
    const std::string& file_hash, uint64_t file_size, int ttl_seconds) {
    PendingLargeUpload plu; 
    plu.user_id = user_id; 
    plu.file_name = file_name; 
    plu.file_hash = file_hash; 
    plu.file_size = file_size;
    plu.token = random_token_hex(); 
    plu.expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    std::lock_guard<std::mutex> lk(mtx_);
    map_[plu.token] = plu; 
    return plu.token;
}

std::optional<PendingLargeUpload> LargeUploadRegistry::get(const std::string& token) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = map_.find(token);
    if (it == map_.end())
        return std::nullopt;
    return it->second;
}

bool LargeUploadRegistry::consume(const std::string& token, PendingLargeUpload& out) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = map_.find(token);
    if (it == map_.end())
        return false;
    out = it->second;
    map_.erase(it);
    return true; // remove to enforce one-time use
}

void LargeUploadRegistry::update_received(const std::string& token, uint64_t bytes) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = map_.find(token);
    if (it == map_.end())
        return;
    it->second.received += bytes;
}

void LargeUploadRegistry::cleanup() {
    std::lock_guard<std::mutex> lk(mtx_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = map_.begin(); it != map_.end();) {
        if (it->second.expire_at < now)
            it = map_.erase(it);
        else
            ++it;
    }
}
