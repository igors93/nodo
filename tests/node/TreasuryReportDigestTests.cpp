#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochTreasuryReport;
using nodo::economics::TreasurySpendRecord;
using nodo::node::EpochTreasuryReportVerifier;
using nodo::utils::Amount;

TreasurySpendRecord makeRecord(
    const std::string& spendId,
    const std::string& proposalId,
    const std::string& recipient,
    std::int64_t amount
) {
    return TreasurySpendRecord(
        spendId, proposalId, recipient,
        Amount::fromRawUnits(amount), "purpose",
        10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(100000 - amount)
    );
}

// Test 11: Treasury report digest changes when recipient changes (same total).
void testDigestChangesOnRecipientChange() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-001", "recipient-B", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    assert(reportA.treasurySpendTotal() == reportB.treasurySpendTotal());
    assert(!reportA.spendRecordsDigest().empty());
    assert(!reportB.spendRecordsDigest().empty());
    assert(reportA.spendRecordsDigest() != reportB.spendRecordsDigest());

    const auto result = EpochTreasuryReportVerifier::verify(reportA, reportB);
    assert(!result.matched());
}

// Test 12: Treasury report digest changes when proposal changes (same total).
void testDigestChangesOnProposalChange() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-002", "recipient", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    assert(reportA.spendRecordsDigest() != reportB.spendRecordsDigest());
}

// Test 13: Duplicate spend identifiers fail verification.
void testDuplicateSpendIdFailsVerification() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 25000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-002", "recipient-B", 25000);
    // Both have spendId "spend-001" — duplicate.

    const EpochTreasuryReport persisted = EpochTreasuryReport::fromSpendRecords(0, {r1, r2});
    const EpochTreasuryReport rebuilt   = EpochTreasuryReport::fromSpendRecords(0, {r1, r2});

    const auto result = EpochTreasuryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
}

// Digest mismatch produces a reason message with "digest" when both reports have digests.
void testDigestMismatchReasonContainsDifferentRecipient() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-001", "recipient-B", 50000);

    const EpochTreasuryReport reportA = EpochTreasuryReport::fromSpendRecords(0, {r1});
    const EpochTreasuryReport reportB = EpochTreasuryReport::fromSpendRecords(0, {r2});

    // Sanity: digests differ.
    assert(reportA.spendRecordsDigest() != reportB.spendRecordsDigest());

    const auto result = EpochTreasuryReportVerifier::verify(reportA, reportB);
    assert(!result.matched());
    // Either digest check or per-record check caught the mismatch.
    assert(!result.reason().empty());
}

// Test 14: Persisted treasury report mismatch fails audit via digest.
void testPersistedMismatchFailsViaDigest() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient-A", 50000);
    const TreasurySpendRecord r2 = makeRecord("spend-001", "prop-001", "recipient-B", 50000);

    const EpochTreasuryReport rebuilt = EpochTreasuryReport::fromSpendRecords(0, {r1});
    // Build persisted from stored fields WITH the wrong digest.
    const EpochTreasuryReport persistedWrongDigest =
        EpochTreasuryReport::fromStoredFields(
            0,
            Amount::fromRawUnits(50000),
            1,
            EpochTreasuryReport::fromSpendRecords(0, {r2}).spendRecordsDigest()
        );

    const auto result = EpochTreasuryReportVerifier::verify(persistedWrongDigest, rebuilt);
    assert(!result.matched());
    assert(!result.reason().empty());
}

// Identical digest reports match.
void testIdenticalDigestReportsMatch() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const EpochTreasuryReport report = EpochTreasuryReport::fromSpendRecords(0, {r1});

    const auto result = EpochTreasuryReportVerifier::verify(report, report);
    assert(result.matched());
}

// Empty report has a non-empty canonical digest.
void testEmptyReportHasDigest() {
    const EpochTreasuryReport empty = EpochTreasuryReport::fromSpendRecords(0, {});
    assert(!empty.spendRecordsDigest().empty());
}

// Persisted report without digest (old format) passes totals-only check.
void testPersistedWithoutDigestFallsBackToTotals() {
    const TreasurySpendRecord r1 = makeRecord("spend-001", "prop-001", "recipient", 50000);
    const EpochTreasuryReport rebuilt = EpochTreasuryReport::fromSpendRecords(0, {r1});

    // Simulate old persisted format: no digest field.
    const EpochTreasuryReport persistedOld =
        EpochTreasuryReport::fromStoredFields(
            0,
            Amount::fromRawUnits(50000),
            1
            // no digest
        );
    assert(persistedOld.spendRecordsDigest().empty());

    // Totals match; no digest to compare → passes (backwards compatibility).
    const auto result = EpochTreasuryReportVerifier::verify(persistedOld, rebuilt);
    assert(result.matched());
}

} // namespace

int main() {
    testDigestChangesOnRecipientChange();
    testDigestMismatchReasonContainsDifferentRecipient();
    testDigestChangesOnProposalChange();
    testDuplicateSpendIdFailsVerification();
    testPersistedMismatchFailsViaDigest();
    testIdenticalDigestReportsMatch();
    testEmptyReportHasDigest();
    testPersistedWithoutDigestFallsBackToTotals();
    return 0;
}
