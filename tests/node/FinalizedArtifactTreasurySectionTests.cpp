#include "node/FinalizedBlockArtifact.hpp"
#include "node/FinalizedTreasurySectionCodec.hpp"
#include "node/FinalizedTreasurySection.hpp"
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

// Default artifact treasury section is empty and valid.
// Proof-of-Protection: no treasury spend without a valid record.
void testDefaultArtifactTreasurySectionIsEmptyAndValid() {
    const FinalizedTreasurySection section;
    assert(section.isValid());
    assert(section.spendRecordCount() == 0);
    assert(section.treasurySpendTotal() == Amount::fromRawUnits(0));
}

// Treasury section with valid spend records is valid.
void testSectionWithValidRecordsIsValid() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 3000, 100000));
    const FinalizedTreasurySection section(std::move(records));
    assert(section.isValid());
    assert(section.spendRecordCount() == 1);
}

// Section with invalid spend record is invalid.
void testSectionWithInvalidRecordIsInvalid() {
    std::vector<TreasurySpendRecord> records;
    records.emplace_back();  // default-constructed, invalid
    const FinalizedTreasurySection section(std::move(records));
    assert(!section.isValid());
}

// Validator passes for empty section.
void testValidatorPassesForEmptySection() {
    const FinalizedTreasurySection section;
    const auto result = FinalizedTreasurySectionValidator::validate(section);
    assert(result.passed());
}

// Standalone codec round-trips non-empty section.
void testStandaloneCodecRoundTrips() {
    std::vector<TreasurySpendRecord> records;
    records.push_back(validRecord("spend-001", 5000, 200000));
    const FinalizedTreasurySection original(std::move(records));
    assert(original.isValid());

    const std::string encoded = FinalizedTreasurySectionCodec::encode(original);
    const FinalizedTreasurySection decoded = FinalizedTreasurySectionCodec::decode(encoded);

    assert(decoded.isValid());
    assert(decoded.spendRecordCount() == 1);
    assert(decoded.treasurySpendTotal() == Amount::fromRawUnits(5000));
    assert(decoded.spendRecords()[0].spendId() == "spend-001");
}

// Standalone codec rejects unexpected extra field.
void testStandaloneCodecRejectsExtraField() {
    bool threw = false;
    try {
        FinalizedTreasurySectionCodec::decode(
            "NODO_FINALIZED_TREASURY_SECTION\nspendRecordCount=0\nunexpected=field\n"
        );
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// Standalone codec rejects invalid spend record (balance mismatch).
void testStandaloneCodecRejectsInvalidRecord() {
    // Manually construct a document with wrong balance arithmetic.
    const std::string badContents =
        "NODO_FINALIZED_TREASURY_SECTION\n"
        "spendRecordCount=1\n"
        "spend.0.spendId=spend-001\n"
        "spend.0.proposalId=prop-001\n"
        "spend.0.recipientAddress=recipient\n"
        "spend.0.amountRawUnits=500\n"
        "spend.0.purpose=test\n"
        "spend.0.executedAtBlock=10\n"
        "spend.0.epoch=0\n"
        "spend.0.balanceBeforeRawUnits=1000\n"
        "spend.0.balanceAfterRawUnits=999\n";  // wrong: should be 500

    bool threw = false;
    try {
        FinalizedTreasurySectionCodec::decode(badContents);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// FinalizedBlockArtifact default-constructed has a valid empty treasury section.
void testFinalizedBlockArtifactHasTreasurySection() {
    const nodo::node::FinalizedBlockArtifact artifact;  // NOLINT: intentional default construct
    assert(artifact.treasurySection().isValid());
    assert(artifact.treasurySection().spendRecordCount() == 0);
}

} // namespace

int main() {
    testDefaultArtifactTreasurySectionIsEmptyAndValid();
    testSectionWithValidRecordsIsValid();
    testSectionWithInvalidRecordIsInvalid();
    testValidatorPassesForEmptySection();
    testStandaloneCodecRoundTrips();
    testStandaloneCodecRejectsExtraField();
    testStandaloneCodecRejectsInvalidRecord();
    testFinalizedBlockArtifactHasTreasurySection();
    return 0;
}
