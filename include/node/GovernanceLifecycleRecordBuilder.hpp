#ifndef NODO_NODE_GOVERNANCE_LIFECYCLE_RECORD_BUILDER_HPP
#define NODO_NODE_GOVERNANCE_LIFECYCLE_RECORD_BUILDER_HPP

#include "economics/GovernanceLifecycleRecord.hpp"
#include "node/GovernanceExecutor.hpp"

#include <cstdint>
#include <optional>

namespace nodo::node {

/*
 * GovernanceLifecycleRecordBuilder restates an already-decided treasury-spend
 * proposal from node::GovernanceExecutor's authoritative state in the
 * economics:: governance lifecycle evidence model.
 *
 * GovernanceExecutor's decision is the single source of truth: it is
 * consensus-critical, deterministic, and fully computed before this builder
 * ever runs. This class never decides anything — it faithfully restates a
 * decision GovernanceExecutor already made so that decision can be verified
 * (economics::GovernanceLifecycleVerifier), turned into a treasury approval
 * (economics::GovernanceApprovalBridge), and audited later.
 *
 * Every node reaching the same GovernanceExecutor state produces a
 * byte-identical lifecycle record: nothing here depends on wall-clock time,
 * randomness, or local configuration beyond the proposal snapshot itself.
 *
 * economics::GovernanceProposalEnvelope only represents treasury spends (it
 * wraps a TreasuryProposal directly), so only TREASURY_SPEND proposals get a
 * lifecycle record. TEXT and PARAMETER_CHANGE proposals are unaffected by
 * this ticket and remain fully described by GovernanceExecutor's own state.
 */
class GovernanceLifecycleRecordBuilder {
public:
  // Builds a lifecycle record for a decided treasury-spend proposal, ending
  // at DECIDED_APPROVED or DECIDED_REJECTED. Returns std::nullopt if the
  // snapshot is not a treasury-spend proposal that has reached a decision
  // (APPROVED-lineage or REJECTED) — in particular, proposals that expired
  // for lack of quorum are not represented, since economics::
  // GovernanceDecisionBuilder can only express APPROVED/REJECTED outcomes.
  static std::optional<economics::GovernanceLifecycleRecord>
  buildDecided(const GovernanceExecutor::GovernanceProposalSnapshot &snapshot);

  // Extends an already-DECIDED_APPROVED lifecycle record with the
  // APPROVAL_PRODUCED -> EXECUTED transitions, once the spend has actually
  // executed.
  static economics::GovernanceLifecycleRecord
  buildExecuted(const economics::GovernanceLifecycleRecord &decided,
                std::uint64_t executedAtHeight);
};

} // namespace nodo::node

#endif
