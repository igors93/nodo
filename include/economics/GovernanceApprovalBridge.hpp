#ifndef NODO_ECONOMICS_GOVERNANCE_APPROVAL_BRIDGE_HPP
#define NODO_ECONOMICS_GOVERNANCE_APPROVAL_BRIDGE_HPP

#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernanceLifecycleRecord.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/TreasuryApproval.hpp"

#include <string>

namespace nodo::economics {

enum class GovernanceApprovalBridgeStatus {
    ACCEPTED,
    INVALID_POLICY,
    INVALID_ENVELOPE,
    INVALID_DECISION,
    POLICY_VERSION_MISMATCH,
    PROPOSAL_MISMATCH,
    DECISION_NOT_APPROVED,
    REVIEW_PERIOD_NOT_SATISFIED,
    DECISION_PROOF_REQUIRED,
    INVALID_LIFECYCLE
};

std::string governanceApprovalBridgeStatusToString(GovernanceApprovalBridgeStatus status);

class GovernanceApprovalBridgeResult {
public:
    GovernanceApprovalBridgeResult();

    static GovernanceApprovalBridgeResult accepted(TreasuryApproval approval);
    static GovernanceApprovalBridgeResult rejected(
        GovernanceApprovalBridgeStatus status,
        std::string reason
    );

    bool isAccepted() const;
    GovernanceApprovalBridgeStatus status() const;
    const std::string& reason() const;
    const TreasuryApproval& treasuryApproval() const;

private:
    GovernanceApprovalBridgeStatus m_status;
    std::string m_reason;
    TreasuryApproval m_treasuryApproval;
};

/*
 * GovernanceApprovalBridge is the only authorized path for producing a
 * production TreasuryApproval from verified governance lifecycle evidence.
 *
 * Security principle:
 * No TreasuryApproval may be produced outside the verified-lifecycle path for
 * use in production treasury execution. The production bridge verifies:
 *   - all three inputs are individually valid;
 *   - policy versions are consistent across policy, envelope, and decision;
 *   - the decision references the same governance proposal as the envelope;
 *   - the decision status is APPROVED;
 *   - the review period (decidedAtBlock >= submittedAtBlock + reviewPeriodBlocks)
 *     is satisfied;
 *   - when the policy requires a decision proof, the decision carries one;
 *   - the produced TreasuryApproval carries a deterministic approvalProof.
 *
 * The produced TreasuryApproval is always deterministic: the same inputs
 * produce the same approval, enabling any node to reproduce and verify it.
 */
class GovernanceApprovalBridge {
public:
    static GovernanceApprovalBridgeResult
    produceTreasuryApprovalFromVerifiedLifecycle(
        const GovernanceLifecycleRecord& lifecycle
    );

    // Kept only so old structural bridge behavior can be tested and proven
    // rejected by production validators. Do not call from production code.
    static GovernanceApprovalBridgeResult
    produceTreasuryApprovalFromStructurallyValidDecisionForTestsOnly(
        const GovernancePolicy& policy,
        const GovernanceProposalEnvelope& envelope,
        const GovernanceDecisionRecord& decision
    );
};

} // namespace nodo::economics

#endif
