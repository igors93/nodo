#include "node/RuntimeMonetaryReportService.hpp"
#include "economics/BurnRecord.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using nodo::economics::BurnRecord;
using nodo::economics::BurnType;
using nodo::economics::MonetaryPolicy;
using nodo::economics::SupplyDelta;
using nodo::node::MonetaryReportServiceStatus;
using nodo::node::RuntimeMonetaryReportService;
using nodo::utils::Amount;

MonetaryPolicy testPolicy() {
    return MonetaryPolicy::localnetDefault(
        "report-service-test", Amount::fromRawUnits(800000)
    );
}

SupplyDelta makeBurnDelta(
    std::uint64_t h,
    const std::string& hash,
    Amount supplyBefore,
    std::int64_t burnRaw
) {
    const BurnRecord burn(
        "burn-" + hash, h, 0, "fee_pool",
        Amount::fromRawUnits(burnRaw),
        "fee burn", BurnType::FEE_BURN
    );
    return SupplyDelta(
        h, hash, 0,
        supplyBefore,
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(burnRaw),
        Amount::fromRawUnits(supplyBefore.rawUnits() - burnRaw),
        {}, {burn}
    );
}

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_report_service_test.txt";
}

// Service persists report from finalized deltas.
void testServicePersistsReport() {
    const auto policy = testPolicy();
    const auto s0 = policy.initialSupply();
    const auto s1 = Amount::fromRawUnits(s0.rawUnits() - 100);
    std::vector<SupplyDelta> deltas = {
        makeBurnDelta(1, "svc-hash-1", s0, 100),
        makeBurnDelta(2, "svc-hash-2", s1, 50)
    };

    const auto path = tempPath();
    const auto result = RuntimeMonetaryReportService::buildAndPersist(
        policy, deltas, 0, path
    );

    assert(result.succeeded());
    assert(result.status() == MonetaryReportServiceStatus::PERSISTED);
    assert(result.report().isValid());
    assert(result.report().deltaCount() == 2);
    assert(std::filesystem::exists(path));

    std::filesystem::remove(path);
}

// Service returns EMPTY_DELTAS for empty input.
void testServiceDoesNotPersistEmptyDeltas() {
    const auto policy = testPolicy();
    const std::vector<SupplyDelta> empty;
    const auto path = tempPath();

    const auto result = RuntimeMonetaryReportService::buildAndPersist(
        policy, empty, 0, path
    );

    assert(!result.succeeded());
    assert(result.status() == MonetaryReportServiceStatus::EMPTY_DELTAS);
    assert(!std::filesystem::exists(path));
}

// Service verifies read-back matches written report.
void testServiceReadbackMatchesWritten() {
    const auto policy = testPolicy();
    const auto s0 = policy.initialSupply();
    std::vector<SupplyDelta> deltas = {
        makeBurnDelta(1, "rb-hash-1", s0, 300)
    };

    const auto path = tempPath();
    const auto result = RuntimeMonetaryReportService::buildAndPersist(
        policy, deltas, 0, path
    );

    assert(result.succeeded());
    assert(result.report().totalBurned() == Amount::fromRawUnits(300));

    std::filesystem::remove(path);
}

// Service with invalid policy returns REPORT_INVALID.
void testServiceRejectsInvalidPolicy() {
    const MonetaryPolicy bad;
    const auto s0 = Amount::fromRawUnits(100000);
    std::vector<SupplyDelta> deltas = {
        makeBurnDelta(1, "bad-hash", s0, 50)
    };
    const auto path = tempPath();

    const auto result = RuntimeMonetaryReportService::buildAndPersist(
        bad, deltas, 0, path
    );

    assert(!result.succeeded());
    // With invalid policy, fromDeltas fails, so we get REPORT_INVALID.
    assert(result.status() == MonetaryReportServiceStatus::REPORT_INVALID);
    assert(!std::filesystem::exists(path));
}

} // namespace

int main() {
    testServicePersistsReport();
    testServiceDoesNotPersistEmptyDeltas();
    testServiceReadbackMatchesWritten();
    testServiceRejectsInvalidPolicy();
    return 0;
}
