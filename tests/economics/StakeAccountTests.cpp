#include "economics/StakeAccount.hpp"
#include "economics/SupplyAudit.hpp"
#include "economics/ValidatorStakeState.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

void testFreshAccountIsEligible() {
    const nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(acct.isEligible());
    assert(!acct.jailed());
    assert(!acct.tombstoned());
    assert(acct.bondedAmount() == nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.slashedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(acct.isValid());
}

void testJailBlocksEligibility() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.jail();
    assert(!acct.isEligible());
    assert(acct.jailed());
    assert(!acct.tombstoned());
}

void testUnjailRestoresEligibility() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.jail();
    acct.unjail();
    assert(acct.isEligible());
}

void testTombstoneBlocksEligibilityPermanently() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.tombstone();
    assert(!acct.isEligible());
    assert(acct.tombstoned());
    acct.unjail(); // unjail must not override tombstone
    assert(!acct.isEligible());
}

void testSlashReducesEffectiveStake() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.applySlash(nodo::utils::Amount::fromRawUnits(300));
    assert(acct.slashedAmount() == nodo::utils::Amount::fromRawUnits(300));
    assert(acct.bondedAmount() == nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.isValid());
}

void testFullSlashAutoTombstones() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    acct.applySlash(nodo::utils::Amount::fromRawUnits(1000));
    assert(acct.tombstoned());
    assert(!acct.isEligible());
}

void testSlashBeyondStakeThrows() {
    nodo::economics::StakeAccount acct(
        "validator-a",
        nodo::utils::Amount::fromRawUnits(100)
    );
    bool threw = false;
    try {
        acct.applySlash(nodo::utils::Amount::fromRawUnits(200));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testSupplyAuditIncludesSlash() {
    nodo::economics::ValidatorStakeState state(
        nodo::economics::StakeAccount(
            "validator-a",
            nodo::utils::Amount::fromRawUnits(1000)
        )
    );

    state.account().applySlash(nodo::utils::Amount::fromRawUnits(100));

    const auto audit =
        nodo::economics::SupplyAudit::audit(
            nodo::utils::Amount::fromRawUnits(1100),
            {state},
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0),
            nodo::utils::Amount::fromRawUnits(0)
        );

    assert(audit.isValid());
    assert(audit.totalSlashed() == nodo::utils::Amount::fromRawUnits(100));
}

void testAccountSerializes() {
    const nodo::economics::StakeAccount acct(
        "validator-x",
        nodo::utils::Amount::fromRawUnits(5000)
    );
    const std::string s = acct.serialize();
    assert(!s.empty());
    assert(s.find("validator-x") != std::string::npos);
    assert(s.find("5000") != std::string::npos);
}

} // namespace

int main() {
    testFreshAccountIsEligible();
    testJailBlocksEligibility();
    testUnjailRestoresEligibility();
    testTombstoneBlocksEligibilityPermanently();
    testSlashReducesEffectiveStake();
    testFullSlashAutoTombstones();
    testSlashBeyondStakeThrows();
    testSupplyAuditIncludesSlash();
    testAccountSerializes();
    return 0;
}
