#include "economics/TreasurySpendValidator.hpp"
#include "economics/TreasuryAccount.hpp"
#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryPolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <limits>
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
    bool reqApproval = true
) {
    return TreasuryPolicy(
        "treasury-policy-v1",
        maxEpoch, maxProp, timelock, reqApproval, false
    );
}

TreasuryProposal validProposal(
    Amount amount = Amount::fromRawUnits(50000),
    std::uint64_t createdAt = 1,
    const std::string& id = "prop-001"
) {
    return TreasuryProposal(
        id, "recipient-addr", amount,
        "fund validator", createdAt, 0, "proposer-node"
    );
}

TreasuryApproval validApproval(const std::string& proposalId = "prop-001") {
    return TreasuryApproval(
        "appr-001", proposalId, 3,
        "governance-node", "valid-proof-abc"
    );
}

// Zero maxSpendPerProposal blocks all spending — not treated as unlimited.
void testZeroProposalLimitRejects() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(0)),
        validProposal(Amount::fromRawUnits(1)),  // even 1 raw unit is blocked
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::PROPOSAL_LIMIT_EXCEEDED);
}

// Zero maxSpendPerEpoch blocks all spending — not treated as unlimited.
void testZeroEpochLimitRejects() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(0), Amount::fromRawUnits(100000)),
        validProposal(Amount::fromRawUnits(1)),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::EPOCH_LIMIT_EXCEEDED);
}

// Non-zero limits allow valid spend within limits.
void testNonZeroLimitsAllowValidSpend() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(100000), Amount::fromRawUnits(50000)),
        validProposal(Amount::fromRawUnits(30000)),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    assert(result.status() == TreasurySpendStatus::ACCEPTED);
}

// Proposal amount above maxSpendPerProposal is rejected.
void testProposalAboveLimitRejects() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(500000), Amount::fromRawUnits(10000)),
        validProposal(Amount::fromRawUnits(10001)),  // 10001 > 10000
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::PROPOSAL_LIMIT_EXCEEDED);
}

// epochSpentSoFar + proposal.amount > maxSpendPerEpoch is rejected.
void testEpochTotalAboveLimitRejects() {
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        validPolicy(Amount::fromRawUnits(100000), Amount::fromRawUnits(100000)),
        validProposal(Amount::fromRawUnits(60000)),
        validApproval(),
        10,
        Amount::fromRawUnits(50000)  // 50000 + 60000 = 110000 > 100000
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::EPOCH_LIMIT_EXCEEDED);
}

// Timelock overflow (createdAtBlock + timelockBlocks would overflow uint64) is rejected.
void testTimelockOverflowRejected() {
    const std::uint64_t maxU64 = std::numeric_limits<std::uint64_t>::max();
    // createdAtBlock=1, timelockBlocks=maxU64 -> sum overflows
    const TreasuryPolicy overflowPolicy(
        "treasury-policy-v1",
        Amount::fromRawUnits(500000),
        Amount::fromRawUnits(100000),
        maxU64,  // extremely large timelock
        true,
        false
    );
    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(),
        overflowPolicy,
        validProposal(Amount::fromRawUnits(50000), 1),
        validApproval(),
        maxU64,
        Amount::fromRawUnits(0)
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::TIMELOCK_OVERFLOW);
}

// Epoch spend overflow (epochSpentSoFar + amount overflows int64) is rejected.
void testEpochSpendOverflowRejected() {
    const std::int64_t maxI64 = std::numeric_limits<std::int64_t>::max();
    // epochSpentSoFar near max, proposal.amount would overflow
    const Amount nearMax = Amount::fromRawUnits(maxI64 - 100);
    const Amount step    = Amount::fromRawUnits(200);  // nearMax + step overflows

    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(Amount::fromRawUnits(maxI64)),
        validPolicy(
            Amount::fromRawUnits(maxI64),  // epoch limit irrelevant — overflow is caught first
            Amount::fromRawUnits(maxI64)   // proposal limit allows it
        ),
        validProposal(step),
        validApproval(),
        10,
        nearMax
    );
    assert(!result.accepted());
    assert(result.status() == TreasurySpendStatus::EPOCH_SPEND_OVERFLOW);
}

