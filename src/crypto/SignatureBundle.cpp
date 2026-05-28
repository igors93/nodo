#include "crypto/SignatureBundle.hpp"
#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>

namespace nodo::crypto {

SignatureBundle::SignatureBundle() = default;

void SignatureBundle::addSignature(const Signature& signature) {
    if (!signature.isValid()) {
        throw std::invalid_argument("Invalid signature rejected by SignatureBundle.");
    }

    /*
     * Segurança básica:
     * Não permitir duas assinaturas do mesmo algoritmo no mesmo bundle.
     * No futuro podemos relaxar isso para multisig, mas agora mantemos simples.
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
     * No futuro, operações críticas podem exigir assinatura híbrida.
     */
    if (policy.requiresHybridSignature(context)) {
        return hasClassic && hasPostQuantum;
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

SignatureBundle SignatureBundle::createDevelopmentSignature(
    const std::string& message,
    const PublicKey& publicKey,
    const PrivateKey& privateKey,
    std::int64_t timestamp
) {
    if (publicKey.algorithm() != CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE) {
        throw std::invalid_argument("Development signature requires development public key.");
    }

    if (privateKey.algorithm() != CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE) {
        throw std::invalid_argument("Development signature requires development private key.");
    }

    if (!publicKey.isValid() || !privateKey.isValid()) {
        throw std::invalid_argument("Invalid development key pair.");
    }

    /*
     * ATENÇÃO:
     * Isto NÃO É UMA ASSINATURA REAL.
     *
     * Estamos usando hash apenas para criar um valor determinístico
     * e testar a arquitetura.
     *
     * No futuro, esta função será substituída por provedores reais:
     * - Ed25519
     * - ECDSA
     * - ML-DSA
     * - SLH-DSA
     */
    const std::string payload =
        "NODO_DEVELOPMENT_SIGNATURE|"
        + message
        + "|publicKey="
        + publicKey.serialize()
        + "|privateKey="
        + privateKey.keyMaterialForSigningOnly();

    char output[65] = {0};
    nodo_hash_string(payload.c_str(), output, sizeof(output));

    Signature signature(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        publicKey,
        std::string(output),
        timestamp
    );

    SignatureBundle bundle;
    bundle.addSignature(signature);

    return bundle;
}

} // namespace nodo::crypto