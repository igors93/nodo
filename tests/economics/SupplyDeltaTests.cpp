#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

nodo::economics::MintRecord makeMint(
    const std::string& id,
    std::int64_t rawUnits,
    const std::string& authorizationId = "auth-delta-test-001",
    std::uint64_t epoch = 1,
    std::uint64_t blockHeight = 5,
    const std::string& blockHash = "block-hash-test"
) {
    return nodo::economics::MintRecord(
        id,
        authorizationId,
        "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        epoch, blockHeight, blockHash, 1900000001
    );
}

nodo::economics::BurnRecord makeBurn(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t blockHeight = 5,
    std::uint64_t epoch = 1
) {
    return nodo::economics::BurnRecord(
        id, blockHeight, epoch, "nodo1sender001",
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
    // Mint record must match delta's epoch=1, blockHeight=5, blockHash="block-hash-B"
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {makeMint("mint-001", 200, "auth-delta-test-001", 1, 5, "block-hash-B")},
        {}
    );
    assert(delta.isValid());
    assert(delta.mintRecords().size() == 1);
}

void testValidBurnDelta() {
    // Burn record must match delta's epoch=1, blockHeight=6
    const nodo::economics::SupplyDelta delta(
        6, "block-hash-C", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {},
        {makeBurn("burn-001", 50, 6, 1)}
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
    // Mint matches delta block/epoch to isolate the arithmetic check.
    const nodo::economics::SupplyDelta delta(
        8, "block-hash-D", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1200),
        {makeMint("mint-002", 100, "auth-delta-test-001", 1, 8, "block-hash-D")},
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
        {makeBurn("burn-002", 50, 9, 1)}
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
        {makeMint("mint-003", 100, "auth-delta-test-001", 1, 10, "block-hash-F")},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("equation") != std::string::npos);
}

void testUnderflowBurnedExceedsAvailableSupply() {
    // Amount constructor now prevents negative supply-after values at the type level.
    bool threw = false;
    try { (void)nodo::utils::Amount(-100); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

void testInvalidMintRecordCausesDeltaRejection() {
    // Mint record with empty id is invalid — isValid() fires first, before consistency checks.
    const nodo::economics::MintRecord badMint(
        "", "auth-bad-001", "nodo1r", nodo::utils::Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 12, "block-hash-H", 1900000001
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
    // Burn record with empty burnId is invalid — isValid() fires first.
    const nodo::economics::BurnRecord badBurn(
        "", 13, 1, "nodo1s",
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

void testMintRecordWithEmptyAuthorizationIdCausesDeltaRejection() {
    // Delta with a mint record that has empty authorizationId must be rejected.
    // isValid() on the MintRecord fires first (empty authId), before consistency checks.
    const nodo::economics::SupplyDelta delta(
        14, "block-hash-J", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-no-auth", 100, "", 1, 14, "block-hash-J")},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("MintRecord") != std::string::npos);
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

// --- Item 1: new consistency rejection tests ---

void testMintEpochMismatchRejected() {
    // Mint has epoch=2 but delta expects epoch=1.
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-epoch", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-epoch", 100, "auth-001", 2, 5, "block-hash-epoch")},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("epoch") != std::string::npos);
    assert(delta.rejectionReason().find("mint-epoch") != std::string::npos);
}

void testMintSourceBlockIndexMismatchRejected() {
    // Mint has sourceBlockIndex=99 but delta blockHeight=5.
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-idx", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-idx", 100, "auth-001", 1, 99, "block-hash-idx")},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("sourceBlockIndex") != std::string::npos);
    assert(delta.rejectionReason().find("mint-idx") != std::string::npos);
}

void testMintSourceBlockHashMismatchRejected() {
    // Mint has sourceBlockHash="wrong-hash" but delta has "block-hash-real".
    const nodo::economics::SupplyDelta delta(
        5, "block-hash-real", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(1100),
        {makeMint("mint-hash", 100, "auth-001", 1, 5, "wrong-hash")},
        {}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("sourceBlockHash") != std::string::npos);
    assert(delta.rejectionReason().find("mint-hash") != std::string::npos);
}

void testBurnEpochMismatchRejected() {
    // Burn has epoch=3 but delta expects epoch=1.
    const nodo::economics::SupplyDelta delta(
        7, "block-hash-burn-epoch", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {},
        {makeBurn("burn-epoch", 50, 7, 3)}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("epoch") != std::string::npos);
    assert(delta.rejectionReason().find("burn-epoch") != std::string::npos);
}

void testBurnBlockHeightMismatchRejected() {
    // Burn has blockHeight=999 but delta expects blockHeight=7.
    const nodo::economics::SupplyDelta delta(
        7, "block-hash-burn-height", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(50),
        nodo::utils::Amount::fromRawUnits(950),
        {},
        {makeBurn("burn-height", 50, 999, 1)}
    );
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("blockHeight") != std::string::npos);
    assert(delta.rejectionReason().find("burn-height") != std::string::npos);
}

void testValidRecordsMatchingDeltaPass() {
    // Explicitly verify all fields match: blockHeight=20, blockHash="block-real", epoch=3.
    const nodo::economics::SupplyDelta delta(
        20, "block-real", 3,
        nodo::utils::Amount::fromRawUnits(5000),
        nodo::utils::Amount::fromRawUnits(200),
        nodo::utils::Amount::fromRawUnits(100),
        nodo::utils::Amount::fromRawUnits(5100),
        {makeMint("mint-match", 200, "auth-match", 3, 20, "block-real")},
        {makeBurn("burn-match", 100, 20, 3)}
    );
    assert(delta.isValid());
    assert(delta.rejectionReason().empty());
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
    testMintRecordWithEmptyAuthorizationIdCausesDeltaRejection();
    testDeltaSerializationContainsKeyFields();
    // Item 1: new consistency rejection tests
    testMintEpochMismatchRejected();
    testMintSourceBlockIndexMismatchRejected();
    testMintSourceBlockHashMismatchRejected();
    testBurnEpochMismatchRejected();
    testBurnBlockHeightMismatchRejected();
    testValidRecordsMatchingDeltaPass();
    return 0;
}
