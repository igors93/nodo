#include "crypto/Ed25519SignatureProvider.hpp"

#include "crypto/Hex.hpp"
#include "crypto/SigningPayload.hpp"
#include "crypto/hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace nodo::crypto {

namespace {

constexpr std::size_t ED25519_KEY_SIZE = 32;
constexpr std::size_t ED25519_SIGNATURE_SIZE = 64;

std::vector<unsigned char> requireHexBytes(
    const std::string& hex,
    std::size_t expectedSize,
    const std::string& label
) {
    if (!hasHexByteSize(hex, expectedSize)) {
        throw std::invalid_argument(label + " has invalid hex size.");
    }

    return hexDecode(hex);
}

std::string bindMessage(
    const std::string& message,
    SigningDomain domain
) {
    return SigningPayload(
        CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        domain,
        message
    ).canonicalMessage();
}

std::string publicKeyFromPrivateKey(
    const std::vector<unsigned char>& privateKey
) {
    EVP_PKEY* key =
        EVP_PKEY_new_raw_private_key_ex(
            nullptr,
            "ED25519",
            nullptr,
            privateKey.data(),
            privateKey.size()
        );

    if (key == nullptr) {
        throw std::runtime_error("OpenSSL rejected Ed25519 private key material.");
    }

    std::array<unsigned char, ED25519_KEY_SIZE> publicBytes = {};
    std::size_t publicSize = publicBytes.size();

    if (EVP_PKEY_get_raw_public_key(
            key,
            publicBytes.data(),
            &publicSize
        ) != 1 ||
        publicSize != publicBytes.size()) {
        EVP_PKEY_free(key);
        throw std::runtime_error("OpenSSL failed to derive Ed25519 public key.");
    }

    EVP_PKEY_free(key);
    return hexEncode(publicBytes.data(), publicBytes.size());
}

std::array<unsigned char, ED25519_KEY_SIZE> seedToPrivateBytes(
    const std::string& seed
) {
    if (seed.empty()) {
        throw std::invalid_argument("Ed25519 deterministic seed cannot be empty.");
    }

    char hash[NODO_HASH_BUFFER_SIZE] = {0};
    const std::string material =
        "NODO_ED25519_TEST_KEY_SEED_V1|" + seed;
    nodo_hash_string(material.c_str(), hash, sizeof(hash));

    const std::vector<unsigned char> bytes =
        hexDecode(std::string(hash));

    std::array<unsigned char, ED25519_KEY_SIZE> output = {};
    std::copy(
        bytes.begin(),
        bytes.begin() + static_cast<std::ptrdiff_t>(output.size()),
        output.begin()
    );

    return output;
}

} // namespace

CryptoAlgorithm Ed25519SignatureProvider::algorithm() const {
    return CryptoAlgorithm::CLASSIC_ED25519;
}

Signature Ed25519SignatureProvider::sign(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp,
    SigningDomain domain
) const {
    if (message.empty()) {
        throw std::invalid_argument("Ed25519 signature message cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Ed25519 signature timestamp must be positive.");
    }

    if (domain == SigningDomain::UNKNOWN) {
        throw std::invalid_argument("Ed25519 signature domain cannot be UNKNOWN.");
    }

    if (publicKey.algorithm() != algorithm() ||
        privateKey.algorithm() != algorithm()) {
        throw std::invalid_argument("Ed25519 signature requires Ed25519 keys.");
    }

    const std::vector<unsigned char> privateBytes =
        requireHexBytes(
            privateKey.keyMaterialForSigningOnly(),
            ED25519_KEY_SIZE,
            "Ed25519 private key"
        );

    if (publicKey.keyMaterial() != publicKeyFromPrivateKey(privateBytes)) {
        throw std::invalid_argument("Ed25519 public key does not match private key.");
    }

    const std::string canonicalMessage =
        bindMessage(message, domain);

    EVP_PKEY* key =
        EVP_PKEY_new_raw_private_key_ex(
            nullptr,
            "ED25519",
            nullptr,
            privateBytes.data(),
            privateBytes.size()
        );

    if (key == nullptr) {
        throw std::runtime_error("OpenSSL rejected Ed25519 private key.");
    }

    EVP_MD_CTX* context =
        EVP_MD_CTX_new();

    if (context == nullptr) {
        EVP_PKEY_free(key);
        throw std::runtime_error("OpenSSL failed to allocate Ed25519 signing context.");
    }

    std::array<unsigned char, ED25519_SIGNATURE_SIZE> signatureBytes = {};
    std::size_t signatureSize = signatureBytes.size();

    const int ok =
        EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestSign(
            context,
            signatureBytes.data(),
            &signatureSize,
            reinterpret_cast<const unsigned char*>(canonicalMessage.data()),
            canonicalMessage.size()
        ) == 1 &&
        signatureSize == signatureBytes.size();

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);

    if (!ok) {
        throw std::runtime_error("OpenSSL Ed25519 signing failed.");
    }

    return Signature(
        CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
        domain,
        algorithm(),
        publicKey,
        hexEncode(signatureBytes.data(), signatureBytes.size()),
        timestamp
    );
}

