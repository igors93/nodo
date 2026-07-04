// P1 tests: Treasury report is empty only when artifact has no spends.
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

// Test 25: Empty spend records produce a valid empty report with zero total.
void testEmptySpendRecordsProduceEmptyReport() {
  const EpochTreasuryReport report =
      EpochTreasuryReport::fromSpendRecords(0, {});
  assert(report.isValid());
  assert(report.treasurySpendTotal() == Amount::fromRawUnits(0));
  assert(report.spendRecordCount() == 0);
}

// Test 26: Non-empty spend records produce a non-empty report.
void testNonEmptySpendRecordsProduceNonEmptyReport() {
  const TreasurySpendRecord record(
      "spend-001", "prop-001", "recipient", Amount::fromRawUnits(50000), "fund",
      10, 0, Amount::fromRawUnits(100000), Amount::fromRawUnits(50000));
  const EpochTreasuryReport report =
      EpochTreasuryReport::fromSpendRecords(0, {record});
  assert(report.isValid());
  assert(report.treasurySpendTotal() == Amount::fromRawUnits(50000));
  assert(report.spendRecordCount() == 1);
}

// Test 27: Treasury report correctly accumulates multiple spends.
void testReportAccumulatesMultipleSpends() {
  const TreasurySpendRecord r1(
      "spend-001", "prop-001", "recipient", Amount::fromRawUnits(30000), "fund",
      10, 0, Amount::fromRawUnits(100000), Amount::fromRawUnits(70000));
  const TreasurySpendRecord r2("spend-002", "prop-002", "recipient",
                               Amount::fromRawUnits(20000), "reward", 11, 0,
                               Amount::fromRawUnits(70000),
                               Amount::fromRawUnits(50000));
  const EpochTreasuryReport report =
      EpochTreasuryReport::fromSpendRecords(0, {r1, r2});
  assert(report.isValid());
  assert(report.treasurySpendTotal() == Amount::fromRawUnits(50000));
  assert(report.spendRecordCount() == 2);
}

} // namespace

int main() {
  testEmptySpendRecordsProduceEmptyReport();
  testNonEmptySpendRecordsProduceNonEmptyReport();
  testReportAccumulatesMultipleSpends();
  return 0;
}
