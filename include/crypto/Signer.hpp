#ifndef NODO_CRYPTO_SIGNER_HPP
#define NODO_CRYPTO_SIGNER_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Transaction.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/SignatureProvider.hpp"
#include "crypto/SigningDomain.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * Signer is the protocol boundary for actions that require local key material.
 *
 * Runtime and CLI code should ask a Signer to sign protocol payloads instead of
 * constructing signatures or private keys inline.
 */
class Signer {
public:
  Signer(KeyPair keyPair, const SignatureProvider &provider);

  void setOutofProcessSigner(class OutofProcessSigner *oopSigner);

  const KeyPair &keyPair() const;
  std::string address() const;

  core::Transaction signTransaction(core::Transaction transaction,
                                    std::int64_t timestamp) const;

  crypto::SignatureBundle
  signValidatorPayload(const std::string &payload, std::uint64_t height,
                       std::uint64_t round, const std::string &proposalHash,
                       std::int64_t timestamp, SigningDomain domain) const;

  consensus::ValidatorVoteRecord
  signValidatorVote(std::uint64_t blockIndex, const std::string &blockHash,
                    const std::string &previousHash, std::uint64_t round,
                    consensus::ValidatorVoteDecision decision,
                    const std::string &reasonHash,
                    std::int64_t createdAt) const;

private:
  KeyPair m_keyPair;
  const SignatureProvider *m_provider;
  class OutofProcessSigner *m_oopSigner = nullptr;
};

} // namespace nodo::crypto

#endif
