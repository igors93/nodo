#include "node/FinalizedTreasurySection.hpp"
#include "node/FinalizedTreasurySectionCodec.hpp"
#include "node/FinalizedTreasurySectionValidator.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::TreasurySpendRecord;
using nodo::node::FinalizedTreasurySection;
using nodo::node::FinalizedTreasurySectionCodec;
using nodo::node::FinalizedTreasurySectionValidator;
using nodo::node::TreasurySectionValidationStatus;
using nodo::utils::Amount;

TreasurySpendRecord validRecord(
    const std::string& id,
    std::int64_t amount,
    std::int64_t balanceBefore
) {
    return TreasurySpendRecord(
        id, "prop-" + id, "recipient-addr",
        Amount::fromRawUnits(amount),
        "fund validator", 10, 0,
        Amount::fromRawUnits(balanceBefore),
        Amount::fromRawUnits(balanceBefore - amount)
    );
}

// Empty section is valid (no treasury spends in this block).
void testEmptySectionIsValid() {
    const FinalizedTreasurySection section;
    assert(section.isValid());
    assert(section.spendRecordCount() == 0);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(0));
}

// Section with one valid spend record is valid.
void testSectionWithOneRecordIsValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));

    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 1);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(5000));
}

// Section with multiple valid spend records sums correctly.
void testSectionWithMultipleRecordsIsValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 3000, 100000));
    records.push_back(validRecord("spend-002", 7000, 97000));

    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 2);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(10000));
}

// Section with invalid spend record is rejected.
void testSectionWithInvalidRecordRejected() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 100000));
    records.emplace_back();  // default-constructed — invalid

    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());
    assert(section.rejectionReason().find("invalid") != std::string::npos);
}

// Codec round-trips an empty section.
void testCodecRoundTripsEmptySection() {
    const FinalizedTreasurySection original;
    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);

    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 0);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(0));
}

// Codec round-trips a section with spend records.
void testCodecRoundTripsWithRecords() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 4000, 100000));
    records.push_back(validRecord("spend-002", 6000, 96000));

    const FinalizedTreasurySection original(std::move(records));
    assert(original.isValid());

    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);

    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 2);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(10000));
    assert(decoded.spendRecords()[0].spendId() == "spend-001");
    assert(decoded.spendRecords()[1].spendId() == "spend-002");
    assert(decoded.spendRecords()[0].amount() == Amount::fromRawUnits(4000));
}

// Validator passes for valid section.
void testValidatorPassesForValidSection() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 1000, 50000));

    const FinalizedTreasurySection section(std::move(records));
    const auto result = FinalizedTreasurySectionValidator::validate(section);

    assert(result.passed());
    assert(result.status() == TreasurySectionValidationStatus::VALID);
}

// Validator fails for invalid section.
void testValidatorFailsForInvalidSection() {
    std::vector<TreasurySpendRecord> records;
    records.emplace_back();  // invalid

    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());

    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(!result.passed());
    assert(result.status() == TreasurySectionValidationStatus::INVALID_SECTION);
}

} // namespace

int main() {
    testEmptySectionIsValid();
    testSectionWithOneRecordIsValid();
    testSectionWithMultipleRecordsIsValid();
    testSectionWithInvalidRecordRejected();
    testCodecRoundTripsEmptySection();
    testCodecRoundTripsWithRecords();
    testValidatorPassesForValidSection();
    testValidatorFailsForInvalidSection();
    return 0;
}
