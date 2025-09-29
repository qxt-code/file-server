// Implementation aligned with new interface described in header.
#include "password_hash.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <stdexcept>

namespace {
std::string bytesToHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return oss.str();
}

std::string sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);
    return bytesToHex(hash, SHA256_DIGEST_LENGTH);
}
}

std::string PasswordHash::generateSalt(size_t size) {
    std::vector<unsigned char> salt(size);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Failed to generate salt");
    }
    return bytesToHex(salt.data(), salt.size());
}

std::string PasswordHash::clientHash(const std::string& password) {
    return sha256(password);
}

std::string PasswordHash::serverHash(const std::string& client_hash, const std::string& salt) {
    return sha256(client_hash + salt);
}

bool PasswordHash::verify(const std::string& client_hash, const std::string& salt, const std::string& stored_hash) {
    return serverHash(client_hash, salt) == stored_hash;
}