#include "economics/TreasuryPolicy.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

using nodo::economics::TreasuryPolicy;
using nodo::utils::Amount;

TreasuryPolicy validPolicy(
    Amount maxEpoch  = Amount::fromRawUnits(100000),
    Amount maxProp   = Amount::fromRawUnits(10000),
    std::uint64_t timelock = 5,
    bool reqApproval = true
) {
    return TreasuryPolicy(
        "treasury-policy-v1",
        maxEpoch, maxProp,
        timelock, reqApproval, false
    );
}

// Valid policy.
void testValidPolicy() {
    const auto p = validPolicy();
    assert(p.isValid());
    assert(p.policyVersion() == "treasury-policy-v1");
    assert(p.timelockBlocks() == 5);
    assert(p.requireApproval());
    assert(!p.allowSpendingWhenLocked());
}

// Empty policy version is rejected.
void testEmptyPolicyVersionRejected() {
    const TreasuryPolicy p(
        "", Amount::fromRawUnits(1000), Amount::fromRawUnits(100),
        5, true, false
    );
    assert(!p.isValid());
    assert(p.rejectionReason().find("policyVersion") != std::string::npos);
}

// Negative maxSpendPerEpoch is rejected. Amount throws for negative values,
// which is the primary defense against negative limits.
void testNegativeEpochLimitRejected() {
    bool threw = false;
    try {
        const TreasuryPolicy p(
            "v1", Amount::fromRawUnits(-1), Amount::fromRawUnits(100),
            5, true, false
        );
        (void)p;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Negative maxSpendPerProposal is rejected.
void testNegativeProposalLimitRejected() {
    bool threw = false;
    try {
        const TreasuryPolicy p(
            "v1", Amount::fromRawUnits(1000), Amount::fromRawUnits(-1),
            5, true, false
        );
        (void)p;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Zero spend policy has zero limits.
void testZeroSpendPolicyHasZeroLimits() {
    const TreasuryPolicy p(
        "zero-policy",
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(0),
        10, true, false
    );
    assert(p.isValid());
    assert(p.maxSpendPerEpoch().isZero());
    assert(p.maxSpendPerProposal().isZero());
}

// Timelock value is exposed correctly.
void testTimelockValueExposed() {
    const auto p = validPolicy(
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(10000),
        42
    );
    assert(p.isValid());
    assert(p.timelockBlocks() == 42);
}

} // namespace

int main() {
    testValidPolicy();
    testEmptyPolicyVersionRejected();
    testNegativeEpochLimitRejected();
    testNegativeProposalLimitRejected();
    testZeroSpendPolicyHasZeroLimits();
    testTimelockValueExposed();
    return 0;
}
