#include "crypto/KeyPair.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::crypto {

KeyPair::KeyPair()
    : m_publicKey(),
      m_privateKey() {}

KeyPair::KeyPair(
    PublicKey publicKey,
    PrivateKey privateKey
)
    : m_publicKey(std::move(publicKey)),
      m_privateKey(std::move(privateKey)) {}

KeyPair KeyPair::createEd25519KeyPair() {
    return Ed25519SignatureProvider::generateKeyPair();
}

KeyPair KeyPair::createDeterministicEd25519KeyPair(
    const std::string& identitySeed
) {
    return Ed25519SignatureProvider::deriveKeyPairFromSeed(identitySeed);
}

KeyPair KeyPair::createBls12381KeyPair() {
    return Bls12381SignatureProvider::generateKeyPair();
}

KeyPair KeyPair::createDeterministicBls12381KeyPair(
    const std::string& identitySeed
) {
    return Bls12381SignatureProvider::deriveKeyPairFromSeed(identitySeed);
}

CryptoAlgorithm KeyPair::algorithm() const {
    if (!isValid()) {
        return CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE;
    }

    return m_publicKey.algorithm();
}

const PublicKey& KeyPair::publicKey() const {
    return m_publicKey;
}

const PrivateKey& KeyPair::privateKeyForSigningOnly() const {
    return m_privateKey;
}

Address KeyPair::address() const {
    if (!isValid()) {
        throw std::invalid_argument("Cannot derive address from invalid KeyPair.");
    }

    return AddressDerivation::deriveFromPublicKey(m_publicKey);
}

bool KeyPair::isValid() const {
    if (!m_publicKey.isValid()) {
        return false;
    }

    if (!m_privateKey.isValid()) {
        return false;
    }

    if (m_publicKey.algorithm() != m_privateKey.algorithm()) {
        return false;
    }

    return true;
}

bool KeyPair::canSignAndVerify(
    const SignatureProvider& provider
) const {
    if (!isValid()) {
        return false;
    }

    if (provider.algorithm() != m_publicKey.algorithm()) {
        return false;
    }

    const std::string challenge =
        signingChallenge(m_publicKey);

    const SigningDomain domain =
        provider.algorithm() == CryptoAlgorithm::BLS12_381
            ? SigningDomain::VALIDATOR_VOTE
            : SigningDomain::USER_TRANSACTION;

    try {
        const SignatureBundle bundle =
            sign(
                challenge,
                1900000000,
                provider,
                domain
            );

        if (bundle.signatures().empty()) {
            return false;
        }

        const SignatureVerificationResult result =
            provider.verify(
                challenge,
                bundle.signatures().front()
            );

        return result.success();
    } catch (...) {
        return false;
    }
}

SignatureBundle KeyPair::sign(
    const std::string& message,
    std::int64_t timestamp,
    const SignatureProvider& provider,
    SigningDomain domain
) const {
    if (!isValid()) {
        throw std::invalid_argument("Invalid KeyPair cannot sign.");
    }

    if (message.empty()) {
        throw std::invalid_argument("KeyPair signing message cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("KeyPair signing timestamp must be positive.");
    }

    if (provider.algorithm() != m_publicKey.algorithm()) {
        throw std::invalid_argument("Signature provider algorithm does not match KeyPair.");
    }

    return SignatureBundle::createSignature(
        message,
        m_publicKey,
        m_privateKey,
        timestamp,
        provider,
        domain
    );
}

std::string KeyPair::publicIdentity() const {
    if (!isValid()) {
        throw std::invalid_argument("Invalid KeyPair has no public identity.");
    }

    std::ostringstream oss;

    oss << "KeyIdentity{"
        << "algorithm=" << cryptoAlgorithmToString(m_publicKey.algorithm())
        << ";publicKeyFingerprint=" << m_publicKey.fingerprint()
        << ";address=" << address().value()
        << "}";

    return oss.str();
}

std::string KeyPair::serializePublic() const {
    if (!isValid()) {
        throw std::invalid_argument("Invalid KeyPair cannot be serialized.");
    }

    std::ostringstream oss;

    oss << "KeyPairPublic{"
        << "publicKey=" << m_publicKey.serialize()
        << ";address=" << address().value()
        << "}";

    return oss.str();
}

std::string KeyPair::signingChallenge(
    const PublicKey& publicKey
) {
    return "NODO_KEYPAIR_CHALLENGE_V1|" + publicKey.serialize();
}

} // namespace nodo::crypto
