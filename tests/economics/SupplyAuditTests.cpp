#include "economics/SupplyAudit.hpp"

#include <cassert>
#include <vector>

namespace {

nodo::economics::ValidatorStakeState makeStake(
    const std::string& addr,
    std::int64_t bonded,
    std::int64_t slashed = 0
) {
    nodo::economics::StakeAccount acct(addr, nodo::utils::Amount::fromRawUnits(bonded));
    if (slashed > 0) {
        acct.applySlash(nodo::utils::Amount::fromRawUnits(slashed));
    }
    return nodo::economics::ValidatorStakeState(std::move(acct));
}

void testBalancedSupplyPasses() {
    // genesis=1000, rewards=100, burned=50 → effective=1050
    // bonded=400, slashed=50, treasury=100, free=500
    // 400+50+100+500 = 1050 ✓
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount::fromRawUnits(1000),
        {makeStake("v-a", 400, 50)},
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(500)
    );
    assert(result.isValid());
}

void testMismatchedSupplyFails() {
    // genesis=1000, rewards=0, burned=0 → effective=1000
    // bonded=400, slashed=0, treasury=100, free=600 → rhs=1100 ≠ 1000
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount::fromRawUnits(1000),
        {makeStake("v-a", 400)},
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(600)
    );
    assert(!result.isValid());
    assert(!result.reason().empty());
}

void testNegativeGenesisSupplyFails() {
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount(-1),  // raw constructor allows negative for testing
        {},
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0)
    );
    assert(!result.isValid());
}

void testNegativeTreasuryFails() {
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount::fromRawUnits(1000),
        {},
        nodo::utils::Amount(-1),  // raw constructor allows negative for testing
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(!result.isValid());
}

void testEmptyStakesAndZeroSupply() {
    // genesis=0, no stakes, treasury=0, rewards=0, burned=0, free=0 → 0==0 ✓
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount::fromRawUnits(0),
        {},
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0)
    );
    assert(result.isValid());
}

void testReportSerializes() {
    const auto result = nodo::economics::SupplyAudit::audit(
        nodo::utils::Amount::fromRawUnits(500),
        {},
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(500)
    );
    assert(result.isValid());
    const std::string s = result.serialize();
    assert(!s.empty());
    assert(s.find("valid=1") != std::string::npos);
}

} // namespace

int main() {
    testBalancedSupplyPasses();
    testMismatchedSupplyFails();
    testNegativeGenesisSupplyFails();
    testNegativeTreasuryFails();
    testEmptyStakesAndZeroSupply();
    testReportSerializes();
    return 0;
}
