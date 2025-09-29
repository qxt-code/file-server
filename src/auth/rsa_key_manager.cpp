#include "rsa_key_manager.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <vector>
#include <stdexcept>

RSAKeyManager& RSAKeyManager::getInstance() {
    static RSAKeyManager instance;
    return instance;
}

RSAKeyManager::~RSAKeyManager() {
    if (rsa_) {
        RSA_free(rsa_);
        rsa_ = nullptr;
    }
}

bool RSAKeyManager::generateKeyPair(int bits) {
    BIGNUM* bn = BN_new();
    if (!bn) return false;
    if (BN_set_word(bn, RSA_F4) != 1) {
        BN_free(bn);
        return false;
    }
    RSA* rsa = RSA_new();
    if (RSA_generate_key_ex(rsa, bits, bn, nullptr) != 1) {
        RSA_free(rsa);
        BN_free(bn);
        return false;
    }
    BN_free(bn);
    if (rsa_) RSA_free(rsa_);
    rsa_ = rsa;
    return true;
}

std::string RSAKeyManager::getPublicKeyPEM() const {
    if (!rsa_) return {};
    BIO* bio = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_RSA_PUBKEY(bio, rsa_)) {
        BIO_free(bio);
        return {};
    }
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string pem(data, len);
    BIO_free(bio);
    return pem;
}

std::string RSAKeyManager::getPrivateKeyPEM() const {
    if (!rsa_) return {};
    BIO* bio = BIO_new(BIO_s_mem());

    if (!PEM_write_bio_RSAPrivateKey(bio, rsa_, nullptr, nullptr, 0, nullptr, nullptr)) {
        BIO_free(bio);
        return {};
    }
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string pem(data, len);
    BIO_free(bio);
    return pem;
}

bool RSAKeyManager::loadPublicKeyPEM(const std::string& pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), pem.size());
    if (!bio) return false;
    RSA* pub = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pub) return false;
    if (rsa_) RSA_free(rsa_);
    rsa_ = pub;
    return true;
}

static std::string base64Encode(const unsigned char* data, size_t len) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return encoded;
}

static std::vector<unsigned char> base64Decode(const std::string& input) {
    BIO* bio = BIO_new_mem_buf(input.data(), input.size());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    std::vector<unsigned char> buffer(input.size());
    int len = BIO_read(bio, buffer.data(), buffer.size());
    if (len < 0) len = 0;
    buffer.resize(len);
    BIO_free_all(bio);
    return buffer;
}

std::string RSAKeyManager::encrypt(const std::string& plaintext) const {
    if (!rsa_) return {};
    std::vector<unsigned char> out(RSA_size(rsa_));
    int len = RSA_public_encrypt(plaintext.size(), reinterpret_cast<const unsigned char*>(plaintext.data()), out.data(), rsa_, RSA_PKCS1_OAEP_PADDING);
    if (len <= 0) return {};
    return base64Encode(out.data(), len);
}

std::string RSAKeyManager::decrypt(const std::string& b64cipher) const {
    if (!rsa_) return {};
    auto cipher = base64Decode(b64cipher);
    std::vector<unsigned char> out(RSA_size(rsa_));
    int len = RSA_private_decrypt(cipher.size(), cipher.data(), out.data(), rsa_, RSA_PKCS1_OAEP_PADDING);
    if (len <= 0) return {};
    return std::string(reinterpret_cast<char*>(out.data()), len);
}
