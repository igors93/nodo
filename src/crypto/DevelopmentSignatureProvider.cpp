#include "crypto/DevelopmentSignatureProvider.hpp"

#include "crypto/hash.h"

#include <stdexcept>

namespace nodo::crypto {

CryptoAlgorithm DevelopmentSignatureProvider::algorithm() const {
    return CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE;
}

Signature DevelopmentSignatureProvider::sign(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp
) const {
    if (message.empty()) {
        throw std::invalid_argument("Development signature message cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Development signature timestamp must be positive.");
    }

    if (publicKey.algorithm() != algorithm()) {
        throw std::invalid_argument("Development signature requires development public key.");
    }

    if (privateKey.algorithm() != algorithm()) {
        throw std::invalid_argument("Development signature requires development private key.");
    }

    if (!publicKey.isValid() || !privateKey.isValid()) {
        throw std::invalid_argument("Invalid development key pair.");
    }

    /*
     * Development-only decision:
     * This deterministic placeholder is verifiable using public information.
     *
     * It does NOT prove private-key ownership. It only preserves the architecture:
     * message -> provider -> signature -> provider verification.
     *
     * Real providers must replace this with real mathematical signing.
     */
    return Signature(
        algorithm(),
        publicKey,
        computeDevelopmentSignatureHex(message, publicKey),
        timestamp
    );
}

SignatureVerificationResult DevelopmentSignatureProvider::verify(
    const std::string& message,
    const Signature& signature
) const {
    if (message.empty()) {
        return SignatureVerificationResult::invalid("Message is empty.");
    }

    if (!signature.isValid()) {
        return SignatureVerificationResult::invalid("Signature is structurally invalid.");
    }

    if (signature.algorithm() != algorithm()) {
        return SignatureVerificationResult::invalid("Signature algorithm is not supported by this provider.");
    }

    if (signature.publicKey().algorithm() != algorithm()) {
        return SignatureVerificationResult::invalid("Signature public key algorithm is not supported by this provider.");
    }

    const std::string expectedSignatureHex =
        computeDevelopmentSignatureHex(
            message,
            signature.publicKey()
        );

    if (signature.signatureHex() != expectedSignatureHex) {
        return SignatureVerificationResult::invalid("Development signature mismatch.");
    }

    return SignatureVerificationResult::valid();
}

std::string DevelopmentSignatureProvider::computeDevelopmentSignatureHex(
    const std::string& message,
    const PublicKey& publicKey
) {
    const std::string payload =
        "NODO_DEVELOPMENT_SIGNATURE_PROVIDER_V1|"
        + message
        + "|publicKey="
        + publicKey.serialize();

    char output[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(payload.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::crypto
