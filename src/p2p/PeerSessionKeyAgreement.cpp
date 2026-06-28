#include "p2p/PeerSessionKeyAgreement.hpp"

#include "crypto/Hex.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <vector>

namespace nodo::p2p {

namespace {

using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyContextPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

std::optional<std::vector<unsigned char>> decodeKey(const std::string& value) {
    if (!crypto::hasHexByteSize(value, PeerSessionKeyAgreement::KEY_BYTES)) {
        return std::nullopt;
    }
    try {
        return crypto::hexDecode(value);
    } catch (...) {
        return std::nullopt;
    }
}

bool isSafeScalar(const std::string& value, std::size_t maximum = 256) {
    if (value.empty() || value.size() > maximum) return false;
    return value.find_first_of("{};\r\n") == std::string::npos;
}

} // namespace

bool PeerEphemeralKeyPair::isValid() const {
    return PeerSessionKeyAgreement::isValidPublicKey(publicKeyHex) &&
           PeerSessionKeyAgreement::isValidPrivateKey(privateKeyHex);
}

bool PeerSessionContext::isValid() const {
    return isSafeScalar(networkId) && isSafeScalar(chainId) &&
           isSafeScalar(protocolVersion) && isSafeScalar(challengerNodeId) &&
           isSafeScalar(challengedNodeId) &&
           challengerNodeId != challengedNodeId &&
           crypto::hasHexByteSize(challengeNonce, 32) &&
           PeerSessionKeyAgreement::isValidPublicKey(
               challengerEphemeralPublicKeyHex) &&
           PeerSessionKeyAgreement::isValidPublicKey(
               challengedEphemeralPublicKeyHex);
}

std::string PeerSessionContext::canonicalTranscript() const {
    if (!isValid()) return "";
    std::ostringstream output;
    output << "NodoPeerSession/v1"
           << "|network=" << networkId.size() << ":" << networkId
           << "|chain=" << chainId.size() << ":" << chainId
           << "|protocol=" << protocolVersion.size() << ":" << protocolVersion
           << "|challenger=" << challengerNodeId.size() << ":" << challengerNodeId
           << "|challenged=" << challengedNodeId.size() << ":" << challengedNodeId
           << "|nonce=" << challengeNonce
           << "|challengerKey=" << challengerEphemeralPublicKeyHex
           << "|challengedKey=" << challengedEphemeralPublicKeyHex;
    return output.str();
}

std::optional<PeerEphemeralKeyPair>
PeerSessionKeyAgreement::generateEphemeralKeyPair() {
    PkeyContextPtr context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr),
        EVP_PKEY_CTX_free
    );
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
        return std::nullopt;
    }

    EVP_PKEY* generated = nullptr;
    if (EVP_PKEY_keygen(context.get(), &generated) != 1) {
        return std::nullopt;
    }
    PkeyPtr key(generated, EVP_PKEY_free);

    std::array<unsigned char, KEY_BYTES> publicBytes = {};
    std::array<unsigned char, KEY_BYTES> privateBytes = {};
    std::size_t publicSize = publicBytes.size();
    std::size_t privateSize = privateBytes.size();
    if (EVP_PKEY_get_raw_public_key(
            key.get(), publicBytes.data(), &publicSize) != 1 ||
        EVP_PKEY_get_raw_private_key(
            key.get(), privateBytes.data(), &privateSize) != 1 ||
        publicSize != KEY_BYTES || privateSize != KEY_BYTES) {
        return std::nullopt;
    }

    return PeerEphemeralKeyPair{
        crypto::hexEncode(publicBytes.data(), publicBytes.size()),
        crypto::hexEncode(privateBytes.data(), privateBytes.size())
    };
}

std::optional<std::string> PeerSessionKeyAgreement::deriveSessionSecret(
    const std::string& localPrivateKeyHex,
    const std::string& remotePublicKeyHex,
    const PeerSessionContext& context
) {
    const auto privateBytes = decodeKey(localPrivateKeyHex);
    const auto publicBytes = decodeKey(remotePublicKeyHex);
    if (!privateBytes.has_value() || !publicBytes.has_value() ||
        !context.isValid()) {
        return std::nullopt;
    }

    PkeyPtr local(
        EVP_PKEY_new_raw_private_key(
            EVP_PKEY_X25519,
            nullptr,
            privateBytes->data(),
            privateBytes->size()),
        EVP_PKEY_free
    );
    PkeyPtr remote(
        EVP_PKEY_new_raw_public_key(
            EVP_PKEY_X25519,
            nullptr,
            publicBytes->data(),
            publicBytes->size()),
        EVP_PKEY_free
    );
    if (!local || !remote) return std::nullopt;

    PkeyContextPtr deriveContext(
        EVP_PKEY_CTX_new(local.get(), nullptr),
        EVP_PKEY_CTX_free
    );
    std::array<unsigned char, KEY_BYTES> shared = {};
    std::size_t sharedSize = shared.size();
    if (!deriveContext || EVP_PKEY_derive_init(deriveContext.get()) != 1 ||
        EVP_PKEY_derive_set_peer(deriveContext.get(), remote.get()) != 1 ||
        EVP_PKEY_derive(
            deriveContext.get(), shared.data(), &sharedSize) != 1 ||
        sharedSize != KEY_BYTES ||
        std::all_of(shared.begin(), shared.end(),
                    [](unsigned char byte) { return byte == 0; })) {
        return std::nullopt;
    }

    const std::string transcript = context.canonicalTranscript();
    static const std::string salt = "NodoPeerSession/HKDF-SHA256/v1";
    PkeyContextPtr hkdf(
        EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr),
        EVP_PKEY_CTX_free
    );
    std::array<unsigned char, KEY_BYTES> secret = {};
    std::size_t secretSize = secret.size();
    if (!hkdf || EVP_PKEY_derive_init(hkdf.get()) != 1 ||
        EVP_PKEY_CTX_set_hkdf_md(hkdf.get(), EVP_sha256()) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_salt(
            hkdf.get(),
            reinterpret_cast<const unsigned char*>(salt.data()),
            static_cast<int>(salt.size())) != 1 ||
        EVP_PKEY_CTX_set1_hkdf_key(
            hkdf.get(), shared.data(), static_cast<int>(shared.size())) != 1 ||
        EVP_PKEY_CTX_add1_hkdf_info(
            hkdf.get(),
            reinterpret_cast<const unsigned char*>(transcript.data()),
            static_cast<int>(transcript.size())) != 1 ||
        EVP_PKEY_derive(hkdf.get(), secret.data(), &secretSize) != 1 ||
        secretSize != KEY_BYTES) {
        return std::nullopt;
    }

    return crypto::hexEncode(secret.data(), secret.size());
}

bool PeerSessionKeyAgreement::isValidPublicKey(
    const std::string& publicKeyHex
) {
    return decodeKey(publicKeyHex).has_value();
}

bool PeerSessionKeyAgreement::isValidPrivateKey(
    const std::string& privateKeyHex
) {
    return decodeKey(privateKeyHex).has_value();
}

} // namespace nodo::p2p
