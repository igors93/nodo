#include "economics/TreasurySpendValidator.hpp"
#include "economics/TreasuryAccount.hpp"
#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryPolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::TreasuryAccount;
using nodo::economics::TreasuryApproval;
using nodo::economics::TreasuryPolicy;
using nodo::economics::TreasuryProposal;
using nodo::economics::TreasurySpendStatus;
using nodo::economics::TreasurySpendValidator;
using nodo::utils::Amount;

TreasuryAccount validTreasury(
    Amount balance = Amount::fromRawUnits(1000000),
    bool locked = false
) {
    return TreasuryAccount(
        "treasury-main", "nodo-treasury-addr",
        balance, 0, locked, locked ? "locked" : ""
    );
}

TreasuryPolicy validPolicy(
    Amount maxEpoch = Amount::fromRawUnits(500000),
    Amount maxProp  = Amount::fromRawUnits(100000),
    std::uint64_t timelock = 5,
    bool reqApproval = true,
    bool allowWhenLocked = false
) {
    return TreasuryPolicy(
        "treasury-policy-v1",
        maxEpoch, maxProp,
        timelock, reqApproval, allowWhenLocked
    );
}

TreasuryProposal validProposal(
    Amount amount = Amount::fromRawUnits(50000),
    std::uint64_t createdAt = 1
) {
    return TreasuryProposal(
        "prop-001", "recipient-addr", amount,
        "fund validator", createdAt, 0, "proposer-node"
    );
}

TreasuryApproval validApproval(
    const std::string& proposalId = "prop-001"
) {
    return TreasuryApproval(
        "appr-001", proposalId, 3,
        "governance-node", "valid-proof-abc"
    );
}

// Valid spend is accepted.
void testValidSpendAccepted() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(),
        validProposal(),
        validApproval(),
        10,  // currentBlockHeight >= createdAt(1) + timelock(5) = 6
        Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    assert(result.status() == TreasurySpendStatus::ACCEPTED);
    assert(result.spendRecord().isValid());
    assert(result.spendRecord().amount() == Amount::fromRawUnits(50000));
}

// Missing approval when required is rejected.
void testMissingApprovalRejectedWhenRequired() {
    const TreasuryApproval badApproval;  // default-constructed, invalid
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(100000), 5, true),
        validProposal(),
        badApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::INVALID_APPROVAL);
}

// Approval proposalId mismatch is rejected.
void testApprovalMismatchRejected() {
    const TreasuryApproval wrongApproval(
        "appr-002", "prop-WRONG", 3,
        "governance-node", "proof-xyz"
    );
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(),
        validProposal(),
        wrongApproval,
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::APPROVAL_MISMATCH);
}

// Timelock not satisfied is rejected.
void testTimelockNotSatisfiedRejected() {
    // proposal.createdAtBlock=1, timelock=5 -> unlockAt=6, but currentBlock=5.
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(100000), 5),
        validProposal(Amount::fromRawUnits(50000), 1),
        validApproval(),
        5,  // too early; needs >= 6
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::TIMELOCK_NOT_SATISFIED);
}

// Insufficient treasury balance is rejected.
void testInsufficientBalanceRejected() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(Amount::fromRawUnits(100)),  // only 100 raw units
        validPolicy(),
        validProposal(Amount::fromRawUnits(50000)),  // asks for 50000
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::INSUFFICIENT_TREASURY_BALANCE);
}

// maxSpendPerProposal exceeded is rejected.
void testProposalLimitExceededRejected() {
    // maxSpendPerProposal = 1000, proposal.amount = 50000.
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(1000)),
        validProposal(Amount::fromRawUnits(50000)),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::PROPOSAL_LIMIT_EXCEEDED);
}

// maxSpendPerEpoch exceeded is rejected.
void testEpochLimitExceededRejected() {
    // maxSpendPerEpoch = 60000, epochSpentSoFar = 40000, proposal = 50000 -> total 90000 > 60000.
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(60000), Amount::fromRawUnits(100000)),
        validProposal(Amount::fromRawUnits(50000)),
        validApproval(),
        10,
        Amount::fromRawUnits(40000)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::EPOCH_LIMIT_EXCEEDED);
}

// Locked treasury is rejected.
void testLockedTreasuryRejected() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(Amount::fromRawUnits(1000000), true),  // locked
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(100000),
                    5, true, false),  // allowSpendingWhenLocked=false
        validProposal(),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::TREASURY_LOCKED);
}

// Accepted spend record balanceAfter is correct.
void testAcceptedSpendRecordBalanceAfterCorrect() {
    const Amount treasuryBalance = Amount::fromRawUnits(100000);
    const Amount proposalAmount  = Amount::fromRawUnits(30000);

    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(treasuryBalance),
        validPolicy(),
        validProposal(proposalAmount),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    assert(result.spendRecord().treasuryBalanceBefore() == treasuryBalance);
    assert(result.spendRecord().treasuryBalanceAfter() ==
           Amount::fromRawUnits(treasuryBalance.rawUnits() - proposalAmount.rawUnits()));
}

} // namespace

int main() {
    testValidSpendAccepted();
    testMissingApprovalRejectedWhenRequired();
    testApprovalMismatchRejected();
    testTimelockNotSatisfiedRejected();
    testInsufficientBalanceRejected();
    testProposalLimitExceededRejected();
    testEpochLimitExceededRejected();
    testLockedTreasuryRejected();
    testAcceptedSpendRecordBalanceAfterCorrect();
    return 0;
}
