#ifndef NODO_NODE_SIGNED_BLOCK_PROPOSAL_MESSAGE_HPP
#define NODO_NODE_SIGNED_BLOCK_PROPOSAL_MESSAGE_HPP

#include "core/Block.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

/*
 * SignedBlockProposalMessage carries a proposed Block together with the
 * proposer's identity and a cryptographic signature over the proposal metadata.
 *
 * Security properties:
 *   - Binds the block hash, height, and round to the proposer's key.
 *   - Prevents replay: a valid signature for (height=5, round=1) is invalid
 *     for (height=5, round=2) or any other combination.
 *   - Proposer identity is verified against the local ValidatorRegistry,
 *     preventing impersonation by non-registered peers.
 *
 * Wire format: serialize() / deserialize() use a canonical text envelope.
 */
class SignedBlockProposalMessage {
public:
  static constexpr const char *SCHEMA_ID =
      "NODO_SIGNED_BLOCK_PROPOSAL_MESSAGE_V1";

  SignedBlockProposalMessage();

  SignedBlockProposalMessage(std::string proposerAddress,
                             crypto::PublicKey proposerPublicKey,
                             std::uint64_t blockIndex, std::uint64_t round,
                             std::string blockHash, std::string serializedBlock,
                             std::int64_t proposedAt,
                             crypto::SignatureBundle signatureBundle,
                             std::string justification = "");

  const std::string &proposerAddress() const;
  const crypto::PublicKey &proposerPublicKey() const;
  std::uint64_t blockIndex() const;
  std::uint64_t round() const;
  const std::string &blockHash() const;
  const std::string &serializedBlock() const;
  std::int64_t proposedAt() const;
  const crypto::SignatureBundle &signatureBundle() const;
  const std::string &justification() const;

  bool isValid() const;

  /*
   * Verify the proposer signature for this message.
   *
   * Confirms:
   *   1. The proposer address matches the expectedProposer from the schedule.
   *   2. The proposer public key is registered in the validator registry.
   *   3. The signature verifies against the canonical signing payload.
   */
  bool verify(const std::string &expectedProposer,
              const core::ValidatorRegistry &validatorRegistry,
              const crypto::CryptoPolicy &policy,
              const crypto::SignatureProvider &provider) const;

  std::string serialize() const;

  static SignedBlockProposalMessage deserialize(const std::string &text);

  static std::string
  buildSigningPayload(const std::string &proposerAddress,
                      const crypto::PublicKey &proposerPublicKey,
                      const std::string &blockHash, std::uint64_t blockIndex,
                      std::uint64_t round, std::int64_t proposedAt,
                      const std::string &justification = "");

  static SignedBlockProposalMessage fromSignatureBundle(
      const core::Block &block, const std::string &proposerAddress,
      const crypto::PublicKey &proposerPublicKey, std::uint64_t round,
      std::int64_t proposedAt, crypto::SignatureBundle signatureBundle,
      std::string justification = "");

  static SignedBlockProposalMessage
  sign(const core::Block &block, const std::string &proposerAddress,
       const crypto::PublicKey &proposerPublicKey,
       const crypto::PrivateKey &proposerPrivateKey, std::uint64_t round,
       std::int64_t proposedAt, const crypto::SignatureProvider &provider,
       const std::string &justification = "");

private:
  std::string m_proposerAddress;
  crypto::PublicKey m_proposerPublicKey;
  std::uint64_t m_blockIndex;
  std::uint64_t m_round;
  std::string m_blockHash;
  std::string m_serializedBlock;
  std::int64_t m_proposedAt;
  crypto::SignatureBundle m_signatureBundle;
  std::string m_justification;
};

} // namespace nodo::node

#endif
