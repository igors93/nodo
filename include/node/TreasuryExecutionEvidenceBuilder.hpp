#ifndef NODO_NODE_TREASURY_EXECUTION_EVIDENCE_BUILDER_HPP
#define NODO_NODE_TREASURY_EXECUTION_EVIDENCE_BUILDER_HPP

#include "config/NetworkParameters.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "node/GovernanceExecutor.hpp"

#include <map>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * TreasuryExecutionEvidenceBuilder detects treasury-spend proposals that
 * became EXECUTED while a block was being applied and deterministically
 * rebuilds the economics::TreasuryExecutionEvidence for each one, so it can
 * be attached to that block's finalized artifact treasury section.
 *
 * GovernanceExecutor::executeQueuedTreasuryProposal is the sole authority
 * that actually validates and moves treasury funds; this builder never
 * decides anything, it only restates that already-applied outcome in the
 * economics:: evidence model by re-running the same deterministic checks
 * (GovernanceLifecycleRecordBuilder, GovernanceApprovalBridge,
 * TreasurySpendValidator) against the proposal's final snapshot. Every node
 * reaching the same GovernanceExecutor state produces byte-identical
 * evidence.
 */
class TreasuryExecutionEvidenceBuilder {
public:
  // Snapshots every proposal's status before a block is applied, so
  // newly-executed treasury spends can be detected afterward by diffing
  // against GovernanceExecutor's post-block state.
  static std::map<std::string, GovernanceProposalStatus>
  snapshotStatuses(const GovernanceExecutor &governance);

  // Returns evidence for every treasury-spend proposal that transitioned to
  // EXECUTED while this block was applied. Proposals unrelated to treasury
  // spends, or unchanged this block, are skipped. A proposal that executed
  // this block but can no longer be independently verified (which would
  // indicate a bug in this reconstruction, since the same checks already
  // passed during execution) is skipped rather than aborting finalization,
  // since its absence here is a pure audit-trail gap and does not affect
  // consensus-critical state.
  static std::vector<economics::TreasuryExecutionEvidence>
  buildForNewlyExecuted(
      const std::map<std::string, GovernanceProposalStatus> &statusesBefore,
      const GovernanceExecutor &governanceAfter,
      const config::NetworkParameters &networkParameters);
};

} // namespace nodo::node

#endif
