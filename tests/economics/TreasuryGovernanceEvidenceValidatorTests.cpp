#include "economics/TreasuryGovernanceEvidenceValidator.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/GovernanceApprovalBridge.hpp"
#include "economics/GovernanceDecisionBuilder.hpp"
#include "economics/GovernanceVoteProof.hpp"
#include "economics/TreasuryApprovalProof.hpp"
#include "economics/TreasuryExecutionEvidence.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

using nodo::economics::GovernanceApprovalBridge;
using nodo::economics::GovernanceApprovalContext;
using nodo::economics::GovernanceDecisionRecord;
using nodo::economics::GovernanceDecisionStatus;
using nodo::economics::GovernanceEvidenceValidationStatus;
using nodo::economics::GovernanceLifecycleRecord;
using nodo::economics::GovernanceTallyReport;
using nodo::economics::GovernanceVoteChoice;
using nodo::economics::GovernanceVoteEvidence;
using nodo::economics::GovernanceVoteProof;
using nodo::economics::GovernanceVoteRecord;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryApprovalProof;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasuryGovernanceEvidenceValidator;
using nodo::tests::fixtures::approvalFromLifecycle;
using nodo::tests::fixtures::validExecutionEvidence;
using nodo::tests::fixtures::validGovernancePolicy;
using nodo::tests::fixtures::validLifecycle;
using nodo::tests::fixtures::validSpendPolicy;
using nodo::tests::fixtures::validTreasury;
using nodo::tests::fixtures::validTreasuryProposal;
using nodo::tests::fixtures::validVotes;
using nodo::tests::fixtures::validVotingPolicy;
using nodo::utils::Amount;

TreasuryExecutionEvidence evidenceWithLifecycle(
    const GovernanceLifecycleRecord& lifecycle,
    const TreasuryApproval& approval,
    const std::string& evidenceId = "ev-custom"
) {
    const auto proposal = lifecycle.proposalEnvelope().treasuryProposal();
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(),
        validSpendPolicy(),
        proposal,
        approval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    GovernanceApprovalContext context;
    context.governanceLifecycle = lifecycle;

    return TreasuryExecutionEvidence(
        evidenceId,
        proposal,
        approval,
        validSpendPolicy(),
        validTreasury(),
        10,
        Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(context)
    );
}

void testValidLifecycleBackedApprovalAccepted() {
    const auto evidence = validExecutionEvidence();
    const auto result =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);
    assert(result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::ACCEPTED);
}

void testDirectApprovalRejected() {
    const auto proposal = validTreasuryProposal();
    const TreasuryApproval directApproval(
        "appr-001",
        proposal.proposalId(),
        3,
        "governance-node",
        "manual-proof"
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(),
        validSpendPolicy(),
        proposal,
        directApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    const TreasuryExecutionEvidence evidence(
        "ev-direct",
        proposal,
        directApproval,
        validSpendPolicy(),
        validTreasury(),
        10,
        Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001
    );

    const auto result =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::MISSING_GOVERNANCE_CONTEXT);
}

void testOldBridgeOnlyApprovalRejectedInProduction() {
    const GovernanceDecisionRecord manualDecision(
        "manual-decision",
        "gov-prop-001",
        "TREASURY_SPEND",
        GovernanceDecisionStatus::APPROVED,
        20,
        "governance-node",
        "manual-proof",
        "governance-v1"
    );

    const auto bridgeResult =
        GovernanceApprovalBridge::produceTreasuryApprovalFromStructurallyValidDecisionForTestsOnly(
            validGovernancePolicy(),
            nodo::tests::fixtures::validEnvelope(),
            manualDecision
        );
    assert(bridgeResult.isAccepted());

    GovernanceApprovalContext context;
    context.governanceLifecycle = validLifecycle();
    const TreasuryExecutionEvidence evidence = evidenceWithLifecycle(
        context.governanceLifecycle,
        bridgeResult.treasuryApproval(),
        "ev-old-bridge"
    );

    const auto result =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::APPROVAL_PROOF_MISMATCH ||
           result.status() == GovernanceEvidenceValidationStatus::APPROVAL_ID_MISMATCH);
}

void testForgedVoteProofRejected() {
    auto votes = validVotes();
    const GovernanceVoteRecord forgedRecord(
        "vote-001",
        "gov-prop-001",
        "validator-a",
        GovernanceVoteChoice::YES,
        60,
        12,
        "governance-v1"
    );
    votes[0] = GovernanceVoteEvidence(forgedRecord, "forged-proof");

    const auto lifecycle = GovernanceLifecycleRecord(
        "lifecycle-forged-vote",
        nodo::tests::fixtures::validEnvelope(),
        validGovernancePolicy(),
        validVotingPolicy(),
        votes,
        validLifecycle().tallyReport(),
        validLifecycle().decisionRecord(),
        5,
        20
    );
    assert(!lifecycle.isValid());
    const auto bridgeResult =
        GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle(lifecycle);
    assert(!bridgeResult.isAccepted());
    assert(bridgeResult.status() == nodo::economics::GovernanceApprovalBridgeStatus::INVALID_LIFECYCLE);
}

