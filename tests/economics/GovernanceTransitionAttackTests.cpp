#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceLifecycleRecord.hpp"
#include "economics/GovernanceLifecycleState.hpp"
#include "economics/GovernanceLifecycleTransition.hpp"
#include "economics/GovernanceLifecycleTransitionAudit.hpp"
#include "economics/GovernanceLifecycleVerifier.hpp"
#include "economics/GovernanceTransitionProof.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::GovernanceApprovalBridge;
using nodo::economics::GovernanceApprovalBridgeStatus;
using nodo::economics::GovernanceLifecycleRecord;
using nodo::economics::GovernanceLifecycleState;
using nodo::economics::GovernanceLifecycleTransition;
using nodo::economics::GovernanceLifecycleTransitionAudit;
using nodo::economics::GovernanceLifecycleVerificationStatus;
using nodo::economics::GovernanceLifecycleVerifier;
using nodo::economics::GovernanceTransitionProof;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::validTransitionHistory;

const std::string kProposalId    = "gov-prop-001";
const std::string kPolicyVersion = "governance-v1";

GovernanceLifecycleTransition makeT(
    const std::string& id,
    GovernanceLifecycleState from,
    GovernanceLifecycleState to,
    std::uint64_t block,
    const std::string& actor = "governance-node",
    const std::string& reason = ""
) {
    const std::string proof = GovernanceTransitionProof::build(
        kProposalId, id, from, to, block, actor, kPolicyVersion
    );
    return GovernanceLifecycleTransition(
        id, kProposalId, from, to, block, actor, reason, proof, kPolicyVersion
    );
}

// Attack: skip REVIEW — transition directly from SUBMITTED to VOTING.
void testSkipReviewRejected() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::VOTING,    6),   // skip REVIEW
        makeT("t-003", S::VOTING,    S::TALLYING,  10),
        makeT("t-004", S::TALLYING,  S::DECIDED_APPROVED, 20),
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

// Attack: skip VOTING — transition directly from REVIEW to TALLYING.
void testSkipVotingRejected() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::REVIEW,    6),
        makeT("t-003", S::REVIEW,    S::TALLYING,  10),  // skip VOTING
        makeT("t-004", S::TALLYING,  S::DECIDED_APPROVED, 20),
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

// Attack: skip from SUBMITTED directly to DECIDED_APPROVED.
void testSubmittedToApprovedDirectlyRejected() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,      S::SUBMITTED,        5),
        makeT("t-002", S::SUBMITTED,  S::DECIDED_APPROVED, 6),  // impossible jump
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

// Attack: produce approval from DECIDED_REJECTED lifecycle.
void testApprovalFromDecidedRejectedLifecycleRejected() {
    using S = GovernanceLifecycleState;
    const auto base = validLifecycle();

    // Build a history ending at DECIDED_REJECTED.
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED,        5),
        makeT("t-002", S::SUBMITTED, S::REVIEW,           6),
        makeT("t-003", S::REVIEW,    S::VOTING,           10),
        makeT("t-004", S::VOTING,    S::TALLYING,         18),
        makeT("t-005", S::TALLYING,  S::DECIDED_REJECTED, 20),
    };

    // Lifecycle with DECIDED_REJECTED state is structurally invalid because
    // the decision record status is APPROVED but state says REJECTED.
    // So build with rejected state should fail at record construction.
    const GovernanceLifecycleRecord lifecycle(
        "lc-rejected",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        S::DECIDED_REJECTED,
        history
    );
    // Either: lifecycle invalid (state mismatch with votes+decision) or bridge rejects.
    if (lifecycle.isValid()) {
        const auto bridgeResult =
            GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle(lifecycle);
        assert(!bridgeResult.isAccepted());
    } else {
        // Lifecycle construction itself rejected the invalid combination.
        assert(!lifecycle.isValid());
    }
}

// Attack: produce approval from CANCELLED lifecycle.
void testApprovalFromCancelledLifecycleRejected() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::CANCELLED, 6, "governance-node", "duplicate proposal"),
    };
    // A CANCELLED lifecycle cannot hold votes/tally/decision, so it can't be
    // constructed as a valid GovernanceLifecycleRecord. Verify audit rejects directly.
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(result.accepted());  // audit itself accepts (structurally valid)
    assert(result.finalState() == S::CANCELLED);
    // The bridge path: if someone tried to construct a lifecycle with CANCELLED state
    // but valid votes, the lifecycle constructor would reject it (CANCELLED is not a decided state).
    const auto base = validLifecycle();
    const GovernanceLifecycleRecord cancelledLifecycle(
        "lc-cancelled",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        S::CANCELLED,
        history
    );
    assert(!cancelledLifecycle.isValid());
}