// Large safe values (well within bounds) still work.
void testLargeSafeValuesWork() {
    const Amount bigBalance  = Amount::fromRawUnits(1000000000);
    const Amount bigLimit    = Amount::fromRawUnits(500000000);
    const Amount spendAmount = Amount::fromRawUnits(300000000);

    const auto result = TreasurySpendValidator::validateSpend(
        validTreasury(bigBalance),
        validPolicy(bigLimit, bigLimit),
        validProposal(spendAmount),
        validApproval(),
        10,
        Amount::fromRawUnits(0)
    );
    assert(result.accepted());
    assert(result.spendRecord().amount() == spendAmount);
}

// Deterministic spend id: same inputs produce the same id.
void testSpendIdIsDeterministic() {
    const auto r1 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    const auto r2 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(r1.accepted() && r2.accepted());
    assert(r1.spendRecord().spendId() == r2.spendRecord().spendId());
}

// Different executedAtBlock changes the spend id.
void testDifferentBlockChangesId() {
    const auto r1 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    const auto r2 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), validProposal(), validApproval(),
        11, Amount::fromRawUnits(0)
    );
    assert(r1.accepted() && r2.accepted());
    assert(r1.spendRecord().spendId() != r2.spendRecord().spendId());
}

// Different proposal amount changes the spend id.
void testDifferentAmountChangesId() {
    const auto r1 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(),
        validProposal(Amount::fromRawUnits(10000)), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    const auto r2 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(),
        validProposal(Amount::fromRawUnits(20000)), validApproval(),
        10, Amount::fromRawUnits(0)
    );
    assert(r1.accepted() && r2.accepted());
    assert(r1.spendRecord().spendId() != r2.spendRecord().spendId());
}

// Different recipient changes the spend id.
void testDifferentRecipientChangesId() {
    const TreasuryProposal prop1(
        "prop-001", "recipient-A", Amount::fromRawUnits(50000),
        "purpose", 1, 0, "proposer"
    );
    const TreasuryProposal prop2(
        "prop-001", "recipient-B", Amount::fromRawUnits(50000),
        "purpose", 1, 0, "proposer"
    );
    const auto r1 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), prop1, validApproval("prop-001"),
        10, Amount::fromRawUnits(0)
    );
    const auto r2 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), prop2, validApproval("prop-001"),
        10, Amount::fromRawUnits(0)
    );
    assert(r1.accepted() && r2.accepted());
    assert(r1.spendRecord().spendId() != r2.spendRecord().spendId());
}

// Different epoch (requestedEpoch) changes the spend id.
void testDifferentEpochChangesId() {
    const TreasuryProposal prop1(
        "prop-001", "recipient-addr", Amount::fromRawUnits(50000),
        "purpose", 1, 0, "proposer"
    );
    const TreasuryProposal prop2(
        "prop-001", "recipient-addr", Amount::fromRawUnits(50000),
        "purpose", 1, 1, "proposer"
    );
    const auto r1 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), prop1, validApproval("prop-001"),
        10, Amount::fromRawUnits(0)
    );
    const auto r2 = TreasurySpendValidator::validateSpend(
        validTreasury(), validPolicy(), prop2, validApproval("prop-001"),
        10, Amount::fromRawUnits(0)
    );
    assert(r1.accepted() && r2.accepted());
    assert(r1.spendRecord().spendId() != r2.spendRecord().spendId());
}

} // namespace

int main() {
    testZeroProposalLimitRejects();
    testZeroEpochLimitRejects();
    testNonZeroLimitsAllowValidSpend();
    testProposalAboveLimitRejects();
    testEpochTotalAboveLimitRejects();
    testTimelockOverflowRejected();
    testEpochSpendOverflowRejected();
    testLargeSafeValuesWork();
    testSpendIdIsDeterministic();
    testDifferentBlockChangesId();
    testDifferentAmountChangesId();
    testDifferentRecipientChangesId();
    testDifferentEpochChangesId();
    return 0;
}
