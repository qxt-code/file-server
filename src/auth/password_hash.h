#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include <string>

// Utility for password hashing.
// Client side: password -> client_hash (SHA256 or similar).
// Server side: stored_hash = H(client_hash + salt), salt randomly generated per user.
// verify: recompute H(client_hash + salt) and compare with stored.
class PasswordHash {
public:
    static std::string generateSalt(size_t size = 16);
    static std::string clientHash(const std::string& password);
    static std::string serverHash(const std::string& client_hash, const std::string& salt);
    static bool verify(const std::string& client_hash,
                       const std::string& salt,
                       const std::string& stored_hash);
};

#endif // PASSWORD_HASH_H