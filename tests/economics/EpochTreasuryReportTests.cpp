#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochTreasuryReport;
using nodo::economics::TreasurySpendRecord;
using nodo::utils::Amount;

TreasurySpendRecord validRecord(
    const std::string& id,
    std::int64_t amountRaw,
    std::int64_t balanceBefore
) {
    return TreasurySpendRecord(
        id, "prop-" + id, "recipient-addr",
        Amount::fromRawUnits(amountRaw),
        "fund validator", 10, 1,
        Amount::fromRawUnits(balanceBefore),
        Amount::fromRawUnits(balanceBefore - amountRaw)
    );
}

// Default-constructed report is invalid.
void testDefaultReportInvalid() {
    const EpochTreasuryReport r;
    assert(!r.isValid());
}

// Report from empty spend record list is valid with zero total.
void testEmptySpendRecordListIsValid() {
    const std::vector<TreasurySpendRecord> empty;
    const auto r = EpochTreasuryReport::fromSpendRecords(0, empty);
    assert(r.isValid());
    assert(r.spendRecordCount() == 0);
    assert(r.treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(r.epoch() == 0);
}

// Report counts spend records correctly.
void testReportCountsSpendRecords() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 1000, 10000));
    records.push_back(validRecord("spend-002", 2000,  9000));
    records.push_back(validRecord("spend-003", 3000,  7000));

    const auto r = EpochTreasuryReport::fromSpendRecords(1, records);
    assert(r.isValid());
    assert(r.spendRecordCount() == 3);
    assert(r.epoch() == 1);
}

// Report sums treasury spend amounts correctly.
void testReportSumsTreasurySpendAmount() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 4000, 100000));
    records.push_back(validRecord("spend-002", 6000,  96000));

    const auto r = EpochTreasuryReport::fromSpendRecords(0, records);
    assert(r.isValid());
    assert(r.treasurySpendTotal() == Amount::fromRawUnits(10000));
}

// fromStoredFields reconstructs a valid report.
void testFromStoredFieldsIsValid() {
    const auto r = EpochTreasuryReport::fromStoredFields(
        2, Amount::fromRawUnits(15000), 3
    );
    assert(r.isValid());
    assert(r.epoch() == 2);
    assert(r.treasurySpendTotal() == Amount::fromRawUnits(15000));
    assert(r.spendRecordCount() == 3);
}

// Persisted report mismatch on treasurySpendTotal is detectable by comparison.
void testPersistedMismatchOnTotalDetectable() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));

    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, records);
    assert(rebuilt.isValid());

    // Persisted report claims a different total.
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        0, Amount::fromRawUnits(99999), 1  // wrong total
    );
    assert(persisted.isValid());

    // The caller can detect the mismatch by comparing fields.
    assert(rebuilt.treasurySpendTotal() != persisted.treasurySpendTotal());
}

// Persisted report mismatch on spendRecordCount is detectable by comparison.
void testPersistedMismatchOnCountDetectable() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));

    const auto rebuilt = EpochTreasuryReport::fromSpendRecords(0, records);
    assert(rebuilt.isValid());

    // Persisted report claims a different count.
    const auto persisted = EpochTreasuryReport::fromStoredFields(
        0, Amount::fromRawUnits(5000), 2  // wrong count
    );
    assert(persisted.isValid());

    assert(rebuilt.spendRecordCount() != persisted.spendRecordCount());
}

// Invalid spend record in sequence causes fromSpendRecords to reject.
void testInvalidSpendRecordInSequenceRejected() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));
    records.emplace_back();  // default-constructed — invalid

    const auto r = EpochTreasuryReport::fromSpendRecords(0, records);
    assert(!r.isValid());
}

} // namespace

int main() {
    testDefaultReportInvalid();
    testEmptySpendRecordListIsValid();
    testReportCountsSpendRecords();
    testReportSumsTreasurySpendAmount();
    testFromStoredFieldsIsValid();
    testPersistedMismatchOnTotalDetectable();
    testPersistedMismatchOnCountDetectable();
    testInvalidSpendRecordInSequenceRejected();
    return 0;
}
