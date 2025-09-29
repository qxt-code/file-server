#pragma once

#include <string>
#include <memory>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

class RSAKeyManager {
public:
    static RSAKeyManager& getInstance();

    bool generateKeyPair(int bits = 2048);
    std::string getPublicKeyPEM() const;
    bool loadPublicKeyPEM(const std::string& pem);
    bool hasPublicKey() const { return rsa_ != nullptr; }
   
    std::string getPrivateKeyPEM() const;

  
    std::string encrypt(const std::string& plaintext) const;
  
    std::string decrypt(const std::string& b64cipher) const;

private:
    RSAKeyManager() = default;
    ~RSAKeyManager();
    RSA* rsa_{nullptr};
};
