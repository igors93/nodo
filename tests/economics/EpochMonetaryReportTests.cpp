#include "economics/EpochMonetaryReport.hpp"
#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

nodo::economics::MonetaryPolicy testPolicy() {
    return nodo::economics::MonetaryPolicy::localnetDefault(
        "nodo-epoch-report-test", nodo::utils::Amount::fromRawUnits(100000)
    );
}

nodo::economics::SupplyDelta noOpDelta(
    std::uint64_t blockHeight,
    const std::string& hash,
    std::uint64_t epoch,
    nodo::utils::Amount supply
) {
    return nodo::economics::SupplyDelta::noOp(blockHeight, hash, epoch, supply);
}

nodo::economics::BurnRecord makeBurn(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t blockHeight,
    std::uint64_t epoch
) {
    return nodo::economics::BurnRecord(
        id, blockHeight, epoch, "nodo_fee_pool",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        "fee burn", nodo::economics::BurnType::FEE_BURN
    );
}

nodo::economics::SupplyDelta burnDelta(
    std::uint64_t blockHeight,
    const std::string& hash,
    std::uint64_t epoch,
    nodo::utils::Amount supplyBefore,
    std::int64_t burnRaw
) {
    return nodo::economics::SupplyDelta(
        blockHeight, hash, epoch,
        supplyBefore,
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(burnRaw),
        nodo::utils::Amount::fromRawUnits(supplyBefore.rawUnits() - burnRaw),
        {},
        {makeBurn("burn-" + hash, burnRaw, blockHeight, epoch)}
    );
}

void testReportFromEmptySequence() {
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 0, 10, {}
    );
    assert(report.isValid());
    assert(report.deltaCount() == 0);
    assert(report.totalMinted() == nodo::utils::Amount::fromRawUnits(0));
    assert(report.totalBurned() == nodo::utils::Amount::fromRawUnits(0));
}

void testReportSumsBurnedAmount() {
    // Two burn deltas: 50 + 30 = 80 burned total.
    const auto s0 = nodo::utils::Amount::fromRawUnits(1000);
    std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, "hash-1", 0, s0, 50),
        burnDelta(2, "hash-2", 0, nodo::utils::Amount::fromRawUnits(950), 30)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 2, deltas
    );
    assert(report.isValid());
    assert(report.totalBurned() == nodo::utils::Amount::fromRawUnits(80));
    assert(report.totalMinted() == nodo::utils::Amount::fromRawUnits(0));
}

void testReportEndingSupplyEqualsLastDeltaSupplyAfter() {
    const auto s0 = nodo::utils::Amount::fromRawUnits(1000);
    std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, "hash-A", 0, s0, 20)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 1, deltas
    );
    assert(report.isValid());
    assert(report.endingSupply() == nodo::utils::Amount::fromRawUnits(980));
}

void testReportRejectsContinuityFailure() {
    // Second delta has wrong supplyBefore.
    const auto s0 = nodo::utils::Amount::fromRawUnits(1000);
    std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, "hash-B", 0, s0, 20),
        burnDelta(2, "hash-C", 0, s0, 10)  // wrong: should be 980
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 2, deltas
    );
    assert(!report.isValid());
    assert(report.rejectionReason().find("continuity") != std::string::npos);
}

void testReportSerializationIncludesKeyFields() {
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 1, 5, 10, {}
    );
    const std::string s = report.serialize();
    assert(!s.empty());
    assert(s.find("epoch=1") != std::string::npos);
    assert(s.find("deltaCount=0") != std::string::npos);
}

void testReportRejectsInvalidPolicy() {
    const nodo::economics::MonetaryPolicy badPolicy;
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        badPolicy, 0, 0, 0, {}
    );
    assert(!report.isValid());
}

} // namespace

int main() {
    testReportFromEmptySequence();
    testReportSumsBurnedAmount();
    testReportEndingSupplyEqualsLastDeltaSupplyAfter();
    testReportRejectsContinuityFailure();
    testReportSerializationIncludesKeyFields();
    testReportRejectsInvalidPolicy();
    return 0;
}
