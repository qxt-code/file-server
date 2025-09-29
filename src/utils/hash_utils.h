#pragma once

#include <string>
#include <openssl/evp.h>

namespace utils {

class HashUtils {
public:
    static std::string calculateFileSHA1(const std::string& file_path);
    static std::string calculateSHA1(const void* data, size_t size);
    static bool verifyFileSHA1(const std::string& file_path, const std::string& expected_hash);
private:
    static std::string hashToHexString(const unsigned char* hash, size_t length);
};

// 增量哈希类
class IncrementalSHA1 {
public:
    IncrementalSHA1();
    ~IncrementalSHA1();
    void update(const void* data, size_t len);
    std::string final();
private:
    EVP_MD_CTX* ctx_ = nullptr;
};

} // namespace utils