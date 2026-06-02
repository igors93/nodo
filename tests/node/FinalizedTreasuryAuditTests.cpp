#include "node/FinalizedTreasuryAudit.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using nodo::economics::EpochTreasuryReport;
using nodo::node::FinalizedBlockArtifact;
using nodo::node::FinalizedTreasuryAudit;
using nodo::utils::Amount;

// Empty artifact sequence passes — no spends, zero totals.
// Proof-of-Protection: empty treasury is verifiable and deterministic.
void testEmptyArtifactSequencePasses() {
    const std::vector<FinalizedBlockArtifact> empty;
    const auto result = FinalizedTreasuryAudit::auditArtifacts(0, empty);

    assert(result.passed());
    assert(result.totalSpendRecords() == 0);
    assert(result.rebuiltReport().isValid());
    assert(result.rebuiltReport().treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(result.rebuiltReport().spendRecordCount() == 0);
}

// Artifact sequence with default-constructed artifacts (empty treasury sections) passes.
// All current finalized artifacts have empty treasury sections.
void testDefaultArtifactsHaveEmptyTreasurySections() {
    // FinalizedBlockArtifact default-constructed has an empty treasury section.
    const FinalizedBlockArtifact artifact;
    assert(artifact.treasurySection().isValid());
    assert(artifact.treasurySection().spendRecordCount() == 0);

    std::vector<FinalizedBlockArtifact> artifacts = {artifact};
    const auto result = FinalizedTreasuryAudit::auditArtifacts(0, artifacts);

    assert(result.passed());
    assert(result.totalSpendRecords() == 0);
}

// Rebuilt treasury report matches spend records in artifacts.
void testRebuiltReportMatchesSpendRecords() {
    // All current artifacts have zero spends, so the rebuilt report has
    // total=0 and count=0.
    const std::vector<FinalizedBlockArtifact> artifacts(3);
    const auto result = FinalizedTreasuryAudit::auditArtifacts(0, artifacts);

    assert(result.passed());
    assert(result.rebuiltReport().spendRecordCount() == 0);
    assert(result.rebuiltReport().treasurySpendTotal() == Amount::fromRawUnits(0));
    assert(result.rebuiltReport().epoch() == 0);
}

// Audit with epoch parameter is stored in rebuilt report.
void testEpochStoredInRebuiltReport() {
    const std::vector<FinalizedBlockArtifact> empty;
    const auto result = FinalizedTreasuryAudit::auditArtifacts(5, empty);

    assert(result.passed());
    assert(result.rebuiltReport().epoch() == 5);
}

} // namespace

int main() {
    testEmptyArtifactSequencePasses();
    testDefaultArtifactsHaveEmptyTreasurySections();
    testRebuiltReportMatchesSpendRecords();
    testEpochStoredInRebuiltReport();
    return 0;
}
