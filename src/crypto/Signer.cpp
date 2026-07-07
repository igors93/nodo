#include "crypto/Signer.hpp"
#include "crypto/OutofProcessSigner.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::crypto {

Signer::Signer(KeyPair keyPair, const SignatureProvider &provider)
    : m_keyPair(std::move(keyPair)), m_provider(&provider) {
  if (!m_keyPair.isValid()) {
    throw std::invalid_argument("Signer requires a valid KeyPair.");
  }

  if (m_provider == nullptr ||
      m_provider->algorithm() != m_keyPair.algorithm()) {
    throw std::invalid_argument(
        "Signer provider does not match KeyPair algorithm.");
  }
}

void Signer::setOutofProcessSigner(OutofProcessSigner *oopSigner) {
  m_oopSigner = oopSigner;
}

const KeyPair &Signer::keyPair() const { return m_keyPair; }

std::string Signer::address() const { return m_keyPair.address().value(); }

core::Transaction Signer::signTransaction(core::Transaction transaction,
                                          std::int64_t timestamp) const {
  transaction.attachSignatureBundle(
      m_keyPair.sign(transaction.signingPayload(), timestamp, *m_provider,
                     SigningDomain::USER_TRANSACTION));

  return transaction;
}

crypto::SignatureBundle Signer::signValidatorPayload(
    const std::string &payload, std::uint64_t height, std::uint64_t round,
    const std::string &proposalHash, std::int64_t timestamp,
    SigningDomain domain) const {
  if (payload.empty() || height == 0 ||
      round > static_cast<std::uint64_t>(
                  std::numeric_limits<std::uint32_t>::max()) ||
      proposalHash.empty() || timestamp <= 0) {
    throw std::invalid_argument(
        "Validator signing payload request is invalid.");
  }

  if (m_oopSigner != nullptr) {
    crypto::SignatureRequest req{height, static_cast<std::uint32_t>(round),
                                 proposalHash, payload};
    std::string sigHex;
    bool signedOk = false;
    if (domain == SigningDomain::VALIDATOR_BLOCK_PROPOSAL) {
      signedOk = m_oopSigner->signBlockProposal(req, sigHex);
    } else if (domain == SigningDomain::VALIDATOR_VOTE) {
      signedOk = m_oopSigner->signVote(req, sigHex);
    } else {
      throw std::invalid_argument(
          "Unsupported external validator signing domain.");
    }
    if (!signedOk) {
      throw std::runtime_error(
          "External validator signer rejected signing request.");
    }
    crypto::Signature signature(crypto::CryptoSuiteId::NODO_CRYPTO_SUITE_V1,
                                domain, m_keyPair.publicKey().algorithm(),
                                m_keyPair.publicKey(), sigHex, timestamp);
    crypto::SignatureBundle bundle;
    bundle.addSignature(signature);
    return bundle;
  }

  return m_keyPair.sign(payload, timestamp, *m_provider, domain);
}

consensus::ValidatorVoteRecord Signer::signValidatorVote(
    std::uint64_t blockIndex, const std::string &blockHash,
    const std::string &previousHash, std::uint64_t round,
    consensus::ValidatorVoteDecision decision, const std::string &reasonHash,
    std::int64_t createdAt) const {
  if (m_oopSigner) {
    std::string payload = consensus::ValidatorVoteRecord::buildSigningPayload(
        address(), m_keyPair.publicKey(), blockIndex, blockHash, previousHash,
        round, decision, reasonHash, createdAt);
    crypto::SignatureBundle bundle =
        signValidatorPayload(payload, blockIndex, round, blockHash, createdAt,
                             SigningDomain::VALIDATOR_VOTE);

    return consensus::ValidatorVoteRecord(
        address(), m_keyPair.publicKey(), blockIndex, blockHash, previousHash,
        round, decision, reasonHash, createdAt, bundle);
  }

  return consensus::ValidatorVoteRecord::createVote(
      address(), m_keyPair.publicKey(), m_keyPair.privateKeyForSigningOnly(),
      blockIndex, blockHash, previousHash, round, decision, reasonHash,
      createdAt, *m_provider);
}

} // namespace nodo::crypto