void testTamperedTallyRejected() {
    const auto lifecycle = validLifecycle();
    const GovernanceTallyReport tamperedTally(
        lifecycle.tallyReport().governanceProposalId(),
        lifecycle.tallyReport().policyVersion(),
        lifecycle.tallyReport().totalVotingPower(),
        59,
        lifecycle.tallyReport().noVotingPower(),
        lifecycle.tallyReport().abstainVotingPower() + 1,
        lifecycle.tallyReport().yesVoteCount(),
        lifecycle.tallyReport().noVoteCount(),
        lifecycle.tallyReport().abstainVoteCount(),
        true,
        false,
        false,
        GovernanceTallyReport::buildTallyProof(
            lifecycle.tallyReport().governanceProposalId(),
            lifecycle.tallyReport().policyVersion(),
            lifecycle.tallyReport().totalVotingPower(),
            59,
            lifecycle.tallyReport().noVotingPower(),
            lifecycle.tallyReport().abstainVotingPower() + 1,
            lifecycle.tallyReport().yesVoteCount(),
            lifecycle.tallyReport().noVoteCount(),
            lifecycle.tallyReport().abstainVoteCount(),
            true,
            false,
            false
        )
    );
    assert(tamperedTally.isValid());

    const GovernanceLifecycleRecord tamperedLifecycle(
        "lifecycle-tampered-tally",
        lifecycle.proposalEnvelope(),
        lifecycle.governancePolicy(),
        lifecycle.votingPolicy(),
        lifecycle.voteEvidenceList(),
        tamperedTally,
        lifecycle.decisionRecord(),
        lifecycle.createdAtBlock(),
        lifecycle.finalizedAtBlock()
    );
    assert(tamperedLifecycle.isValid());

    const TreasuryExecutionEvidence evidence =
        evidenceWithLifecycle(tamperedLifecycle, approvalFromLifecycle(lifecycle));
    const auto result =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_LIFECYCLE);
}

void testTamperedDecisionRejected() {
    const auto lifecycle = validLifecycle();
    const GovernanceDecisionRecord tamperedDecision(
        lifecycle.decisionRecord().decisionId(),
        lifecycle.decisionRecord().governanceProposalId(),
        lifecycle.decisionRecord().proposalType(),
        GovernanceDecisionStatus::REJECTED,
        lifecycle.decisionRecord().decidedAtBlock(),
        lifecycle.decisionRecord().decisionMaker(),
        lifecycle.decisionRecord().decisionProof(),
        lifecycle.decisionRecord().policyVersion()
    );

    const GovernanceLifecycleRecord tamperedLifecycle(
        "lifecycle-tampered-decision",
        lifecycle.proposalEnvelope(),
        lifecycle.governancePolicy(),
        lifecycle.votingPolicy(),
        lifecycle.voteEvidenceList(),
        lifecycle.tallyReport(),
        tamperedDecision,
        lifecycle.createdAtBlock(),
        lifecycle.finalizedAtBlock()
    );
    assert(tamperedLifecycle.isValid());

    const TreasuryExecutionEvidence evidence =
        evidenceWithLifecycle(tamperedLifecycle, approvalFromLifecycle(lifecycle));
    const auto result =
        TreasuryGovernanceEvidenceValidator::validateGovernanceContext(evidence);
    assert(!result.isAccepted());
    assert(result.status() == GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_LIFECYCLE);
}

void testApprovalForDifferentTreasuryProposalRejected() {
    const auto lifecycle = validLifecycle();
    const auto approval = approvalFromLifecycle(lifecycle);

    GovernanceApprovalContext context;
    context.governanceLifecycle = lifecycle;

    const auto proposal = validTreasuryProposal("prop-999");
    const TreasuryApproval mismatchedApproval(
        approval.approvalId(),
        proposal.proposalId(),
        approval.approvedAtBlock(),
        approval.approver(),
        approval.approvalProof()
    );
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(),
        validSpendPolicy(),
        proposal,
        mismatchedApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    const TreasuryExecutionEvidence evidence(
        "ev-proposal-mismatch",
        proposal,
        mismatchedApproval,
        validSpendPolicy(),
        validTreasury(),
        10,
        Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001,
        std::move(context)
    );
    assert(!evidence.isValid());
}

void testStatusToString() {
    using nodo::economics::governanceEvidenceValidationStatusToString;
    assert(governanceEvidenceValidationStatusToString(
               GovernanceEvidenceValidationStatus::ACCEPTED) == "ACCEPTED");
    assert(governanceEvidenceValidationStatusToString(
               GovernanceEvidenceValidationStatus::INVALID_GOVERNANCE_LIFECYCLE) ==
           "INVALID_GOVERNANCE_LIFECYCLE");
}

} // namespace

int main() {
    testValidLifecycleBackedApprovalAccepted();
    testDirectApprovalRejected();
    testOldBridgeOnlyApprovalRejectedInProduction();
    testForgedVoteProofRejected();
    testTamperedTallyRejected();
    testTamperedDecisionRejected();
    testApprovalForDifferentTreasuryProposalRejected();
    testStatusToString();
    return 0;
}
