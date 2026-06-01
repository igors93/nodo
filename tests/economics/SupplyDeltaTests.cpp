#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

nodo::economics::MintRecord makeMint(
    const std::string& id,
    std::int64_t rawUnits
) {
    return nodo::economics::MintRecord(
        id,
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 5, "block-hash-test", 1900000001
    );
}

nodo::economics::BurnRecord makeBurn(
    const std::string& id,
    std::int64_t rawUnits
) {
    return nodo::economics::BurnRecord(
        id, 5, 1, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        "fee", nodo::economics::BurnType::FEE_BURN
    );
}

void testNoOpDeltaIsValid() {
    const auto delta = nodo::economics::SupplyDelta::noOp(
        10, "block-hash-A", 1, nodo::utils::Amount::fromRawUnits(1000)
    );
    assert(delta.isValid());
    assert(delta.rejectionReason().empty());
    assert(delta.supplyBefore() == nodo::utils::Amount::fromRawUnits(1000));
    assert(delta.supplyAfter() == nodo::utils::Amount::fromRawUnits(1000));
    assert(delta.mintedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(delta.burnedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(delta.mintRecords().empty());
    assert(delta.burnRecords().empty());
}

void testValidMintDelta() {
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {makeMint("mint-001", 200)},
        {}
    );
    assert(delta.isValid());
    assert(delta.mintRecords().size() == 1);
}

void testValidBurnDelta() {
    const nodo::economics::SupplyDelta delta(
        6, "block-hash-C", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {},
        {makeBurn("burn-001", 50)}
    );
    assert(delta.isValid());
}

void testEmptyBlockHashRejected() {
    const nodo::economics::SupplyDelta delta(
        7, "", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1000),
        {}, {}
    );
    assert(!delta.isValid());
    assert(!delta.rejectionReason().empty());
}

void testMintAmountMismatchRejected() {
    // mintedAmount=200 but mint records sum=100
    const nodo::economics::SupplyDelta delta(
        8, "block-hash-D", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {makeMint("mint-002", 100)},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("mint") != std::string::npos);
}

void testBurnAmountMismatchRejected() {
    // burnedAmount=100 but burn records sum=50
    const nodo::economics::SupplyDelta delta(
        9, "block-hash-E", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(900),
        {},
        {makeBurn("burn-002", 50)}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("burn") != std::string::npos);
}

void testSupplyAfterMismatchRejected() {
    // supplyBefore=1000, minted=100, burned=0, but supplyAfter=999
    const nodo::economics::SupplyDelta delta(
        10, "block-hash-F", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(999),
        {makeMint("mint-003", 100)},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("equation") != std::string::npos);
}

void testUnderflowBurnedExceedsAvailableSupply() {
    // supplyBefore=100, minted=0, burned=200 → would go negative
    // The SupplyDelta itself will catch the underflow in rejectionReason
    const nodo::economics::SupplyDelta delta(
        11, "block-hash-G", 1,
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount(-100),  // intentionally negative to test rejection
        {},
        {makeBurn("burn-003", 200)}
    );
    assert(!delta.isValid());
}

void testInvalidMintRecordCausesDeltaRejection() {
    // Mint record with empty id is invalid
    const nodo::economics::MintRecord badMint(
        "", "nodo1r", nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 5, "block-hash", 1900000001
    );
    const nodo::economics::SupplyDelta delta(
        12, "block-hash-H", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {badMint}, {}
    );
    assert(!delta.isValid());
}

void testInvalidBurnRecordCausesDeltaRejection() {
    const nodo::economics::BurnRecord badBurn(
        "", 5, 1, "nodo1s",
        nodo::utils::Amount::fromRawUnits(50),
        "fee", nodo::economics::BurnType::FEE_BURN
    );
    const nodo::economics::SupplyDelta delta(
        13, "block-hash-I", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {}, {badBurn}
    );
    assert(!delta.isValid());
}

void testDeltaSerializationContainsKeyFields() {
    const auto delta = nodo::economics::SupplyDelta::noOp(
        100, "block-xyz", 5, nodo::utils::Amount::fromRawUnits(2500)
    );
    const std::string s = delta.serialize();
    assert(!s.empty());
    assert(s.find("block-xyz") != std::string::npos);
    assert(s.find("2500") != std::string::npos);
    assert(s.find("100") != std::string::npos);
}

} // namespace

int main() {
    testNoOpDeltaIsValid();
    testValidMintDelta();
    testValidBurnDelta();
    testEmptyBlockHashRejected();
    testMintAmountMismatchRejected();
    testBurnAmountMismatchRejected();
    testSupplyAfterMismatchRejected();
    testUnderflowBurnedExceedsAvailableSupply();
    testInvalidMintRecordCausesDeltaRejection();
    testInvalidBurnRecordCausesDeltaRejection();
    testDeltaSerializationContainsKeyFields();
    return 0;
}
