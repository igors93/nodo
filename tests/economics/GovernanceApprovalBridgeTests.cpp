#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/TreasuryApprovalProof.hpp"
#include "economics/TreasuryProposal.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernanceApprovalBridge;
using nodo::economics::GovernanceApprovalBridgeStatus;
using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernancePolicy;
using nodo::economics::GovernanceProposalEnvelope;
using nodo::economics::TreasuryApprovalProof;
using nodo::economics::TreasuryProposal;
using nodo::utils::Amount;

GovernancePolicy validPolicy() {
    return GovernancePolicy("governance-v1", 10, 5, true, false);
}

TreasuryProposal validTreasuryProposal() {
    return TreasuryProposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

GovernanceProposalEnvelope validEnvelope(
    std::uint64_t submittedAtBlock = 5
) {
    return GovernanceProposalEnvelope(
        "gov-prop-001",
        "TREASURY_SPEND",
        validTreasuryProposal(),
        submittedAtBlock,
        "submitter-node",
        "governance-v1",
        "hash-abc123"
    );
}

GovernanceDecisionRecord validApprovedDecision(
    std::uint64_t decidedAtBlock = 20
) {
    // decidedAtBlock=20 >= submittedAtBlock=5 + reviewPeriod=10 = 15 → satisfied
    return GovernanceDecisionRecord(
        "decision-001",
        "gov-prop-001",
        "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        decidedAtBlock,
        "governance-node",
        "decision-proof-xyz",
        "governance-v1"
    );
}

// ---- Positive tests ----

void testValidBridgeProducesTreasuryApproval() {
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision()
    );
    assert(result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::ACCEPTED);
    assert(result.reason().empty());

    const auto& approval = result.treasuryApproval();
    assert(approval.isValid());
    assert(approval.proposalId() == "prop-001");
    assert(approval.approvedAtBlock() == 20);
    assert(approval.approver() == "governance-node");
    assert(!approval.approvalProof().empty());
    assert(approval.approvalId() == "gov-approval:decision-001");
}

void testApprovalProofIsDeterministic() {
    const auto r1 = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision()
    );
    const auto r2 = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision()
    );
    assert(r1.isAccepted());
    assert(r2.isAccepted());
    assert(r1.treasuryApproval().approvalProof() == r2.treasuryApproval().approvalProof());
    assert(r1.treasuryApproval().approvalId() == r2.treasuryApproval().approvalId());
}

void testApprovalProofMatchesTreasuryApprovalProof() {
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision()
    );
    assert(result.isAccepted());

    const std::string expected = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    assert(result.treasuryApproval().approvalProof() == expected);
}

// ---- Rejection tests ----

void testRejectedDecisionCannotProduceApproval() {
    const GovernanceDecisionRecord rejected(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::REJECTED,
        20, "governance-node", "decision-proof-xyz", "governance-v1"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), rejected
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::DECISION_NOT_APPROVED);
}

void testExpiredDecisionCannotProduceApproval() {
    const GovernanceDecisionRecord expired(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::EXPIRED,
        20, "governance-node", "", "governance-v1"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), expired
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::DECISION_NOT_APPROVED);
}

void testPolicyVersionMismatchEnvelopeRejected() {
    const GovernanceProposalEnvelope wrongVersionEnvelope(
        "gov-prop-001", "TREASURY_SPEND", validTreasuryProposal(),
        5, "submitter-node", "governance-v2", "hash-abc123"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), wrongVersionEnvelope, validApprovedDecision()
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH);
}

void testPolicyVersionMismatchDecisionRejected() {
    const GovernanceDecisionRecord wrongVersionDecision(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "decision-proof-xyz", "governance-v2"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), wrongVersionDecision
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH);
}

void testProposalIdMismatchRejected() {
    const GovernanceDecisionRecord wrongProposalDecision(
        "decision-001", "gov-prop-999", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "decision-proof-xyz", "governance-v1"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), wrongProposalDecision
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::PROPOSAL_MISMATCH);
}

void testReviewPeriodNotSatisfiedRejected() {
    // submittedAtBlock=5, reviewPeriod=10, so decidedAtBlock must be >= 15.
    // decidedAtBlock=14 should be rejected.
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision(14)
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::REVIEW_PERIOD_NOT_SATISFIED);
}

void testExactlyAtReviewPeriodBoundaryAccepted() {
    // submittedAtBlock=5 + reviewPeriod=10 = 15, decidedAtBlock=15 → exactly satisfied
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), validApprovedDecision(15)
    );
    assert(result.isAccepted());
}

void testDecisionProofRequiredWhenPolicyRequires() {
    const GovernanceDecisionRecord noProofDecision(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "",  // empty decisionProof
        "governance-v1"
    );
    // policy.requireDecisionProof = true (validPolicy())
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), noProofDecision
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::DECISION_PROOF_REQUIRED);
}

void testDecisionProofNotRequiredWhenPolicyDoesNot() {
    const GovernancePolicy noProofPolicy("governance-v1", 10, 5, false, false);
    const GovernanceDecisionRecord noProofDecision(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "", "governance-v1"
    );
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        noProofPolicy, validEnvelope(), noProofDecision
    );
    assert(result.isAccepted());
}

void testInvalidPolicyRejected() {
    const GovernancePolicy invalid;  // default-constructed = invalid
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        invalid, validEnvelope(), validApprovedDecision()
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::INVALID_POLICY);
}

void testInvalidEnvelopeRejected() {
    const GovernanceProposalEnvelope invalid;  // default-constructed = invalid
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), invalid, validApprovedDecision()
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::INVALID_ENVELOPE);
}

void testInvalidDecisionRejected() {
    const GovernanceDecisionRecord invalid;  // default-constructed = invalid
    const auto result = GovernanceApprovalBridge::produceTreasuryApproval(
        validPolicy(), validEnvelope(), invalid
    );
    assert(!result.isAccepted());
    assert(result.status() == GovernanceApprovalBridgeStatus::INVALID_DECISION);
}

void testStatusToString() {
    using nodo::economics::governanceApprovalBridgeStatusToString;
    assert(governanceApprovalBridgeStatusToString(
               GovernanceApprovalBridgeStatus::ACCEPTED) == "ACCEPTED");
    assert(governanceApprovalBridgeStatusToString(
               GovernanceApprovalBridgeStatus::POLICY_VERSION_MISMATCH) ==
           "POLICY_VERSION_MISMATCH");
    assert(governanceApprovalBridgeStatusToString(
               GovernanceApprovalBridgeStatus::REVIEW_PERIOD_NOT_SATISFIED) ==
           "REVIEW_PERIOD_NOT_SATISFIED");
}

} // namespace

int main() {
    testValidBridgeProducesTreasuryApproval();
    testApprovalProofIsDeterministic();
    testApprovalProofMatchesTreasuryApprovalProof();
    testRejectedDecisionCannotProduceApproval();
    testExpiredDecisionCannotProduceApproval();
    testPolicyVersionMismatchEnvelopeRejected();
    testPolicyVersionMismatchDecisionRejected();
    testProposalIdMismatchRejected();
    testReviewPeriodNotSatisfiedRejected();
    testExactlyAtReviewPeriodBoundaryAccepted();
    testDecisionProofRequiredWhenPolicyRequires();
    testDecisionProofNotRequiredWhenPolicyDoesNot();
    testInvalidPolicyRejected();
    testInvalidEnvelopeRejected();
    testInvalidDecisionRejected();
    testStatusToString();
    return 0;
}
