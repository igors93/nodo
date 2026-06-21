#include "node/EpochTreasuryReportStore.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochTreasuryReport;
using nodo::economics::TreasurySpendRecord;
using nodo::node::EpochTreasuryReportStore;
using nodo::node::EpochTreasuryReportVerifier;
using nodo::node::EpochTreasuryVerificationStatus;
using nodo::utils::Amount;

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_epoch_treasury_report_test.txt";
}

EpochTreasuryReport zeroReport() {
    return EpochTreasuryReport::fromSpendRecords(0, {});
}

EpochTreasuryReport reportWithTotal(
    std::uint64_t epoch,
    std::int64_t totalRaw,
    std::size_t count
) {
    return EpochTreasuryReport::fromStoredFields(
        epoch, Amount::fromRawUnits(totalRaw), count
    );
}

// Encode/decode round-trip preserves fields.
void testRoundTripPreservesFields() {
    const auto report = reportWithTotal(3, 15000, 2);
    const std::string encoded = EpochTreasuryReportStore::encode(report);
    const auto decoded = EpochTreasuryReportStore::decode(encoded);

    assert(decoded.isValid());
    assert(decoded.epoch() == 3);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(15000));
    assert(decoded.spendRecordCount() == 2);
}

// Zero-spend report round-trips.
void testZeroSpendReportRoundTrips() {
    const auto report = zeroReport();
    const std::string encoded = EpochTreasuryReportStore::encode(report);
    const auto decoded = EpochTreasuryReportStore::decode(encoded);

    assert(decoded.isValid());
    assert(decoded.epoch() == 0);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(decoded.spendRecordCount() == 0);
}

// Write/read round-trip through file system.
void testFileWriteReadRoundTrip() {
    const auto path = tempPath();
    const auto report = reportWithTotal(1, 8000, 1);

    EpochTreasuryReportStore::write(path, report);
    const auto loaded = EpochTreasuryReportStore::read(path);

    assert(loaded.isValid());
    assert(loaded.epoch() == 1);
    assert(loaded.treasurySpendTotal() == Amount::fromRawUnits(8000));

    std::filesystem::remove(path);
}

// V-style schema ID is rejected.
void testVStyleSchemaRejected() {
    bool threw = false;
    try {
        EpochTreasuryReportStore::decode(
            "NODO_EPOCH_TREASURY_REPORT_V1\nepoch=0\n"
        );
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// Missing field is rejected.
void testMissingFieldRejected() {
    bool threw = false;
    try {
        // Only epoch field, missing required fields.
        EpochTreasuryReportStore::decode(
            "NODO_EPOCH_TREASURY_REPORT\nepoch=0\n"
        );
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testPartialNumericEpochRejected() {
    std::string encoded = EpochTreasuryReportStore::encode(zeroReport());
    const std::string target = "epoch=0\n";
    const std::string replacement = "epoch=0abc\n";
    const std::size_t pos = encoded.find(target);
    assert(pos != std::string::npos);
    encoded.replace(pos, target.size(), replacement);

    bool threw = false;
    try {
        (void)EpochTreasuryReportStore::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

void testPartialNumericSpendCountRejected() {
    std::string encoded = EpochTreasuryReportStore::encode(zeroReport());
    const std::string target = "spendRecordCount=0\n";
    const std::string replacement = "spendRecordCount=0abc\n";
    const std::size_t pos = encoded.find(target);
    assert(pos != std::string::npos);
    encoded.replace(pos, target.size(), replacement);

    bool threw = false;
    try {
        (void)EpochTreasuryReportStore::decode(encoded);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// Matching persisted vs rebuilt report passes verification.
void testMatchingReportsPassed() {
    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, {});
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        0, Amount::fromRawUnits(0), 0
    );

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(result.matched());
    assert(result.status() == EpochTreasuryVerificationStatus::MATCH);
}

// Tampered spend total fails verification.
void testTamperedTotalFails() {
    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, {});
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        0, Amount::fromRawUnits(99999), 0  // wrong total
    );

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
    assert(result.status() == EpochTreasuryVerificationStatus::FIELD_MISMATCH);
    assert(result.reason().find("treasurySpendTotal") != std::string::npos);
}

// Tampered spend count fails verification.
void testTamperedCountFails() {
    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, {});
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        0, Amount::fromRawUnits(0), 5  // wrong count
    );

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
    assert(result.status() == EpochTreasuryVerificationStatus::FIELD_MISMATCH);
    assert(result.reason().find("spendRecordCount") != std::string::npos);
}

// Wrong epoch fails verification.
void testWrongEpochFails() {
    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(1, {});
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        2, Amount::fromRawUnits(0), 0  // epoch mismatch
    );

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
    assert(result.status() == EpochTreasuryVerificationStatus::FIELD_MISMATCH);
    assert(result.reason().find("epoch") != std::string::npos);
}

// Invalid persisted report returns PERSISTED_INVALID.
void testInvalidPersistedReturnsPersistedInvalid() {
    const EpochTreasuryReport invalid;
    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, {});

    const auto result = EpochTreasuryReportVerifier::verify(invalid, rebuilt);
    assert(!result.matched());
    assert(result.status() == EpochTreasuryVerificationStatus::PERSISTED_INVALID);
}

} // namespace

int main() {
    testRoundTripPreservesFields();
    testZeroSpendReportRoundTrips();
    testFileWriteReadRoundTrip();
    testVStyleSchemaRejected();
    testMissingFieldRejected();
    testPartialNumericEpochRejected();
    testPartialNumericSpendCountRejected();
    testMatchingReportsPassed();
    testTamperedTotalFails();
    testTamperedCountFails();
    testWrongEpochFails();
    testInvalidPersistedReturnsPersistedInvalid();
    return 0;
}
