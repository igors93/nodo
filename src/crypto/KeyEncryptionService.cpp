#include "crypto/KeyEncryptionService.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nodo::crypto {

namespace {

// --- hex helpers -----------------------------------------------------------

std::string bytesToHex(const unsigned char* data, std::size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

bool hexToBytes(
    const std::string& hex,
    std::vector<unsigned char>& out
) {
    if (hex.size() % 2 != 0) return false;
    out.resize(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned int byte = 0;
        const char hi = hex[i * 2];
        const char lo = hex[i * 2 + 1];
        auto toNibble = [](char c, bool& ok) -> unsigned int {
            if (c >= '0' && c <= '9') return static_cast<unsigned int>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<unsigned int>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<unsigned int>(c - 'A' + 10);
            ok = false;
            return 0;
        };
        bool ok = true;
        byte = (toNibble(hi, ok) << 4) | toNibble(lo, ok);
        if (!ok) return false;
        out[i] = static_cast<unsigned char>(byte);
    }
    return true;
}

// --- PBKDF2-HMAC-SHA256 key derivation ------------------------------------

bool deriveKey(
    const std::string& password,
    const unsigned char* salt,
    int saltLen,
    unsigned char* keyOut,
    int keyLen
) {
    return PKCS5_PBKDF2_HMAC(
        password.c_str(),
        static_cast<int>(password.size()),
        salt,
        saltLen,
        KeyEncryptionService::PBKDF2_ITERS,
        EVP_sha256(),
        keyLen,
        keyOut
    ) == 1;
}

// --- AES-256-GCM encrypt/decrypt ------------------------------------------

constexpr const char* ENVELOPE_PREFIX = "NODO_ENC_V1:";

} // namespace

std::string KeyEncryptionService::encrypt(
    const std::string& keyId,
    const std::string& plaintext,
    const std::string& password
) {
    if (plaintext.empty() || password.empty() || keyId.empty()) {
        return "";
    }

    unsigned char salt[SALT_BYTES];
    unsigned char nonce[NONCE_BYTES];
    if (RAND_bytes(salt, SALT_BYTES) != 1 ||
        RAND_bytes(nonce, NONCE_BYTES) != 1) {
        return "";
    }

    unsigned char key[KEY_BYTES];
    if (!deriveKey(password, salt, SALT_BYTES, key, KEY_BYTES)) {
        return "";
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    unsigned char tag[TAG_BYTES];
    int outLen = 0;
    int totalLen = 0;
    bool ok = false;

    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_BYTES, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;

        // AAD: bind ciphertext to keyId
        if (EVP_EncryptUpdate(ctx, nullptr, &outLen,
                reinterpret_cast<const unsigned char*>(keyId.c_str()),
                static_cast<int>(keyId.size())) != 1) break;

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen,
                reinterpret_cast<const unsigned char*>(plaintext.c_str()),
                static_cast<int>(plaintext.size())) != 1) break;
        totalLen = outLen;

        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + totalLen, &outLen) != 1) break;
        totalLen += outLen;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_BYTES, tag) != 1) break;
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    std::memset(key, 0, sizeof(key));

    if (!ok) return "";

    // Encode: prefix + hex(salt) + hex(nonce) + hex(tag) + hex(ciphertext)
    return std::string(ENVELOPE_PREFIX)
        + bytesToHex(salt, SALT_BYTES)
        + bytesToHex(nonce, NONCE_BYTES)
        + bytesToHex(tag, TAG_BYTES)
        + bytesToHex(ciphertext.data(), static_cast<std::size_t>(totalLen));
}

std::string KeyEncryptionService::decrypt(
    const std::string& keyId,
    const std::string& envelope,
    const std::string& password
) {
    if (!isEncryptedEnvelope(envelope)) return "";
    if (password.empty() || keyId.empty()) return "";

    const std::string hex = envelope.substr(std::string(ENVELOPE_PREFIX).size());

    // Minimum hex chars: salt(64) + nonce(24) + tag(32) + at least 2 for ciphertext
    const std::size_t minHexLen = SALT_BYTES * 2 + NONCE_BYTES * 2 + TAG_BYTES * 2;
    if (hex.size() < minHexLen) return "";

    std::size_t pos = 0;
    std::vector<unsigned char> salt, nonce, tag, ciphertext;
    if (!hexToBytes(hex.substr(pos, SALT_BYTES * 2), salt)) return "";
    pos += SALT_BYTES * 2;
    if (!hexToBytes(hex.substr(pos, NONCE_BYTES * 2), nonce)) return "";
    pos += NONCE_BYTES * 2;
    if (!hexToBytes(hex.substr(pos, TAG_BYTES * 2), tag)) return "";
    pos += TAG_BYTES * 2;
    if (!hexToBytes(hex.substr(pos), ciphertext)) return "";

    unsigned char key[KEY_BYTES];
    if (!deriveKey(password, salt.data(), SALT_BYTES, key, KEY_BYTES)) return "";

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { std::memset(key, 0, sizeof(key)); return ""; }

    std::vector<unsigned char> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;
    int totalLen = 0;
    bool ok = false;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NONCE_BYTES, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce.data()) != 1) break;

        // AAD
        if (EVP_DecryptUpdate(ctx, nullptr, &outLen,
                reinterpret_cast<const unsigned char*>(keyId.c_str()),
                static_cast<int>(keyId.size())) != 1) break;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &outLen,
                ciphertext.data(),
                static_cast<int>(ciphertext.size())) != 1) break;
        totalLen = outLen;

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_BYTES,
                const_cast<unsigned char*>(tag.data())) != 1) break;

        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + totalLen, &outLen) != 1) break;
        totalLen += outLen;
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    std::memset(key, 0, sizeof(key));

    if (!ok) return "";
    return std::string(
        reinterpret_cast<char*>(plaintext.data()),
        static_cast<std::size_t>(totalLen)
    );
}

bool KeyEncryptionService::verify(
    const std::string& keyId,
    const std::string& envelope,
    const std::string& password
) {
    return !decrypt(keyId, envelope, password).empty();
}

bool KeyEncryptionService::isEncryptedEnvelope(const std::string& value) {
    return value.rfind(ENVELOPE_PREFIX, 0) == 0;
}

} // namespace nodo::crypto
