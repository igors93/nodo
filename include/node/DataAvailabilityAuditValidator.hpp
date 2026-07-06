#ifndef NODO_NODE_DATA_AVAILABILITY_AUDIT_VALIDATOR_HPP
#define NODO_NODE_DATA_AVAILABILITY_AUDIT_VALIDATOR_HPP

#include "node/DataAvailabilityEvidence.hpp"
#include "node/FinalizedBlockArtifact.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class DataAvailabilityAuditStatus {
  PASSED,
  RESPONSE_DIGEST_MISMATCH,
  CHALLENGE_MISSING,
  RESPONSE_CHALLENGE_MISMATCH,
  FAILURE_EVIDENCE_MISSING_CHALLENGE,
  ARTIFACT_DIGEST_MISMATCH
};

std::string
dataAvailabilityAuditStatusToString(DataAvailabilityAuditStatus status);

class DataAvailabilityAuditResult {
public:
  DataAvailabilityAuditResult();

  static DataAvailabilityAuditResult passed();
  static DataAvailabilityAuditResult failed(DataAvailabilityAuditStatus status,
                                            std::string reason);

  bool isPassed() const;
  DataAvailabilityAuditStatus status() const;
  const std::string &reason() const;

private:
  bool m_passed;
  DataAvailabilityAuditStatus m_status;
  std::string m_reason;
};

/*
 * DataAvailabilityAuditValidator validates availability evidence records
 * for correctness and internal consistency.
 *
 * Security principle:
 * A DataAvailabilityResponse must reference the exact challenge and carry
 * the same artifactDigest as the challenge. A DataAvailabilityFailureEvidence
 * must reference a valid challenge. Artifact digests that cannot be
 * reconstructed from the artifact content must be rejected.
 */
class DataAvailabilityAuditValidator {
public:
  // Validate that a response matches its challenge.
  static DataAvailabilityAuditResult
  validateResponse(const DataAvailabilityChallenge &challenge,
                   const DataAvailabilityResponse &response);

  // Validate that failure evidence references a valid challenge.
  static DataAvailabilityAuditResult
  validateFailureEvidence(const DataAvailabilityChallenge &challenge,
                          const DataAvailabilityFailureEvidence &evidence);

  // Validate that an artifact's recomputed digest matches the given expected
  // digest. Used during reload to detect corrupted artifacts.
  static DataAvailabilityAuditResult
  validateArtifactDigest(const FinalizedBlockArtifact &artifact,
                         const std::string &expectedDigest);
};

} // namespace nodo::node

#endif
