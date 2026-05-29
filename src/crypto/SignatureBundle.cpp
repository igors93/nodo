#include "crypto/SignatureBundle.hpp"

#include "crypto/DevelopmentSignatureProvider.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::crypto {

SignatureBundle::SignatureBundle() = default;

void SignatureBundle::addSignature(const Signature& signature) {
    if (!signature.isValid()) {
        throw std::invalid_argument("Invalid signature rejected by SignatureBundle.");
    }

    /*
     * Basic safety:
     * Do not allow two signatures with the same algorithm in the same bundle.
     * This can be revisited later for multisig.
     */
    for (const auto& existing : m_signatures) {
        if (existing.algorithm() == signature.algorithm()) {
            throw std::logic_error("Duplicated signature algorithm rejected.");
        }
    }

    m_signatures.push_back(signature);
}

const std::vector<Signature>& SignatureBundle::signatures() const {
    return m_signatures;
}

bool SignatureBundle::empty() const {
    return m_signatures.empty();
}

bool SignatureBundle::hasAlgorithm(CryptoAlgorithm algorithm) const {
    for (const auto& signature : m_signatures) {
        if (signature.algorithm() == algorithm) {
            return true;
        }
    }

    return false;
}

bool SignatureBundle::isValidForPolicy(
    const CryptoPolicy& policy,
    SecurityContext context
) const {
    if (m_signatures.empty()) {
        return false;
    }

    bool hasClassic = false;
    bool hasPostQuantum = false;

    for (const auto& signature : m_signatures) {
        if (!signature.isValid()) {
            return false;
        }

        if (!policy.isAlgorithmAllowed(signature.algorithm(), context)) {
            return false;
        }

        if (isClassicAlgorithm(signature.algorithm())) {
            hasClassic = true;
        }

        if (isPostQuantumAlgorithm(signature.algorithm())) {
            hasPostQuantum = true;
        }
    }

    /*
     * In future policies, critical operations may require hybrid signatures.
     */
    if (policy.requiresHybridSignature(context)) {
        return hasClassic && hasPostQuantum;
    }

    return true;
}

bool SignatureBundle::verifyForPolicy(
    const std::string& message,
    const CryptoPolicy& policy,
    SecurityContext context,
    const SignatureProvider& provider
) const {
    if (!isValidForPolicy(policy, context)) {
        return false;
    }

    if (message.empty()) {
        return false;
    }

    for (const auto& signature : m_signatures) {
        if (signature.algorithm() != provider.algorithm()) {
            return false;
        }

        const SignatureVerificationResult result =
            provider.verify(
                message,
                signature
            );

        if (!result.success()) {
            return false;
        }
    }

    return true;
}

std::string SignatureBundle::serialize() const {
    std::ostringstream oss;

    oss << "SignatureBundle{";

    for (std::size_t i = 0; i < m_signatures.size(); ++i) {
        if (i > 0) {
            oss << ";";
        }

        oss << m_signatures[i].serialize();
    }

    oss << "}";

    return oss.str();
}

SignatureBundle SignatureBundle::createSignature(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp,
    const SignatureProvider& provider
) {
    SignatureBundle bundle;

    bundle.addSignature(
        provider.sign(
            message,
            publicKey,
            privateKey,
            timestamp
        )
    );

    return bundle;
}

SignatureBundle SignatureBundle::createDevelopmentSignature(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp
) {
    const DevelopmentSignatureProvider provider;

    return createSignature(
        message,
        publicKey,
        privateKey,
        timestamp,
        provider
    );
}

} // namespace nodo::crypto
