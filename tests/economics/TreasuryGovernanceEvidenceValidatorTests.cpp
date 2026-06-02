#include "economics/TreasuryGovernanceEvidenceValidator.hpp"
#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceDecisionRecord.hpp"
#include "economics/GovernancePolicy.hpp"
#include "economics/GovernanceProposalEnvelope.hpp"
#include "economics/TreasuryApprovalProof.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::GovernanceApprovalBridge;
using nodo::economics::GovernanceApprovalContext;
using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernanceEvidenceValidationStatus;
using nodo::economics::GovernancePolicy;
using nodo::economics::GovernanceProposalEnvelope;
using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryApprovalProof;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasuryGovernanceEvidenceValidator;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::utils::Amount;

// ---- Shared fixture helpers ----

TreasuryAccount validTreasury(Amount balance = Amount::fromRawUnits(1000000)) {
    return TreasuryAccount("treasury-main", "nodo-treasury-addr", balance, 0, false, "");
}

TreasuryPolicy validSpendPolicy() {
    return TreasuryPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000), Amount::fromRawUnits(100000),
        5, true, false
    );
}

TreasuryProposal validTreasuryProposal(
    const std::string& id = "prop-001"
) {
    return TreasuryProposal(
        id, "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

GovernancePolicy validGovPolicy() {
    return GovernancePolicy("governance-v1", 10, 5, true, false);
}

GovernanceProposalEnvelope validEnvelope(
    const std::string& proposalId = "prop-001"
) {
    return GovernanceProposalEnvelope(
        "gov-prop-001", "TREASURY_SPEND",
        validTreasuryProposal(proposalId),
        5, "submitter-node", "governance-v1", "hash-abc123"
    );
}

GovernanceDecisionRecord validDecision() {
    // decidedAtBlock=20 >= submittedAtBlock=5 + reviewPeriod=10 = 15 → satisfied
    return GovernanceDecisionRecord(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20, "governance-node", "decision-proof-xyz", "governance-v1"
    );
}

// Build a full governance-backed evidence using the canonical bridge path.
TreasuryExecutionEvidence buildGovernanceBackedEvidence(
    const std::string& evidenceId = "ev-001",
    const std::string& proposalId = "prop-001"
) {
    const auto proposal = validTreasuryProposal(proposalId);
    const auto spendPolicy = validSpendPolicy();
    const auto treasury = validTreasury();
    const auto govPolicy = validGovPolicy();

    const GovernanceProposalEnvelope envelope(
        "gov-prop-001", "TREASURY_SPEND",
        proposal, 5, "submitter-node", "governance-v1", "hash-abc123"
    );
    const auto decision = validDecision();

    // Run bridge to get the official approval.
    const auto bridgeResult = GovernanceApprovalBridge::produceTreasuryApproval(
        govPolicy, envelope, decision
    );
    assert(bridgeResult.isAccepted());
    const TreasuryApproval& approval = bridgeResult.treasuryApproval();

    // Run spend validator to get the matching spend record.
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        treasury, spendPolicy, proposal, approval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext ctx;
    ctx.governancePolicy = govPolicy;
    ctx.governanceProposalEnvelope = envelope;
    ctx.governanceDecisionRecord = decision;

    return TreasuryExecutionEvidence(
        evidenceId,
        proposal, approval, spendPolicy,
        treasury, 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(ctx)
    );
}

// ---- Tests ----

void testEvidenceWithBridgeApprovalAccepted() {
    const auto ev = buildGovernanceBackedEvidence();
    assert(ev.hasGovernanceContext());

    const auto result = TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);
    assert(result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testEvidenceWithoutGovernanceContextRejected() {
    // Build evidence using the legacy constructor (no governance context).
    const auto proposal = validTreasuryProposal();
    const TreasuryApproval directApproval(
        "appr-001", "prop-001", 3, "governance-node", "manual-proof"
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validSpendPolicy(), proposal, directApproval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    const TreasuryExecutionEvidence ev(
        "ev-legacy",
        proposal, directApproval, validSpendPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001
        // no governance context
    );

    const auto result = TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT);
}

void testEvidenceWithForgedApprovalProofRejected() {
    const auto proposal = validTreasuryProposal();
    const auto spendPolicy = validSpendPolicy();
    const auto treasury = validTreasury();
    const auto govPolicy = validGovPolicy();
    const auto envelope = validEnvelope();
    const auto decision = validDecision();

    // Build the correct bridge approval, then tamper the proof.
    const auto bridgeResult = GovernanceApprovalBridge::produceTreasuryApproval(
        govPolicy, envelope, decision
    );
    assert(bridgeResult.isAccepted());

    const TreasuryApproval forgedApproval(
        bridgeResult.treasuryApproval().approvalId(),
        bridgeResult.treasuryApproval().proposalId(),
        bridgeResult.treasuryApproval().approvedAtBlock(),
        bridgeResult.treasuryApproval().approver(),
        "forged-proof-tampered"  // tampered proof
    );

    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        treasury, spendPolicy, proposal, forgedApproval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext ctx;
    ctx.governancePolicy = govPolicy;
    ctx.governanceProposalEnvelope = envelope;
    ctx.governanceDecisionRecord = decision;

    const TreasuryExecutionEvidence ev(
        "ev-forged",
        proposal, forgedApproval, spendPolicy,
        treasury, 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(ctx)
    );

    const auto result = TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH);
}

void testEvidenceWithRejectedDecisionRejected() {
    const auto proposal = validTreasuryProposal();
    const auto spendPolicy = validSpendPolicy();
    const auto treasury = validTreasury();
    const auto govPolicy = validGovPolicy();
    const auto envelope = validEnvelope();

    const GovernanceDecisionRecord rejectedDecision(
        "decision-001", "gov-prop-001", "TREASURY_SPEND",
        GovernanceDecisionStatus::REJECTED,
        20, "governance-node", "decision-proof-xyz", "governance-v1"
    );

    // We must still build a technically-valid approval for the spend validator.
    // We use a direct approval (forged from bridge inputs but with rejected decision proof).
    const std::string approvalProof = TreasuryApprovalProof::build(
        "gov-prop-001", "prop-001", "decision-001", "governance-v1", 20
    );
    const TreasuryApproval approval(
        "gov-approval:decision-001", "prop-001", 20, "governance-node", approvalProof
    );

    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        treasury, spendPolicy, proposal, approval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext ctx;
    ctx.governancePolicy = govPolicy;
    ctx.governanceProposalEnvelope = envelope;
    ctx.governanceDecisionRecord = rejectedDecision;

    const TreasuryExecutionEvidence ev(
        "ev-rejected-dec",
        proposal, approval, spendPolicy,
        treasury, 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(ctx)
    );

    const auto result = TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::DECISION_NOT_APPROVED);
}

void testEvidenceWithDifferentProposalRejected() {
    // The governance context references a different treasury proposal than the evidence.
    const auto proposal = validTreasuryProposal("prop-001");
    const auto spendPolicy = validSpendPolicy();
    const auto treasury = validTreasury();
    const auto govPolicy = validGovPolicy();

    // Envelope references "prop-999" but the evidence carries "prop-001".
    const GovernanceProposalEnvelope wrongEnvelope(
        "gov-prop-001", "TREASURY_SPEND",
        validTreasuryProposal("prop-999"),  // different proposalId
        5, "submitter-node", "governance-v1", "hash-abc123"
    );
    const auto decision = validDecision();

    const auto bridgeResult = GovernanceApprovalBridge::produceTreasuryApproval(
        govPolicy, wrongEnvelope, decision
    );
    // Bridge produces approval for prop-999.
    assert(bridgeResult.isAccepted());
    assert(bridgeResult.treasuryApproval().proposalId() == "prop-999");

    // Build a "prop-001" approval manually that matches the bridge proof structure
    // but with the wrong proposalId — this won't match what the validator expects.
    // Actually: the bridge returned approval for prop-999. If we use it with prop-001
    // evidence, the evidence structural validator will reject it (proposalId mismatch).
    // Use a valid approval for prop-001 but with governance context referencing prop-999.
    const auto& bridgeApproval = bridgeResult.treasuryApproval();
    // We need a spend-validator-accepted approval for prop-001.
    const TreasuryApproval proposalOneApproval(
        "gov-approval:decision-001", "prop-001", 20, "governance-node",
        bridgeApproval.approvalProof()  // wrong proof (built for prop-999)
    );

    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        treasury, spendPolicy, proposal, proposalOneApproval,
        10, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext ctx;
    ctx.governancePolicy = govPolicy;
    ctx.governanceProposalEnvelope = wrongEnvelope;
    ctx.governanceDecisionRecord = decision;

    const TreasuryExecutionEvidence ev(
        "ev-mismatch",
        proposal, proposalOneApproval, spendPolicy,
        treasury, 10, Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(ctx)
    );

    const auto result = TreasuryGovernanceEvidenceValidator::validateGovernanceContext(ev);
    assert(!result.isAccepted());
    // The proof computed by bridge for prop-999 does not match what the
    // bridge would compute for prop-001, so proof mismatch is expected.
    assert(result.status() == GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH ||
           result.status() == GovernanceEvidenceValidationStatus::APPROVAL_PROPOSAL_MISMATCH);
}

void testStatusToString() {
    using nodo::economics::governanceEvidenceValidationStatusToString;
    assert(governanceEvidenceValidationStatusToString(
               GovernanceEvidenceValidationStatus::ACCEPTED) == "ACCEPTED");
    assert(governanceEvidenceValidationStatusToString(
               GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT) ==
           "MISSING_GOVERNANCE_CONTEXT");
    assert(governanceEvidenceValidationStatusToString(
               GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH) ==
           "APPROVAL_PROOF_MISMATCH");
}

} // namespace

int main() {
    testEvidenceWithBridgeApprovalAccepted();
    testEvidenceWithoutGovernanceContextRejected();
    testEvidenceWithForgedApprovalProofRejected();
    testEvidenceWithRejectedDecisionRejected();
    testEvidenceWithDifferentProposalRejected();
    testStatusToString();
    return 0;
}