SignatureVerificationResult Ed25519SignatureProvider::verify(
    const std::string& message,
    const Signature& signature
) const {
    if (message.empty()) {
        return SignatureVerificationResult::invalid("Message is empty.");
    }

    if (!signature.isValid()) {
        return SignatureVerificationResult::invalid("Signature is structurally invalid.");
    }

    if (signature.algorithm() != algorithm() ||
        signature.publicKey().algorithm() != algorithm()) {
        return SignatureVerificationResult::invalid("Signature is not Ed25519.");
    }

    try {
        const std::vector<unsigned char> publicBytes =
            requireHexBytes(
                signature.publicKey().keyMaterial(),
                ED25519_KEY_SIZE,
                "Ed25519 public key"
            );

        const std::vector<unsigned char> signatureBytes =
            requireHexBytes(
                signature.signatureHex(),
                ED25519_SIGNATURE_SIZE,
                "Ed25519 signature"
            );

        const std::string canonicalMessage =
            bindMessage(message, signature.domain());

        EVP_PKEY* key =
            EVP_PKEY_new_raw_public_key_ex(
                nullptr,
                "ED25519",
                nullptr,
                publicBytes.data(),
                publicBytes.size()
            );

        if (key == nullptr) {
            return SignatureVerificationResult::invalid("OpenSSL rejected Ed25519 public key.");
        }

        EVP_MD_CTX* context =
            EVP_MD_CTX_new();

        if (context == nullptr) {
            EVP_PKEY_free(key);
            return SignatureVerificationResult::invalid("OpenSSL failed to allocate verification context.");
        }

        const int verifyStatus =
            EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key) == 1
                ? EVP_DigestVerify(
                      context,
                      signatureBytes.data(),
                      signatureBytes.size(),
                      reinterpret_cast<const unsigned char*>(canonicalMessage.data()),
                      canonicalMessage.size()
                  )
                : 0;

        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);

        if (verifyStatus != 1) {
            return SignatureVerificationResult::invalid("Ed25519 signature mismatch.");
        }
    } catch (const std::exception& error) {
        return SignatureVerificationResult::invalid(error.what());
    }

    return SignatureVerificationResult::valid();
}

KeyPair Ed25519SignatureProvider::generateKeyPair() {
    std::array<unsigned char, ED25519_KEY_SIZE> privateBytes = {};

    if (RAND_bytes(privateBytes.data(), static_cast<int>(privateBytes.size())) != 1) {
        throw std::runtime_error("OpenSSL RAND_bytes failed for Ed25519 key generation.");
    }

    const std::vector<unsigned char> privateVector(
        privateBytes.begin(),
        privateBytes.end()
    );

    return KeyPair(
        PublicKey(
            CryptoAlgorithm::CLASSIC_ED25519,
            publicKeyFromPrivateKey(privateVector)
        ),
        PrivateKey(
            CryptoAlgorithm::CLASSIC_ED25519,
            hexEncode(privateBytes.data(), privateBytes.size())
        )
    );
}

KeyPair Ed25519SignatureProvider::deriveKeyPairFromSeed(
    const std::string& seed
) {
    const std::array<unsigned char, ED25519_KEY_SIZE> privateBytes =
        seedToPrivateBytes(seed);

    const std::vector<unsigned char> privateVector(
        privateBytes.begin(),
        privateBytes.end()
    );

    return KeyPair(
        PublicKey(
            CryptoAlgorithm::CLASSIC_ED25519,
            publicKeyFromPrivateKey(privateVector)
        ),
        PrivateKey(
            CryptoAlgorithm::CLASSIC_ED25519,
            hexEncode(privateBytes.data(), privateBytes.size())
        )
    );
}

bool Ed25519SignatureProvider::isValidPublicKeyMaterial(
    const std::string& keyMaterial
) {
    return hasHexByteSize(keyMaterial, ED25519_KEY_SIZE);
}

bool Ed25519SignatureProvider::isValidPrivateKeyMaterial(
    const std::string& keyMaterial
) {
    return hasHexByteSize(keyMaterial, ED25519_KEY_SIZE);
}

} // namespace nodo::crypto
