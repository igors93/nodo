#include "economics/SupplyDeltaBuilder.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

nodo::economics::MintRecord makeMint(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t epoch,
    std::uint64_t blockHeight,
    const std::string& blockHash
) {
    return nodo::economics::MintRecord(
        id, "auth-builder-001", "nodo1recipient001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        epoch, blockHeight, blockHash, 1900000001
    );
}

nodo::economics::BurnRecord makeBurn(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t blockHeight,
    std::uint64_t epoch
) {
    return nodo::economics::BurnRecord(
        id, blockHeight, epoch, "nodo1sender001",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        "fee", nodo::economics::BurnType::FEE_BURN
    );
}

void testBuilderComputesMintedAmount() {
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        10, "build-hash-A", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        {
            makeMint("m1", 300, 1, 10, "build-hash-A"),
            makeMint("m2", 200, 1, 10, "build-hash-A")
        },
        {}
    );
    assert(delta.mintedAmount() == nodo::utils::Amount::fromRawUnits(500));
}

void testBuilderComputesBurnedAmount() {
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        10, "build-hash-B", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        {},
        {
            makeBurn("b1", 80, 10, 1),
            makeBurn("b2", 20, 10, 1)
        }
    );
    assert(delta.burnedAmount() == nodo::utils::Amount::fromRawUnits(100));
}

void testBuilderComputesSupplyAfter() {
    // supplyBefore=1000, minted=500, burned=100 → supplyAfter=1400
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        10, "build-hash-C", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        {makeMint("m1", 500, 1, 10, "build-hash-C")},
        {makeBurn("b1", 100, 10, 1)}
    );
    assert(delta.supplyAfter() == nodo::utils::Amount::fromRawUnits(1400));
}

void testBuilderCreatedDeltaIsValidWhenRecordsMatch() {
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        7, "build-hash-D", 2,
        nodo::utils::Amount::fromRawUnits(2000),
        {makeMint("m1", 100, 2, 7, "build-hash-D")},
        {makeBurn("b1", 50, 7, 2)}
    );
    assert(delta.isValid());
    assert(delta.rejectionReason().empty());
    assert(delta.mintedAmount() == nodo::utils::Amount::fromRawUnits(100));
    assert(delta.burnedAmount() == nodo::utils::Amount::fromRawUnits(50));
    assert(delta.supplyAfter() == nodo::utils::Amount::fromRawUnits(2050));
}

void testBuilderWithNoRecordsIsNoOp() {
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        3, "build-hash-E", 1,
        nodo::utils::Amount::fromRawUnits(500),
        {}, {}
    );
    assert(delta.isValid());
    assert(delta.mintedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(delta.burnedAmount() == nodo::utils::Amount::fromRawUnits(0));
    assert(delta.supplyAfter() == nodo::utils::Amount::fromRawUnits(500));
}

void testBuilderRejectsBurnExceedingAvailableSupply() {
    // supplyBefore=100, minted=0, burned=200 → underflow must throw.
    bool threw = false;
    try {
        (void)nodo::economics::SupplyDeltaBuilder::build(
            5, "build-hash-underflow", 1,
            nodo::utils::Amount::fromRawUnits(100),
            {},
            {makeBurn("b-overflow", 200, 5, 1)}
        );
    } catch (const std::underflow_error& e) {
        threw = true;
        const std::string msg(e.what());
        assert(msg.find("exceeds available supply") != std::string::npos ||
               msg.find("underflow") != std::string::npos);
    }
    assert(threw);
}

void testBuilderRejectsBurnExceedingSupplyPlusMint() {
    // supplyBefore=100, minted=50 → available=150, but burned=200 → underflow.
    bool threw = false;
    try {
        (void)nodo::economics::SupplyDeltaBuilder::build(
            5, "build-hash-uf2", 1,
            nodo::utils::Amount::fromRawUnits(100),
            {makeMint("m1", 50, 1, 5, "build-hash-uf2")},
            {makeBurn("b1", 200, 5, 1)}
        );
    } catch (const std::underflow_error&) {
        threw = true;
    }
    assert(threw);
}

void testBuilderDeltaIsInvalidWhenRecordsMismatchBlock() {
    // Mint has wrong blockHash — delta consistency check fires.
    const auto delta = nodo::economics::SupplyDeltaBuilder::build(
        10, "build-hash-F", 1,
        nodo::utils::Amount::fromRawUnits(1000),
        {makeMint("m1", 100, 1, 10, "WRONG-HASH")},
        {}
    );
    // Builder computes arithmetic correctly but SupplyDelta.isValid() catches
    // the sourceBlockHash mismatch.
    assert(!delta.isValid());
    assert(delta.rejectionReason().find("sourceBlockHash") != std::string::npos);
}

} // namespace

int main() {
    testBuilderComputesMintedAmount();
    testBuilderComputesBurnedAmount();
    testBuilderComputesSupplyAfter();
    testBuilderCreatedDeltaIsValidWhenRecordsMatch();
    testBuilderWithNoRecordsIsNoOp();
    testBuilderDeltaIsInvalidWhenRecordsMismatchBlock();
    testBuilderRejectsBurnExceedingAvailableSupply();
    testBuilderRejectsBurnExceedingSupplyPlusMint();
    return 0;
}
