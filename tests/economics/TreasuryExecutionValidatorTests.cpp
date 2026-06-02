#include "economics/TreasuryExecutionValidator.hpp"
#include "economics/TreasurySpendValidator.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendRecord;
using nodo::economics::TreasuryExecutionEvidence;
using nodo::economics::TreasuryExecutionValidator;
using nodo::economics::TreasuryExecutionValidationStatus;
using nodo::utils::Amount;

TreasuryAccount validTreasury(Amount balance = Amount::fromRawUnits(1000000)) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr",
        balance, 0, false, ""
    );
}

TreasuryPolicy validPolicy(std::uint64_t timelock = 5) {
    return TreasuryPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000),
        Amount::fromRawUnits(100000),
        timelock, true, false
    );
}

TreasuryProposal validProposal() {
    return TreasuryProposal(
        "prop-001", "recipient-addr",
        Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
}

TreasuryApproval validApproval() {
    return TreasuryApproval(
        "appr-001", "prop-001", 3, "governance-node", "valid-proof-abc"
    );
}

TreasuryExecutionEvidence buildValidEvidence(
    std::uint64_t blockHeight = 10
) {
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        blockHeight, Amount::fromRawUnits(0)
    );
    assert(spendResult.accepted());
    return TreasuryExecutionEvidence(
        "ev-001",
        validProposal(), validApproval(), validPolicy(),
        validTreasury(), blockHeight,
        Amount::fromRawUnits(0),
        spendResult.spendRecord(),
        1900000001
    );
}

void testValidEvidenceAccepted() {
    const auto ev = buildValidEvidence();
    const auto result = TreasuryExecutionValidator::validateEvidence(ev);
    assert(result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::ACCEPTED);
    assert(result.reason().empty());
}

void testInvalidEvidenceRejected() {
    const TreasuryExecutionEvidence invalidEv;  // default-constructed = invalid
    const auto result = TreasuryExecutionValidator::validateEvidence(invalidEv);
    assert(!result.isAccepted());
    assert(result.status() == TreasuryExecutionValidationStatus::INVALID_EVIDENCE);
}

void testTimelockViolationRejected() {
    // blockHeight=3, createdAt=1, timelock=5 → 3 < 1+5=6, so timelock violated.
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(5), validProposal(), validApproval(),
        3, Amount::fromRawUnits(0)
    );
    assert(!spendResult.accepted());
    // Can't build valid evidence with timelock violation since validator rejects it.
    // Build evidence with an arbitrary spend record that has the wrong block.
    const auto goodSpend = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(5), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    ).spendRecord();

    // Force mismatch: evidence says blockHeight=3 but spend record says executedAtBlock=10.
    const TreasuryExecutionEvidence badEv(
        "ev-timelock",
        validProposal(), validApproval(), validPolicy(5),
        validTreasury(), 3,  // timelock violated block
        Amount::fromRawUnits(0), goodSpend, 1900000001
    );
    // Evidence structural check: executedAtBlock(10) != currentBlockHeight(3) → invalid evidence.
    const auto result = TreasuryExecutionValidator::validateEvidence(badEv);
    assert(!result.isAccepted());
    // Either INVALID_EVIDENCE (structural mismatch) or SPEND_VALIDATOR_REJECTED.
    assert(result.status() == TreasuryExecutionValidationStatus::INVALID_EVIDENCE ||
           result.status() == TreasuryExecutionValidationStatus::SPEND_VALIDATOR_REJECTED);
}

void testInsufficientTreasuryBalanceRejected() {
    // Treasury has only 1000, proposal wants 50000.
    const auto spendResult = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(Amount::fromRawUnits(1000)),
        validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(!spendResult.accepted());

    // Build evidence with a mismatched spend record (from a wealthy treasury).
    const auto richSpend = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    ).spendRecord();

    // The evidence uses a poor treasury but a spend record from a rich one.
    // Structural check: richSpend.treasuryBalanceBefore(1000000) != poorTreasury.balance(1000)
    const TreasuryExecutionEvidence ev(
        "ev-poor",
        validProposal(), validApproval(), validPolicy(),
        validTreasury(Amount::fromRawUnits(1000)),
        10, Amount::fromRawUnits(0), richSpend, 1900000001
    );
    const auto result = TreasuryExecutionValidator::validateEvidence(ev);
    assert(!result.isAccepted());
}

void testSpendRecordMismatchRejected() {
    // Build two valid evidences for two different proposals — use the second's
    // spend record with the first's evidence fields.
    const TreasuryProposal proposal1(
        "prop-001", "recipient-addr", Amount::fromRawUnits(50000),
        "fund validator", 1, 0, "proposer-node"
    );
    const TreasuryProposal proposal2(
        "prop-002", "other-recipient", Amount::fromRawUnits(30000),
        "different purpose", 1, 0, "proposer-node"
    );
    const TreasuryApproval approval2("appr-002", "prop-002", 3, "gov", "proof-002");

    const auto spend2 = nodo::economics::TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), proposal2, approval2,
        10, Amount::fromRawUnits(0)
    );
    assert(spend2.accepted());

    // Evidence uses proposal1 but spend record from proposal2.
    // Structural check will catch the proposalId mismatch.
    const TreasuryExecutionEvidence ev(
        "ev-mismatch",
        proposal1, validApproval(), validPolicy(),
        validTreasury(), 10, Amount::fromRawUnits(0),
        spend2.spendRecord(), 1900000001
    );
    const auto result = TreasuryExecutionValidator::validateEvidence(ev);
    assert(!result.isAccepted());
}

void testStatusToString() {
    assert(nodo::economics::treasuryExecutionValidationStatusToString(
               TreasuryExecutionValidationStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::economics::treasuryExecutionValidationStatusToString(
               TreasuryExecutionValidationStatus::SPEND_RECORD_MISMATCH) ==
           "SPEND_RECORD_MISMATCH");
}

} // namespace

int main() {
    testValidEvidenceAccepted();
    testInvalidEvidenceRejected();
    testTimelockViolationRejected();
    testInsufficientTreasuryBalanceRejected();
    testSpendRecordMismatchRejected();
    testStatusToString();
    return 0;
}
