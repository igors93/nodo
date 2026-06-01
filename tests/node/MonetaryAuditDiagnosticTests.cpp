#include "node/MonetaryAuditDiagnostic.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::node::MonetaryAuditDiagnostic;
using nodo::node::MonetaryAuditDiagnosticStatus;
using nodo::utils::Amount;

void testOkDiagnosticIsOk() {
    const auto d = MonetaryAuditDiagnostic::ok();
    assert(d.isOk());
    assert(d.status() == MonetaryAuditDiagnosticStatus::OK);
    assert(d.reason().empty());
}

// Continuity failure captures failed block height and expected/actual supply.
void testContinuityFailureCapturesBlockAndSupply() {
    const Amount expected = Amount::fromRawUnits(1000);
    const Amount actual   = Amount::fromRawUnits(900);
    const Amount latest   = Amount::fromRawUnits(800);

    const auto d = MonetaryAuditDiagnostic::supplyContinuityFailure(
        "supply mismatch at block 5", 5, expected, actual, latest
    );

    assert(!d.isOk());
    assert(d.status() == MonetaryAuditDiagnosticStatus::SUPPLY_CONTINUITY_FAILURE);
    assert(d.failedBlockHeight() == 5);
    assert(d.expectedSupply() == expected);
    assert(d.actualSupply()   == actual);
    assert(d.latestValidSupply() == latest);
    assert(!d.reason().empty());
}

// Report mismatch captures epoch and expected/actual ending supply.
void testReportMismatchCapturesEpochAndSupply() {
    const Amount expected = Amount::fromRawUnits(5000);
    const Amount actual   = Amount::fromRawUnits(4000);

    const auto d = MonetaryAuditDiagnostic::reportMismatch(
        "report ending supply differs", 2, expected, actual
    );

    assert(!d.isOk());
    assert(d.status() == MonetaryAuditDiagnosticStatus::REPORT_MISMATCH);
    assert(d.reportEpoch() == 2);
    assert(d.reportExpectedEndingSupply() == expected);
    assert(d.reportActualEndingSupply()   == actual);
}

// Report missing diagnostic.
void testReportMissingDiagnostic() {
    const auto d = MonetaryAuditDiagnostic::reportMissing(
        "expected epoch report not found", 3
    );

    assert(!d.isOk());
    assert(d.status() == MonetaryAuditDiagnosticStatus::REPORT_MISSING);
    assert(d.reportEpoch() == 3);
    assert(!d.reason().empty());
}

// Serialization includes key fields.
void testSerializationIncludesBlockHeightAndSupply() {
    const auto d = MonetaryAuditDiagnostic::supplyContinuityFailure(
        "test", 7, Amount::fromRawUnits(111), Amount::fromRawUnits(222), Amount::fromRawUnits(333)
    );
    const std::string s = d.serialize();
    assert(s.find("failedBlockHeight=7") != std::string::npos);
    assert(s.find("expectedSupplyRaw=111") != std::string::npos);
    assert(s.find("actualSupplyRaw=222") != std::string::npos);
    assert(s.find("SUPPLY_CONTINUITY_FAILURE") != std::string::npos);
}

void testSerializationIncludesReportFields() {
    const auto d = MonetaryAuditDiagnostic::reportMismatch(
        "mismatch", 1, Amount::fromRawUnits(500), Amount::fromRawUnits(499)
    );
    const std::string s = d.serialize();
    assert(s.find("REPORT_MISMATCH") != std::string::npos);
    assert(s.find("reportEpoch=1") != std::string::npos);
    assert(s.find("reportExpectedEndingSupplyRaw=500") != std::string::npos);
    assert(s.find("reportActualEndingSupplyRaw=499") != std::string::npos);
}

} // namespace

int main() {
    testOkDiagnosticIsOk();
    testContinuityFailureCapturesBlockAndSupply();
    testReportMismatchCapturesEpochAndSupply();
    testReportMissingDiagnostic();
    testSerializationIncludesBlockHeightAndSupply();
    testSerializationIncludesReportFields();
    return 0;
}
