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
using nodo::tests::fixtures::voteEvidence;
using nodo::utils::Amount;

TreasuryExecutionEvidence evidenceWithLifecycle(
    const GovernanceLifecycleRecord& lifecycle,
    const TreasuryApproval& approval,
    const std::string& evidenceId = "ev-custom"
) {
    const auto proposal = lifecycle.proposalEnvelope().treasuryProposal();
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        nodo::economics::DefenseModeState::INACTIVE,
        nodo::economics::DefenseModePolicy::defaultPolicy(),
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
        nodo::economics::DefenseModeState::INACTIVE,
        nodo::economics::DefenseModePolicy::defaultPolicy(),
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

// Forged vote proof makes the lifecycle structurally invalid at construction time.
// The bridge rejects it because lifecycle.isValid() == false.
void testForgedVoteProofRejected() {
    auto votes = validVotes();
    const GovernanceVoteRecord forgedRecord(
        "vote-001",
        "gov-prop-001",
        "validator-a",
        GovernanceVoteChoice::YES,
        Amount::fromRawUnits(60),
        12,
        "validator-stake",
        "forged-proof",
        "governance-v1"
    );
    votes[0] = GovernanceVoteEvidence(
        "evidence-vote-001",
        nodo::tests::fixtures::validEnvelope(),
        validVotingPolicy(),
        forgedRecord
    );

    // votes[0] is individually invalid (forged proof), so the lifecycle constructor
    // rejects it at the vote evidence validation step.
    const auto base = validLifecycle();
    const auto lifecycle = GovernanceLifecycleRecord(
        "lifecycle-forged-vote",
        nodo::tests::fixtures::validEnvelope(),
        validGovernancePolicy(),
        validVotingPolicy(),
        votes,
        base.tallyReport(),
        base.decisionRecord(),
        5,
        20,
        base.declaredCurrentState(),
        base.transitionHistory()
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
        static_cast<std::uint64_t>(lifecycle.tallyReport().totalVotingPower().rawUnits()),
        59,
        static_cast<std::uint64_t>(lifecycle.tallyReport().noVotingPower().rawUnits()),
        static_cast<std::uint64_t>(
            lifecycle.tallyReport().abstainVotingPower().rawUnits() + 1
        ),
        lifecycle.tallyReport().yesVoteCount(),
        lifecycle.tallyReport().noVoteCount(),
        lifecycle.tallyReport().abstainVoteCount(),
        true,
        false,
        false,
        GovernanceTallyReport::buildTallyProof(
            lifecycle.tallyReport().governanceProposalId(),
            lifecycle.tallyReport().policyVersion(),
            static_cast<std::uint64_t>(
                lifecycle.tallyReport().totalVotingPower().rawUnits()
            ),
            59,
            static_cast<std::uint64_t>(
                lifecycle.tallyReport().noVotingPower().rawUnits()
            ),
            static_cast<std::uint64_t>(
                lifecycle.tallyReport().abstainVotingPower().rawUnits() + 1
            ),
            lifecycle.tallyReport().yesVoteCount(),
            lifecycle.tallyReport().noVoteCount(),
            lifecycle.tallyReport().abstainVoteCount(),
            true,
            false,
            false
        )
    );
    assert(tamperedTally.isValid());

    // The constructor validates transition history independently from tally consistency.
    // A lifecycle with a tampered tally is structurally valid until the verifier checks it.
    const GovernanceLifecycleRecord tamperedLifecycle(
        "lifecycle-tampered-tally",
        lifecycle.proposalEnvelope(),
        lifecycle.governancePolicy(),
        lifecycle.votingPolicy(),
        lifecycle.voteEvidenceList(),
        tamperedTally,
        lifecycle.decisionRecord(),
        lifecycle.createdAtBlock(),
        lifecycle.finalizedAtBlock(),
        lifecycle.declaredCurrentState(),
        lifecycle.transitionHistory()
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

    // The constructor does not check that decision status matches tally.approved.
    // A lifecycle with a tampered decision status is structurally valid until the verifier.
    const GovernanceLifecycleRecord tamperedLifecycle(
        "lifecycle-tampered-decision",
        lifecycle.proposalEnvelope(),
        lifecycle.governancePolicy(),
        lifecycle.votingPolicy(),
        lifecycle.voteEvidenceList(),
        lifecycle.tallyReport(),
        tamperedDecision,
        lifecycle.createdAtBlock(),
        lifecycle.finalizedAtBlock(),
        lifecycle.declaredCurrentState(),
        lifecycle.transitionHistory()
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
    const auto lifecycleApproval = approvalFromLifecycle(lifecycle);

    GovernanceApprovalContext context;
    context.governanceLifecycle = lifecycle;

    // Proposal for a different treasury proposal (not what the lifecycle approved).
    const auto proposal = validTreasuryProposal("prop-999");

    // Construct an approval that references prop-999 (forged to bypass the spend validator).
    const TreasuryApproval mismatchedApproval(
        lifecycleApproval.approvalId(),
        proposal.proposalId(),       // "prop-999"
        lifecycleApproval.approvedAtBlock(),
        lifecycleApproval.approver(),
        lifecycleApproval.approvalProof()
    );

    // Use mismatchedApproval (proposalId="prop-999") with proposal (prop-999) so
    // the spend validator accepts it (IDs match).
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        nodo::economics::DefenseModeState::INACTIVE,
        nodo::economics::DefenseModePolicy::defaultPolicy(),
        validTreasury(),
        validSpendPolicy(),
        proposal,
        mismatchedApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());

    // Evidence carries the governance context (lifecycle for prop-001) but the
    // evidence proposal is prop-999.  TreasuryExecutionEvidence::validate() detects
    // this governance-lifecycle / evidence-proposal mismatch and rejects the evidence.
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
