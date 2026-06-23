#include "crypto/Signer.hpp"
#include "crypto/OutofProcessSigner.hpp"

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

void Signer::setOutofProcessSigner(OutofProcessSigner* oopSigner) {
    m_oopSigner = oopSigner;
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
            *m_provider,
            SigningDomain::USER_TRANSACTION
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
    if (m_oopSigner) {
        std::string payload = consensus::ValidatorVoteRecord::buildSigningPayload(
            address(), m_keyPair.publicKey(),
            blockIndex, blockHash, previousHash, round, decision, reasonHash, createdAt
        );
        crypto::SignatureRequest req{blockIndex, static_cast<std::uint32_t>(round), blockHash, payload};
        std::string sigHex;
        if (!m_oopSigner->signVote(req, sigHex)) {
            throw std::runtime_error("Signer rejected vote signing (possible double-sign).");
        }
        crypto::Signature signature(
            m_keyPair.publicKey().algorithm() == crypto::CryptoAlgorithm::BLS12_381 ? crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1 : crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
            crypto::SigningDomain::VALIDATOR_VOTE,
            m_keyPair.publicKey().algorithm(),
            m_keyPair.publicKey(),
            sigHex,
            createdAt
        );
        crypto::SignatureBundle bundle(crypto::SignatureBundleType::SINGLE);
        bundle.addSignature(signature);

        return consensus::ValidatorVoteRecord(
            address(),
            m_keyPair.publicKey(),
            blockIndex,
            blockHash,
            previousHash,
            round,
            decision,
            reasonHash,
            createdAt,
            bundle
        );
    }

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
