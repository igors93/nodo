#include "economics/SupplyAudit.hpp"
#include "economics/BurnRecord.hpp"
#include "economics/MintRecord.hpp"

#include <cassert>
#include <stdexcept>
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
    // genesis=1000, rewards=100, burned=50 => effective=1050
    // bonded=400, slashed=50, treasury=100, free=500
    // 400+50+100+500 = 1050
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
    // genesis=1000, rewards=0, burned=0 => effective=1000
    // bonded=400, slashed=0, treasury=100, free=600 => rhs=1100 != 1000
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
    // Amount constructor now prevents negative values at the type level.
    bool threw = false;
    try { (void)nodo::utils::Amount(-1); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testNegativeTreasuryFails() {
    // Amount constructor now prevents negative values at the type level.
    bool threw = false;
    try { (void)nodo::utils::Amount(-1); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testEmptyStakesAndZeroSupply() {
    // genesis=0, no stakes, treasury=0, rewards=0, burned=0, free=0 => 0==0
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

// ---- Delta sequence audit tests ----

nodo::economics::SupplyDelta noOpDelta(
    std::uint64_t height,
    nodo::utils::Amount supply
) {
    return nodo::economics::SupplyDelta::noOp(
        height,
        "block-hash-" + std::to_string(height),
        1,
        supply
    );
}

nodo::economics::BurnRecord feeBurn(
    std::uint64_t height,
    std::int64_t rawUnits
) {
    return nodo::economics::BurnRecord(
        "fee-burn-" + std::to_string(height),
        height,
        1,
        "nodo_fee_pool",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        "fee burn",
        nodo::economics::BurnType::FEE_BURN
    );
}

nodo::economics::SupplyDelta burnDelta(
    std::uint64_t height,
    nodo::utils::Amount supplyBefore,
    std::int64_t rawUnits
) {
    return nodo::economics::SupplyDelta(
        height,
        "block-hash-" + std::to_string(height),
        1,
        supplyBefore,
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::utils::Amount::fromRawUnits(supplyBefore.rawUnits() - rawUnits),
        {},
        {feeBurn(height, rawUnits)}
    );
}

void testDeltaSequenceEmptyAuditFromPolicy() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, {});
    assert(result.isValid());
    assert(result.deltaCount() == 0);
    assert(result.finalSupply() == nodo::utils::Amount::fromRawUnits(1000));
    assert(result.status() == nodo::economics::SupplyAuditStatus::VALID);
}

void testDeltaSequenceValidSequence() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, nodo::utils::Amount::fromRawUnits(1000)),
        noOpDelta(2, nodo::utils::Amount::fromRawUnits(1000)),
        noOpDelta(3, nodo::utils::Amount::fromRawUnits(1000))
    };
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, deltas);
    assert(result.isValid());
    assert(result.deltaCount() == 3);
    assert(result.finalSupply() == nodo::utils::Amount::fromRawUnits(1000));
}

void testDeltaSequenceContinuityFailureRejected() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    // delta[0] supplyBefore=1000, delta[1] supplyBefore=500 (should be 1000)
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, nodo::utils::Amount::fromRawUnits(1000)),
        noOpDelta(2, nodo::utils::Amount::fromRawUnits(500))  // wrong
    };
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, deltas);
    assert(!result.isValid());
    assert(result.status() == nodo::economics::SupplyAuditStatus::SUPPLY_CONTINUITY_FAILURE);
    assert(result.failedBlockHeight() == 2);
}

void testDeltaSequenceRejectsResetToGenesisAfterBurn() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    const std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, nodo::utils::Amount::fromRawUnits(1000), 20),
        noOpDelta(2, nodo::utils::Amount::fromRawUnits(1000))
    };
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, deltas);
    assert(!result.isValid());
    assert(result.status() == nodo::economics::SupplyAuditStatus::SUPPLY_CONTINUITY_FAILURE);
    assert(result.failedBlockHeight() == 2);
}

void testDeltaSequenceInvalidDeltaRejected() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1",
        nodo::utils::Amount::fromRawUnits(1000)
    );
    // Delta with empty block hash is invalid
    const nodo::economics::SupplyDelta badDelta(
        1, "", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1000),
        {}, {}
    );
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, {badDelta});
    assert(!result.isValid());
    assert(result.status() == nodo::economics::SupplyAuditStatus::INVALID_DELTA);
}

void testDeltaSequenceInvalidPolicyRejected() {
    const nodo::economics::MonetaryPolicy invalidPolicy;  // default-constructed, empty fields
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(invalidPolicy, {});
    assert(!result.isValid());
    assert(result.status() == nodo::economics::SupplyAuditStatus::INVALID_POLICY);
}

void testDeltaSequenceResultSerializes() {
    const auto policy = nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-testnet-1", nodo::utils::Amount::fromRawUnits(500)
    );
    const auto result = nodo::economics::SupplyAudit::auditDeltaSequence(policy, {});
    const std::string s = result.serialize();
    assert(!s.empty());
    assert(s.find("VALID") != std::string::npos);
}

} // namespace

int main() {
    testBalancedSupplyPasses();
    testMismatchedSupplyFails();
    testNegativeGenesisSupplyFails();
    testNegativeTreasuryFails();
    testEmptyStakesAndZeroSupply();
    testReportSerializes();

    testDeltaSequenceEmptyAuditFromPolicy();
    testDeltaSequenceValidSequence();
    testDeltaSequenceContinuityFailureRejected();
    testDeltaSequenceRejectsResetToGenesisAfterBurn();
    testDeltaSequenceInvalidDeltaRejected();
    testDeltaSequenceInvalidPolicyRejected();
    testDeltaSequenceResultSerializes();
    return 0;
}
