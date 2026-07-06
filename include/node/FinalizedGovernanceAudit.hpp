#ifndef NODO_NODE_FINALIZED_GOVERNANCE_AUDIT_HPP
#define NODO_NODE_FINALIZED_GOVERNANCE_AUDIT_HPP

#include "node/FinalizedBlockArtifact.hpp"
#include "node/GovernanceExecutor.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * FinalizedGovernanceAudit is ChainAuditor's governance-specific replay gate.
 *
 * It deliberately sits outside GovernanceExecutor: the executor is the live
 * state machine, while this class is a read-only auditor that re-states the
 * final governance state into independently-verifiable lifecycle evidence.
 *
 * Security boundary:
 * - finalized artifacts must carry structurally valid governance policy,
 *   guard, and summary records when those sections are active;
 * - the latest artifact's GovernanceSummary must match the rebuilt runtime
 *   governance state at the audited tip;
 * - every decided treasury proposal that the runtime still knows about must
 *   rebuild into a GovernanceLifecycleRecord accepted by
 *   economics::GovernanceLifecycleVerifier;
 * - every embedded treasury execution evidence must carry a verified
 *   governance lifecycle context, so a spend record is never accepted as proof
 *   of authorization by itself.
 */
enum class FinalizedGovernanceAuditStatus {
  ACCEPTED,
  INVALID_ARTIFACT_GOVERNANCE_POLICY,
  INVALID_ARTIFACT_GOVERNANCE_GUARD,
  INVALID_ARTIFACT_GOVERNANCE_SUMMARY,
  GOVERNANCE_SUMMARY_MISMATCH,
  GOVERNANCE_LIFECYCLE_BUILD_FAILED,
  GOVERNANCE_LIFECYCLE_VERIFICATION_FAILED,
  DUPLICATE_GOVERNANCE_LIFECYCLE,
  TREASURY_EXECUTION_EVIDENCE_MISSING_CONTEXT,
  TREASURY_EXECUTION_LIFECYCLE_MISMATCH,
  TREASURY_EXECUTION_EVIDENCE_MISSING_FOR_EXECUTED_PROPOSAL
};

std::string
finalizedGovernanceAuditStatusToString(FinalizedGovernanceAuditStatus status);

class FinalizedGovernanceAuditResult {
public:
  FinalizedGovernanceAuditResult();

  static FinalizedGovernanceAuditResult accepted(std::size_t artifactCount,
                                                 std::size_t lifecycleCount,
                                                 std::size_t evidenceCount);

  static FinalizedGovernanceAuditResult
  rejected(FinalizedGovernanceAuditStatus status, std::string reason,
           std::uint64_t failedBlockHeight = 0, std::string proposalId = "",
           std::size_t lifecycleCount = 0, std::size_t evidenceCount = 0);

  bool accepted() const;
  FinalizedGovernanceAuditStatus status() const;
  const std::string &reason() const;
  std::uint64_t failedBlockHeight() const;
  const std::string &proposalId() const;
  std::size_t artifactCount() const;
  std::size_t lifecycleCount() const;
  std::size_t evidenceCount() const;

  std::string serialize() const;

private:
  bool m_accepted;
  FinalizedGovernanceAuditStatus m_status;
  std::string m_reason;
  std::uint64_t m_failedBlockHeight;
  std::string m_proposalId;
  std::size_t m_artifactCount;
  std::size_t m_lifecycleCount;
  std::size_t m_evidenceCount;
};

class FinalizedGovernanceAudit {
public:
  static FinalizedGovernanceAuditResult
  auditRuntime(const GovernanceExecutor &governance,
               const std::vector<FinalizedBlockArtifact> &artifacts,
               std::uint64_t auditedTipHeight);
};

} // namespace nodo::node

#endif
