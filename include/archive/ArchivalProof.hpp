#ifndef NODO_ARCHIVE_ARCHIVAL_PROOF_HPP
#define NODO_ARCHIVE_ARCHIVAL_PROOF_HPP

#include "archive/ArchivalChallenge.hpp"
#include "archive/ArchiveProvider.hpp"
#include "archive/ArchiveSegment.hpp"
#include "config/HistoryParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signature.hpp"
#include "serialization/V1MerkleTree.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::archive {

struct ArchivalSample {
  std::uint64_t pieceIndex = 0;
  std::vector<unsigned char> piece;
  std::vector<serialization::V1MerkleTree::Digest> siblings;
};

/*
 * A provider's answer to one challenge: the sampled pieces with their
 * inclusion proofs against the segment commitment, signed by the provider.
 * proofId() excludes the signature.
 */
class ArchivalProof {
public:
  static constexpr const char *SCHEMA = "NODO_ARCHIVAL_PROOF_V1";

  ArchivalProof();
  ArchivalProof(std::string challengeId, std::string providerId,
                std::uint64_t segmentIndex, std::uint64_t round,
                std::uint64_t submittedHeight,
                std::vector<ArchivalSample> samples,
                crypto::Signature signature);

  // Prover: reads the sampled pieces from the provider's own block copies.
  // Throws std::runtime_error when the provider no longer holds the data.
  static ArchivalProof build(const ArchivalChallenge &challenge,
                             const ArchiveSegmentIndex &segment,
                             const ArchivedBlockSource &source,
                             std::uint64_t submittedHeight,
                             const crypto::KeyPair &providerKey,
                             std::int64_t signedAt);

  // Re-signs a proof body (used to model providers that sign wrong data).
  static ArchivalProof signedWith(const ArchivalProof &unsignedProof,
                                  const crypto::KeyPair &providerKey,
                                  std::int64_t signedAt);

  const std::string &challengeId() const;
  const std::string &providerId() const;
  std::uint64_t segmentIndex() const;
  std::uint64_t round() const;
  std::uint64_t submittedHeight() const;
  const std::vector<ArchivalSample> &samples() const;
  const crypto::Signature &signature() const;

  std::vector<unsigned char> encodeBody() const;
  std::string proofId() const;
  std::vector<unsigned char> encode() const;
  static ArchivalProof decode(const std::vector<unsigned char> &bytes,
                              std::uint64_t maxBytes);

private:
  std::string m_challengeId;
  std::string m_providerId;
  std::uint64_t m_segmentIndex;
  std::uint64_t m_round;
  std::uint64_t m_submittedHeight;
  std::vector<ArchivalSample> m_samples;
  crypto::Signature m_signature;
};

enum class ArchivalProofStatus {
  ACCEPTED,
  OVERSIZED,
  MALFORMED,
  PROVIDER_UNKNOWN,
  PROVIDER_INACTIVE,
  WRONG_CHALLENGE,
  BAD_SIGNATURE,
  EXPIRED,
  DUPLICATE,
  WRONG_SAMPLES,
  WRONG_DATA
};

std::string archivalProofStatusToString(ArchivalProofStatus status);

struct ArchivalProofVerdict {
  ArchivalProofStatus status = ArchivalProofStatus::MALFORMED;
  std::string reason;
  std::string proofId;

  bool accepted() const { return status == ArchivalProofStatus::ACCEPTED; }
  // The provider signed an answer to its own valid challenge that fails the
  // data checks: verifiable misbehavior, unlike a missing or late answer.
  bool isFraud() const {
    return status == ArchivalProofStatus::WRONG_SAMPLES ||
           status == ArchivalProofStatus::WRONG_DATA;
  }
};

// Remembers which challenges were answered, so a proof is counted once and
// an old round's proof cannot be replayed into a new one.
class ArchivalReplayGuard {
public:
  bool answered(const std::string &challengeId) const;
  void record(const std::string &challengeId, const std::string &proofId);
  std::size_t size() const;

private:
  std::map<std::string, std::string> m_answered;
};

/*
 * Verification costs k piece hashes, k Merkle paths of log2(n) hashes and
 * one Ed25519 check: far cheaper than holding or replaying the segment.
 */
class ArchivalProofVerifier {
public:
  // Registry lookup (provider must be ACTIVE) plus verifyForRegistration.
  // finalizedHeight must come from the finalized block that included the
  // proof, never from the provider or the proof's signed height field.
  static ArchivalProofVerdict
  verify(const config::HistoryParameters &parameters,
         const ArchivalChallenge &challenge,
         const ArchiveSegmentCommitment &segment,
         const ArchiveProviderRegistry &registry, const ArchivalProof &proof,
         const ArchivalReplayGuard &guard, std::uint64_t finalizedHeight);

  // Every check except the provider's current status. The caller supplies
  // the independently authenticated finalized inclusion height.
  static ArchivalProofVerdict
  verifyForRegistration(const config::HistoryParameters &parameters,
                        const ArchivalChallenge &challenge,
                        const ArchiveSegmentCommitment &segment,
                        const ArchiveProviderRegistration &registration,
                        const ArchivalProof &proof,
                        const ArchivalReplayGuard &guard,
                        std::uint64_t finalizedHeight);
};

/*
 * Evidence that a provider signed an invalid answer to a valid challenge.
 * Anyone holding the segment commitment, provider registration and finalized
 * inclusion context can re-verify it. The inclusion height is committed by
 * the evidence id and must equal the independently observed block height;
 * a provider's signed claimed height alone cannot justify a penalty.
 * A missing or late proof is never evidence (it may be censorship).
 */
class ArchivalFaultEvidence {
public:
  ArchivalFaultEvidence();
  ArchivalFaultEvidence(ArchivalChallenge challenge, ArchivalProof proof,
                        std::uint64_t finalizedHeight);

  const ArchivalChallenge &challenge() const;
  const ArchivalProof &proof() const;
  std::uint64_t finalizedHeight() const;
  std::string evidenceId() const;

  bool verify(const config::HistoryParameters &parameters,
              const ArchiveSegmentCommitment &segment,
              const ArchiveProviderRegistry &registry,
              std::uint64_t observedFinalizedHeight) const;

private:
  ArchivalChallenge m_challenge;
  ArchivalProof m_proof;
  std::uint64_t m_finalizedHeight = 0;
};

} // namespace nodo::archive

#endif
