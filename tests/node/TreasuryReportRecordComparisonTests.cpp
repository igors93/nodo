#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "node/TreasuryReportDeriver.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochTreasuryReport;
using nodo::economics::TreasurySpendRecord;
using nodo::node::EpochTreasuryReportVerifier;
using nodo::node::TreasuryReportDeriver;
using nodo::utils::Amount;

TreasurySpendRecord makeRecord(
    const std::string& spendId,
    const std::string& proposalId,
    const std::string& recipient,
    std::int64_t amount,
    std::uint64_t block = 10
) {
    return TreasurySpendRecord(
        spendId, proposalId, recipient,
        Amount::fromRawUnits(amount), "purpose",
        block, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(100000 - amount)
    );
}

// Test 16: Treasury report comparison catches same total with different recipient.
void testSameTotalDifferentRecipientFails() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-001", "recipient-B", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    // Same total, same count, but different recipient.
    assert(reportA.treasurySpendTotal() == reportB.treasurySpendTotal());
    assert(reportA.spendRecordCount() == reportB.spendRecordCount());

    const auto result = EpochTreasuryReportVerifier::verify(reportA, reportB);
    assert(!result.matched());
    // Same total, same count, but recipient-A vs recipient-B must fail.
    // Either the digest check or the record-level check catches this.
    assert(!result.reason().empty());

    // verifyConsistency should also catch it.
    assert(!TreasuryReportDeriver::verifyConsistency(reportA, reportB));
}

// Test 17: Treasury report comparison catches same total with different approval/proposalId.
void testSameTotalDifferentProposalIdFails() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-002", "recipient", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    const auto result = EpochTreasuryReportVerifier::verify(reportA, reportB);
    assert(!result.matched());
    assert(!result.reason().empty());
}

// Test 18: Treasury report comparison catches duplicate spend identifiers.
void testDuplicateSpendIdFails() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 25000, 10);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-002", "recipient-B", 25000, 11);
    // Both have the same spendId — this is a duplicate.

    const EpochTreasuryReport persisted = EpochTreasuryReport::fromSpendRecords(0, {r1, r2});
    const EpochTreasuryReport rebuilt   = EpochTreasuryReport::fromSpendRecords(0, {r1, r2});

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
    assert(result.reason().find("duplicate") != std::string::npos ||
           result.reason().find("spend") != std::string::npos);
}

// Identical reports match.
void testIdenticalReportsMatch() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const EpochTreasuryReport report = EpochTreasuryReport::fromSpendRecords(0, {r1});

    const auto result = EpochTreasuryReportVerifier::verify(report, report);
    assert(result.matched());
}

// Different spendId in same position fails.
void testDifferentSpendIdFails() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-002", "prop-001", "recipient", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    const auto result = EpochTreasuryReportVerifier::verify(reportA, reportB);
    assert(!result.matched());
    assert(!result.reason().empty());
}

// fromSpendRecords has records; fromStoredFields does not.
void testHasSpendRecordsDistinguishesSource() {
    const EpochTreasuryReport fromRecords =
        EpochTreasuryReport::fromSpendRecords(0, {});
    assert(fromRecords.hasSpendRecords());

    const EpochTreasuryReport fromFields =
        EpochTreasuryReport::fromStoredFields(0, Amount::fromRawUnits(0), 0);
    assert(!fromFields.hasSpendRecords());
}

// Epoch mismatch still fails even with records.
void testEpochMismatchFails() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const EpochTreasuryReport reportEpoch0 = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportEpoch1 = EpochTreasuryReport::fromSpendRecords(1, {r1});

    const auto result = EpochTreasuryReportVerifier::verify(reportEpoch0, reportEpoch1);
    assert(!result.matched());
    assert(result.reason().find("epoch") != std::string::npos);
}

} // namespace

int main() {
    testSameTotalDifferentRecipientFails();
    testSameTotalDifferentProposalIdFails();
    testDuplicateSpendIdFails();
    testIdenticalReportsMatch();
    testDifferentSpendIdFails();
    testHasSpendRecordsDistinguishesSource();
    testEpochMismatchFails();
    return 0;
}
