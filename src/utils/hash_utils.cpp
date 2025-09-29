#include "hash_utils.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <openssl/sha.h>

#include "common/debug.h"

namespace utils {

std::string HashUtils::calculateFileSHA1(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        error_cpp20("Failed to open file for hash calculation: " + file_path);
        return "";
    }
    
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";
        const EVP_MD* md = EVP_sha1();
        EVP_DigestInit_ex(ctx, md, nullptr);
    
    const size_t BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
            EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        EVP_DigestFinal_ex(ctx, hash, &hash_len);
        EVP_MD_CTX_free(ctx);
    
        return hashToHexString(hash, hash_len);
}
    // 增量哈希实现
    IncrementalSHA1::IncrementalSHA1() {
        ctx_ = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx_, EVP_sha1(), nullptr);
    }
    
    IncrementalSHA1::~IncrementalSHA1() {
        if (ctx_) EVP_MD_CTX_free(ctx_);
    }
    
    void IncrementalSHA1::update(const void* data, size_t len) {
        if (ctx_) EVP_DigestUpdate(ctx_, data, len);
    }
    
    std::string IncrementalSHA1::final() {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        EVP_DigestFinal_ex(ctx_, hash, &hash_len);
        std::stringstream ss;
        for (unsigned int i = 0; i < hash_len; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

std::string HashUtils::calculateSHA1(const void* data, size_t size) {
    if (!data || size == 0) {
        error_cpp20("Invalid data or size for hash calculation");
        return "";
    }
    
    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);
    SHA1_Update(&sha_ctx, data, size);
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha_ctx);
    
    return hashToHexString(hash, SHA_DIGEST_LENGTH);
}

bool HashUtils::verifyFileSHA1(const std::string& file_path, const std::string& expected_hash) {
    if (expected_hash.empty()) {
        error_cpp20("Expected hash is empty");
        return false;
    }
    
    std::string actual_hash = calculateFileSHA1(file_path);
    if (actual_hash.empty()) {
        error_cpp20("Failed to calculate file hash for verification");
        return false;
    }
    
    // Convert to lowercase for case-insensitive comparison
    std::string expected_lower = expected_hash;
    std::string actual_lower = actual_hash;
    
    std::transform(expected_lower.begin(), expected_lower.end(), expected_lower.begin(), ::tolower);
    std::transform(actual_lower.begin(), actual_lower.end(), actual_lower.begin(), ::tolower);
    
    bool match = (expected_lower == actual_lower);
    if (!match) {
        error_cpp20("Hash verification failed. Expected: " + expected_hash + ", Actual: " + actual_hash);
    }
    
    return match;
}

std::string HashUtils::hashToHexString(const unsigned char* hash, size_t length) {
    std::stringstream ss;
    for (size_t i = 0; i < length; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

} // namespace utils