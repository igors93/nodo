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

nodo::utils::Amount initialSupply() {
    return testPolicy().initialSupply();
}

nodo::economics::SupplyDelta noOpDelta(
    std::uint64_t blockHeight,
    const std::string& hash,
    std::uint64_t epoch,
    nodo::utils::Amount supply
) {
    return nodo::economics::SupplyDelta::noOp(blockHeight, hash, epoch, supply);
}

nodo::economics::MintRecord makeMint(
    const std::string& id,
    std::int64_t rawUnits,
    std::uint64_t blockHeight,
    const std::string& blockHash,
    std::uint64_t epoch
) {
    return nodo::economics::MintRecord(
        id, "epoch-report-auth", "nodo_epoch_report_recipient",
        nodo::utils::Amount::fromRawUnits(rawUnits),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        epoch, blockHeight, blockHash, 1900000000
    );
}

nodo::economics::SupplyDelta mintDelta(
    std::uint64_t blockHeight,
    const std::string& hash,
    std::uint64_t epoch,
    nodo::utils::Amount supplyBefore,
    std::int64_t mintRaw
) {
    return nodo::economics::SupplyDelta(
        blockHeight, hash, epoch,
        supplyBefore,
        nodo::utils::Amount::fromRawUnits(mintRaw),
        nodo::utils::Amount::fromRawUnits(0),
        nodo::utils::Amount::fromRawUnits(supplyBefore.rawUnits() + mintRaw),
        {makeMint("mint-" + hash, mintRaw, blockHeight, hash, epoch)},
        {}
    );
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
    assert(report.startingSupply() == initialSupply());
    assert(report.endingSupply() == initialSupply());
    assert(report.mintRecordCount() == 0);
    assert(report.burnRecordCount() == 0);
}

void testReportFromNoOpDeltas() {
    const auto supply = initialSupply();
    std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, "hash-noop-1", 0, supply),
        noOpDelta(2, "hash-noop-2", 0, supply)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 2, deltas
    );
    assert(report.isValid());
    assert(report.deltaCount() == 2);
    assert(report.startingSupply() == supply);
    assert(report.endingSupply() == supply);
}

void testReportSumsBurnedAmount() {
    const auto s0 = initialSupply();
    std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, "hash-1", 0, s0, 50),
        burnDelta(2, "hash-2", 0, nodo::utils::Amount::fromRawUnits(s0.rawUnits() - 50), 30)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 2, deltas
    );
    assert(report.isValid());
    assert(report.totalBurned() == nodo::utils::Amount::fromRawUnits(80));
    assert(report.totalMinted() == nodo::utils::Amount::fromRawUnits(0));
    assert(report.burnRecordCount() == 2);
    assert(report.mintRecordCount() == 0);
}

void testReportSumsMintedAmount() {
    const auto s0 = initialSupply();
    std::vector<nodo::economics::SupplyDelta> deltas = {
        mintDelta(1, "hash-mint-1", 0, s0, 40),
        mintDelta(2, "hash-mint-2", 0, nodo::utils::Amount::fromRawUnits(s0.rawUnits() + 40), 60)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 2, deltas
    );
    assert(report.isValid());
    assert(report.totalMinted() == nodo::utils::Amount::fromRawUnits(100));
    assert(report.totalBurned() == nodo::utils::Amount::fromRawUnits(0));
    assert(report.mintRecordCount() == 2);
    assert(report.burnRecordCount() == 0);
}

void testReportEndingSupplyEqualsLastDeltaSupplyAfter() {
    const auto s0 = initialSupply();
    std::vector<nodo::economics::SupplyDelta> deltas = {
        burnDelta(1, "hash-A", 0, s0, 20)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 1, deltas
    );
    assert(report.isValid());
    assert(report.endingSupply() == nodo::utils::Amount::fromRawUnits(s0.rawUnits() - 20));
}

void testReportRejectsContinuityFailure() {
    const auto s0 = initialSupply();
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

void testReportRejectsSequenceNotStartingAtPolicySupply() {
    const auto wrongSupply = nodo::utils::Amount::fromRawUnits(1000);
    std::vector<nodo::economics::SupplyDelta> deltas = {
        noOpDelta(1, "hash-reset", 0, wrongSupply)
    };
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 0, 1, 1, deltas
    );
    assert(!report.isValid());
    assert(report.rejectionReason().find("expected supplyBefore") != std::string::npos);
}

void testReportSerializationIncludesKeyFields() {
    const auto report = nodo::economics::EpochMonetaryReport::fromDeltas(
        testPolicy(), 1, 5, 10, {}
    );
    const std::string s = report.serialize();
    assert(!s.empty());
    assert(s.find("epoch=1") != std::string::npos);
    assert(s.find("deltaCount=0") != std::string::npos);
    assert(s.find("mintRecordCount=0") != std::string::npos);
    assert(s.find("burnRecordCount=0") != std::string::npos);
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
    testReportFromNoOpDeltas();
    testReportSumsBurnedAmount();
    testReportSumsMintedAmount();
    testReportEndingSupplyEqualsLastDeltaSupplyAfter();
    testReportRejectsContinuityFailure();
    testReportRejectsSequenceNotStartingAtPolicySupply();
    testReportSerializationIncludesKeyFields();
    testReportRejectsInvalidPolicy();
    return 0;
}
