#ifndef NODO_CONSENSUS_PROPOSAL_JUSTIFICATION_HPP
#define NODO_CONSENSUS_PROPOSAL_JUSTIFICATION_HPP

#include "consensus/QuorumCertificate.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

enum class ProposalJustificationType { NONE, UNLOCK_QUORUM_CERTIFICATE };

std::string proposalJustificationTypeToString(ProposalJustificationType type);

/*
 * ProposalJustification is the typed "Proof-Of-Lock" a proposer attaches to
 * a block proposal when nominating a block different from what quorum may
 * already be locked on from an earlier round.
 *
 * Security principle:
 * A validator that already PRECOMMITted (locked) on a block may only vote
 * for a conflicting block if 2/3+ voting weight already proved — via a real
 * QuorumCertificate — that it precommitted to that exact block in a round at
 * least as recent as the lock. This is the only thing that makes it safe to
 * unlock: without it, two conflicting blocks could each gather enough votes
 * to finalize at the same height. This type makes that proof a first-class,
 * always-well-formed value instead of an opaque string that callers had to
 * know, by convention, to interpret as a serialized QuorumCertificate.
 */
class ProposalJustification {
public:
  ProposalJustification();

  static ProposalJustification none();
  static ProposalJustification
  unlockQuorumCertificate(QuorumCertificate certificate);

  ProposalJustificationType type() const;
  bool hasUnlockCertificate() const;
  // Throws std::logic_error if hasUnlockCertificate() is false.
  const QuorumCertificate &unlockCertificate() const;

  bool isValid() const;
  std::string serialize() const;
  static ProposalJustification deserialize(const std::string &serialized);

  /*
   * The BFT unlock safety rule itself, extracted from ConsensusEventLoop so
   * it is independently testable: is it safe for a validator locked at
   * lockedRound to vote for candidateBlockHash instead, in round `round`,
   * given this justification? validators must be the validator set active
   * at the candidate's height; policy/provider drive the certificate's
   * cryptographic verification. Never throws; a malformed or absent
   * justification simply yields false.
   */
  static bool permitsUnlock(const ProposalJustification &justification,
                            std::uint64_t lockedRound,
                            const std::string &candidateBlockHash,
                            std::uint64_t round,
                            const core::ValidatorRegistry &validators,
                            const crypto::CryptoPolicy &policy,
                            const crypto::SignatureProvider &provider,
                            std::string *reason = nullptr);

private:
  ProposalJustificationType m_type;
  QuorumCertificate m_certificate;
};

} // namespace nodo::consensus

#endif
