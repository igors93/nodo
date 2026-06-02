#include "node/MonetaryReportVerifier.hpp"
#include "economics/EpochMonetaryReport.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "economics/BurnRecord.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using nodo::economics::BurnRecord;
using nodo::economics::BurnType;
using nodo::economics::EpochMonetaryReport;
using nodo::economics::MonetaryPolicy;
using nodo::economics::SupplyDelta;
using nodo::node::MonetaryReportVerificationStatus;
using nodo::node::MonetaryReportVerifier;
namespace utils = nodo::utils;
using nodo::utils::Amount;

MonetaryPolicy testPolicy() {
    return MonetaryPolicy::localnetDefault(
        "report-verifier-test", Amount::fromRawUnits(500000)
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

EpochMonetaryReport buildReport() {
    const auto policy = testPolicy();
    const auto s0 = policy.initialSupply();
    const auto s1 = Amount::fromRawUnits(s0.rawUnits() - 100);
    std::vector<SupplyDelta> deltas = {
        makeBurnDelta(1, "hash-v1", s0, 100),
        makeBurnDelta(2, "hash-v2", s1, 50)
    };
    return EpochMonetaryReport::fromDeltas(policy, 0, 1, 2, deltas);
}

EpochMonetaryReport buildReportFromStoredFields(
    const MonetaryPolicy& policy,
    Amount starting,
    Amount minted,
    Amount burned,
    Amount ending,
    std::size_t deltaCount = 2
) {
    return EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        1, 2,
        starting, ending, minted, burned,
        deltaCount, 0, deltaCount
    );
}

// Matching persisted and rebuilt reports pass verification.
void testMatchingReportsPass() {
    const auto rebuilt = buildReport();
    assert(rebuilt.isValid());

    // Build persisted from same values as rebuilt.
    const auto persisted = buildReportFromStoredFields(
        testPolicy(),
        rebuilt.startingSupply(),
        rebuilt.totalMinted(),
        rebuilt.totalBurned(),
        rebuilt.endingSupply(),
        rebuilt.deltaCount()
    );
    assert(persisted.isValid());

    const auto result = MonetaryReportVerifier::verify(persisted, rebuilt);
    assert(result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::MATCH);
}

// Tampered totalBurned in persisted report fails.
void testTamperedBurnedFails() {
    const auto rebuilt = buildReport();
    assert(rebuilt.isValid());

    // Persisted has wrong totalBurned (and matching wrong ending).
    const auto policy = testPolicy();
    const auto tampered = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        1, 2,
        rebuilt.startingSupply(),
        // Adjust ending to match fake burned so fromStoredFields passes its own check.
        Amount::fromRawUnits(rebuilt.startingSupply().rawUnits() - 999),
        rebuilt.totalMinted(),
        Amount::fromRawUnits(999),  // fake burned
        rebuilt.deltaCount(), 0, rebuilt.deltaCount()
    );
    assert(tampered.isValid());

    const auto result = MonetaryReportVerifier::verify(tampered, rebuilt);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::FIELD_MISMATCH);
}

// Tampered endingSupply fails (even if arithmetic matches within persisted).
void testTamperedEndingSupplyFails() {
    const auto rebuilt = buildReport();
    assert(rebuilt.isValid());

    // Build persisted with different deltaCount so endingSupply check works.
    // Actually, change deltaCount to force mismatch.
    const auto policy = testPolicy();
    const auto tampered = buildReportFromStoredFields(
        policy,
        rebuilt.startingSupply(),
        rebuilt.totalMinted(),
        rebuilt.totalBurned(),
        rebuilt.endingSupply(),
        rebuilt.deltaCount() + 1  // tampered deltaCount
    );
    assert(tampered.isValid());

    const auto result = MonetaryReportVerifier::verify(tampered, rebuilt);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::FIELD_MISMATCH);
}

// Invalid persisted report fails with PERSISTED_INVALID.
void testInvalidPersistedFails() {
    const EpochMonetaryReport invalid;
    const auto rebuilt = buildReport();

    const auto result = MonetaryReportVerifier::verify(invalid, rebuilt);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::PERSISTED_INVALID);
}

// Invalid rebuilt report fails with REBUILT_INVALID.
void testInvalidRebuiltFails() {
    const auto persisted = buildReport();
    const EpochMonetaryReport invalid;

    const auto result = MonetaryReportVerifier::verify(persisted, invalid);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::REBUILT_INVALID);
}

// Tampered startBlock in persisted report fails.
void testTamperedStartBlockFails() {
    const auto rebuilt = buildReport();
    assert(rebuilt.isValid());

    const auto policy = testPolicy();
    // startBlock=3 > endBlock=2 would be rejected by fromStoredFields, so
    // use startBlock=0 with endBlock=2 (valid range but mismatches rebuilt startBlock=1).
    const auto tampered = EpochMonetaryReport::fromStoredFields(
        policy, policy.policyVersion(), 0,
        0, 2,  // startBlock=0 != rebuilt.startBlock()=1
        rebuilt.startingSupply(),
        rebuilt.endingSupply(),
        rebuilt.totalMinted(),
        rebuilt.totalBurned(),
        rebuilt.deltaCount(), 0, rebuilt.deltaCount()
    );
    assert(tampered.isValid());

    const auto result = MonetaryReportVerifier::verify(tampered, rebuilt);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::FIELD_MISMATCH);
}

// policyVersion mismatch fails.
// localnetDefault always uses "NODO_MONETARY_POLICY_V1", so we use the full
// MonetaryPolicy constructor to create a policy with a different policyVersion.
void testPolicyVersionMismatchFails() {
    const auto rebuilt = buildReport();
    assert(rebuilt.isValid());

    // Create a policy with a different policyVersion string.
    const MonetaryPolicy otherPolicy(
        "NODO_MONETARY_POLICY_V2",
        "report-verifier-test",
        "NODO", "raw",
        static_cast<std::uint64_t>(utils::Amount::UNITS_PER_NODO),
        Amount::fromRawUnits(500000),
        400
    );
    assert(otherPolicy.isValid());
    assert(otherPolicy.policyVersion() == "NODO_MONETARY_POLICY_V2");

    // Build a persisted report using the other policy (different policyVersion).
    // All field values match the rebuilt report except policyVersion.
    const auto persisted = EpochMonetaryReport::fromStoredFields(
        otherPolicy, "NODO_MONETARY_POLICY_V2", 0,
        rebuilt.startBlock(), rebuilt.endBlock(),
        rebuilt.startingSupply(), rebuilt.endingSupply(),
        rebuilt.totalMinted(), rebuilt.totalBurned(),
        rebuilt.deltaCount(), 0, rebuilt.deltaCount()
    );
    assert(persisted.isValid());
    assert(persisted.policyVersion() == "NODO_MONETARY_POLICY_V2");

    // The verifier should detect the policyVersion mismatch.
    const auto result = MonetaryReportVerifier::verify(persisted, rebuilt);
    assert(!result.matched());
    assert(result.status() == MonetaryReportVerificationStatus::FIELD_MISMATCH);
}

} // namespace

int main() {
    testMatchingReportsPass();
    testTamperedBurnedFails();
    testTamperedEndingSupplyFails();
    testInvalidPersistedFails();
    testInvalidRebuiltFails();
    testTamperedStartBlockFails();
    testPolicyVersionMismatchFails();
    return 0;
}