// Attack: produce approval from EXPIRED lifecycle.
void testApprovalFromExpiredLifecycleRejected() {
    using S = GovernanceLifecycleState;
    const auto base = validLifecycle();
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::EXPIRED,   8, "governance-node", "timeout exceeded"),
    };
    const GovernanceLifecycleRecord expiredLifecycle(
        "lc-expired",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        S::EXPIRED,
        history
    );
    assert(!expiredLifecycle.isValid());
}

// Attack: duplicate transitionId in history.
void testDuplicateTransitionIdRejected() {
    using S = GovernanceLifecycleState;
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-001", S::SUBMITTED, S::REVIEW,    6),  // same id as first
        makeT("t-002", S::REVIEW,    S::VOTING,    10),
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

// Attack: forge a transition proof from another proposal.
void testTransitionReusedForAnotherProposalRejected() {
    using S = GovernanceLifecycleState;
    const std::string otherProposalId = "gov-prop-999";
    // Proof built for gov-prop-999 but transition claims gov-prop-001.
    const std::string proof = GovernanceTransitionProof::build(
        otherProposalId, "t-002", S::SUBMITTED, S::REVIEW, 6, "actor", kPolicyVersion
    );
    const GovernanceLifecycleTransition bad(
        "t-002", kProposalId, S::SUBMITTED, S::REVIEW, 6, "actor", "", proof, kPolicyVersion
    );
    assert(!bad.isValid());  // proof doesn't match kProposalId
}

// Attack: state changed after persisted lifecycle (tampered declaredCurrentState).
void testTamperedDeclaredStateRejected() {
    using S = GovernanceLifecycleState;
    const auto base = validLifecycle();
    // History ends at DECIDED_APPROVED but we claim APPROVAL_PRODUCED.
    const GovernanceLifecycleRecord lifecycle(
        "lc-tampered-state",
        base.proposalEnvelope(),
        base.governancePolicy(),
        base.votingPolicy(),
        base.voteEvidenceList(),
        base.tallyReport(),
        base.decisionRecord(),
        base.createdAtBlock(),
        base.finalizedAtBlock(),
        S::APPROVAL_PRODUCED,    // wrong: transitions end at DECIDED_APPROVED
        base.transitionHistory()
    );
    assert(!lifecycle.isValid());
}

// Attack: forged transitionProof with correct fields but wrong proof string.
void testForgedTransitionProofRejectedByAudit() {
    using S = GovernanceLifecycleState;
    const GovernanceLifecycleTransition bad(
        "t-002", kProposalId, S::SUBMITTED, S::REVIEW,
        6, "governance-node", "", "forged-proof", kPolicyVersion
    );
    assert(!bad.isValid());

    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT, S::SUBMITTED, 5),
        bad,
    };
    const auto result = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!result.accepted());
}

// Correct lifecycle verifies and produces approval.
void testValidApprovedLifecycleProducesApproval() {
    const auto lifecycle = validLifecycle();
    const auto verifyResult = GovernanceLifecycleVerifier::verify(lifecycle);
    assert(verifyResult.verified());

    const auto bridgeResult =
        GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle(lifecycle);
    assert(bridgeResult.isAccepted());
}

// Lifecycle whose transition history skips VOTING cannot verify.
void testLifecycleSkippingVotingFailsVerifier() {
    using S = GovernanceLifecycleState;
    const auto base = validLifecycle();

    // Construct a history that jumps SUBMITTED -> TALLYING (bypassing REVIEW+VOTING).
    // This will be rejected by the state machine (SUBMITTED -> TALLYING not allowed).
    std::vector<GovernanceLifecycleTransition> history = {
        makeT("t-001", S::DRAFT,     S::SUBMITTED, 5),
        makeT("t-002", S::SUBMITTED, S::TALLYING,  10),  // illegal
    };
    const auto auditResult = GovernanceLifecycleTransitionAudit::audit(
        history, kProposalId, kPolicyVersion
    );
    assert(!auditResult.accepted());
}

} // namespace

int main() {
    testSkipReviewRejected();
    testSkipVotingRejected();
    testSubmittedToApprovedDirectlyRejected();
    testApprovalFromDecidedRejectedLifecycleRejected();
    testApprovalFromCancelledLifecycleRejected();
    testApprovalFromExpiredLifecycleRejected();
    testDuplicateTransitionIdRejected();
    testTransitionReusedForAnotherProposalRejected();
    testTamperedDeclaredStateRejected();
    testForgedTransitionProofRejectedByAudit();
    testValidApprovedLifecycleProducesApproval();
    testLifecycleSkippingVotingFailsVerifier();
    return 0;
}
