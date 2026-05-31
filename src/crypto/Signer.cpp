#include "crypto/Signer.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::crypto {

Signer::Signer(
    KeyPair keyPair,
    const SignatureProvider& provider
)
    : m_keyPair(std::move(keyPair)),
      m_provider(&provider) {
    if (!m_keyPair.isValid()) {
        throw std::invalid_argument("Signer requires a valid KeyPair.");
    }

    if (m_provider == nullptr ||
        m_provider->algorithm() != m_keyPair.algorithm()) {
        throw std::invalid_argument("Signer provider does not match KeyPair algorithm.");
    }
}

const KeyPair& Signer::keyPair() const {
    return m_keyPair;
}

std::string Signer::address() const {
    return m_keyPair.address().value();
}

core::Transaction Signer::signTransaction(
    core::Transaction transaction,
    std::int64_t timestamp
) const {
    transaction.attachSignatureBundle(
        m_keyPair.sign(
            transaction.signingPayload(),
            timestamp,
            *m_provider
        )
    );

    return transaction;
}

consensus::ValidatorVoteRecord Signer::signValidatorVote(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    const std::string& previousHash,
    std::uint64_t round,
    consensus::ValidatorVoteDecision decision,
    const std::string& reasonHash,
    std::int64_t createdAt
) const {
    return consensus::ValidatorVoteRecord::createVote(
        address(),
        m_keyPair.publicKey(),
        m_keyPair.privateKeyForSigningOnly(),
        blockIndex,
        blockHash,
        previousHash,
        round,
        decision,
        reasonHash,
        createdAt,
        *m_provider
    );
}

} // namespace nodo::crypto
