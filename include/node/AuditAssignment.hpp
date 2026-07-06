#ifndef NODO_NODE_AUDIT_ASSIGNMENT_HPP
#define NODO_NODE_AUDIT_ASSIGNMENT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * AuditAssignmentTargetType identifies what kind of entity is being assigned
 * for audit.
 */
enum class AuditAssignmentTargetType {
  BLOCK_ARTIFACT,
  TREASURY_SECTION,
  GOVERNANCE_LIFECYCLE,
  VALIDATOR_STATE
};

std::string auditAssignmentTargetTypeToString(AuditAssignmentTargetType type);

/*
 * AuditAssignment is a deterministic, verifiable record of which validator
 * is assigned to audit a specific target at a given block height.
 *
 * Security principle:
 * Assignments must be reproducible from public data (seed = previous block
 * hash). No randomness from the current block, clock, or local state is used.
 * The assignmentProof binds all fields so that any node can verify the
 * assignment without trusting the assigning party.
 */
class AuditAssignment {
public:
  AuditAssignment();

  AuditAssignment(std::string assignmentId, std::uint64_t blockHeight,
                  std::uint64_t epoch, AuditAssignmentTargetType targetType,
                  std::string targetId, std::string validatorAddress,
                  std::string sourceSeedDigest, std::string assignmentProof);

  const std::string &assignmentId() const;
  std::uint64_t blockHeight() const;
  std::uint64_t epoch() const;
  AuditAssignmentTargetType targetType() const;
  const std::string &targetId() const;
  const std::string &validatorAddress() const;
  const std::string &sourceSeedDigest() const;
  const std::string &assignmentProof() const;

  bool isValid() const;
  const std::string &rejectionReason() const;

  std::string serialize() const;

private:
  std::string m_assignmentId;
  std::uint64_t m_blockHeight;
  std::uint64_t m_epoch;
  AuditAssignmentTargetType m_targetType;
  std::string m_targetId;
  std::string m_validatorAddress;
  std::string m_sourceSeedDigest;
  std::string m_assignmentProof;
  bool m_valid;
  std::string m_rejectionReason;
};

/*
 * AuditAssignmentCalculator derives deterministic audit assignments from
 * a seed (typically the previous finalized block hash).
 *
 * Security principle:
 * The seed must come from already-finalized data. Using the current block's
 * hash or local state would allow manipulation. The proof format is:
 * audit-assignment:<seedDigest>:<blockHeight>:<epoch>:<targetType>:<targetId>:<validatorAddress>
 */
class AuditAssignmentCalculator {
public:
  static std::string buildAssignmentProof(const std::string &sourceSeedDigest,
                                          std::uint64_t blockHeight,
                                          std::uint64_t epoch,
                                          AuditAssignmentTargetType targetType,
                                          const std::string &targetId,
                                          const std::string &validatorAddress);

  // Select one validator from the list deterministically based on the seed.
  // Returns empty string if validatorAddresses is empty.
  static std::string
  selectValidator(const std::string &sourceSeedDigest,
                  const std::string &targetId,
                  const std::vector<std::string> &validatorAddresses);

  static AuditAssignment
  buildAssignment(const std::string &assignmentId, std::uint64_t blockHeight,
                  std::uint64_t epoch, AuditAssignmentTargetType targetType,
                  const std::string &targetId,
                  const std::string &sourceSeedDigest,
                  const std::vector<std::string> &validatorAddresses);
};

} // namespace nodo::node

#endif
