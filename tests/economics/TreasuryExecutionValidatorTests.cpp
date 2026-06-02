#include "economics/TreasuryExecutionValidator.hpp"

#include "../common/GovernanceLifecycleFixtures.hpp"

#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {

using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasuryExecutionValidationStatus;
using nodo::economics::TreasuryExecutionValidator;
using nodo::tests::fixtures::validExecutionEvidence;
using nodo::tests::fixtures::validSpendPolicy;
using nodo::tests::fixtures::validTreasury;
using nodo::tests::fixtures::validTreasuryProposal;
using nodo::utils::Amount;

void testValidLifecycleBackedEvidenceAccepted() {
    const auto evidence = validExecutionEvidence();
    const auto result = TreasuryExecutionValidator::validateEvidence(evidence);
    assert(result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testInvalidEvidenceRejected() {
    const TreasuryExecutionEvidence invalidEvidence;
    const auto result = TreasuryExecutionValidator::validateEvidence(invalidEvidence);
    assert(!result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::INVALID_EVIDENCE);
}

void testMissingGovernanceContextRejected() {
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
        "ev-no-gov",
        proposal,
        directApproval,
        validSpendPolicy(),
        validTreasury(),
        10,
        Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001
    );

    const auto result = TreasuryExecutionValidator::validateEvidence(evidence);
    assert(!result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::MISSING_GOVERNANCE_CONTEXT);
}

void testSpendRecordMismatchRejected() {
    const auto goodEvidence = validExecutionEvidence();
    const auto proposal = validTreasuryProposal();
    const auto approval = goodEvidence.approval();
    const auto goodSpend = goodEvidence.spendRecord();

    const TreasuryExecutionEvidence mismatched(
        "ev-mismatch",
        proposal,
        approval,
        validSpendPolicy(),
        validTreasury(),
        11,
        Amount::fromRawUnits(0),
        goodSpend,
        1900000001,
        goodEvidence.governanceContext()
    );

    const auto result = TreasuryExecutionValidator::validateEvidence(mismatched);
    assert(!result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::INVALID_EVIDENCE ||
           result.status() == TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH);
}

void testStatusToString() {
    assert(nodo::economics::treasuryExecutionValidationStatusToString(
               TreasuryExecutionValidationStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::economics::treasuryExecutionValidationStatusToString(
               TreasuryExecutionValidationStatus::MISSING_GOVERNANCE_CONTEXT) ==
           "MISSING_GOVERNANCE_CONTEXT");
    assert(nodo::economics::treasuryExecutionValidationStatusToString(
               TreasuryExecutionValidationStatus::INVALID_GOVERNANCE_CONTEXT) ==
           "INVALID_GOVERNANCE_CONTEXT");
}

} // namespace

int main() {
    testValidLifecycleBackedEvidenceAccepted();
    testInvalidEvidenceRejected();
    testMissingGovernanceContextRejected();
    testSpendRecordMismatchRejected();
    testStatusToString();
    return 0;
}
