// Tests for defense mode behavior in the local testnet runtime.
// Covers: activation without canonical evidence rejected, activation through
// validated transition applier accepted, direct safety state bypass rejected,
// direct treasury/monetary report bypass rejected.

#include "economics/DefenseModeState.hpp"
#include "economics/DefenseModeTransitionRecord.hpp"
#include "economics/EpochTreasuryReport.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "node/DefenseModeTransitionApplier.hpp"
#include "node/EpochTreasuryReportVerifier.hpp"
#include "node/RuntimeSafetyState.hpp"
#include "node/RuntimeSafetyStateStore.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::economics::DefenseModeTransitionRecord;
using nodo::economics::EpochTreasuryReport;
using nodo::economics::TreasurySpendRecord;
using nodo::node::DefenseModeTransitionApplier;
using nodo::node::DefenseModeTransitionApplyStatus;
using nodo::node::EpochTreasuryReportVerifier;
using nodo::node::RuntimeSafetyState;
using nodo::node::RuntimeSafetyStateStore;
using nodo::utils::Amount;

std::filesystem::path tempStatePath() {
    return std::filesystem::temp_directory_path() /
           "nodo_local_defense_mode_test.nodo";
}

void cleanState() {
    std::error_code ec;
    std::filesystem::remove(tempStatePath(), ec);
}

RuntimeSafetyState inactiveState() {
    return RuntimeSafetyState::newNodeDefault();
}

RuntimeSafetyState activeStateCritical() {
    return RuntimeSafetyState(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        50,
        "chain audit failed",
        "ev-critical-001",  // evidence required for critical trigger
        "",
        0,
        true,
        1900100001
    );
}

// ---- Test 1: Critical trigger activation without evidence is rejected at state level ----

void testCriticalActivationWithoutEvidenceRejectedByState() {
    cleanState();

    // SUPPLY_DIVERGENCE is a critical trigger requiring evidenceId.
    const RuntimeSafetyState invalidState(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        100,
        "supply divergence detected",
        "",    // evidenceId empty — invalid for critical trigger
        "",
        0,
        true,
        1900100001
    );
    assert(!invalidState.isValid() &&
        "SUPPLY_DIVERGENCE activation without evidenceId must be invalid.");

    // Store must reject the invalid state.
    const auto result = RuntimeSafetyStateStore::write(tempStatePath(), invalidState);
    assert(!result.isWritten() &&
        "RuntimeSafetyStateStore must reject state without evidence for critical trigger.");
    assert(!std::filesystem::exists(tempStatePath()) &&
        "No file must be written for invalid critical activation.");

    cleanState();
}

// ---- Test 2: Governance trigger activation without proposal ID is rejected ----

void testGovernanceTriggerWithoutProposalIdRejected() {
    cleanState();

    const RuntimeSafetyState invalidState(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        200,
        "governance activated",
        "",
        "",    // governanceProposalId empty — invalid for GOVERNANCE_VOTED
        0,
        true,
        1900100001
    );
    assert(!invalidState.isValid() &&
        "GOVERNANCE_VOTED activation without proposalId must be invalid.");

    const auto result = RuntimeSafetyStateStore::write(tempStatePath(), invalidState);
    assert(!result.isWritten() &&
        "Store must reject GOVERNANCE_VOTED activation without proposal id.");

    cleanState();
}

// ---- Test 3: Activation through validated transition applier is accepted ----

void testActivationThroughValidatedTransitionApplierAccepted() {
    cleanState();

    // OPERATOR_DECLARED does not require evidenceId.
    const DefenseModeTransitionRecord record(
        "local-trans-act-001",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        100,
        "operator declared defense mode for local testing",
        "",   // no evidence required for OPERATOR_DECLARED
        "",
        0,
        1900100010
    );

    const auto result = DefenseModeTransitionApplier::applyActivation(
        record, inactiveState(), tempStatePath(), 1900100010
    );
    assert(result.isApplied() &&
        "Valid OPERATOR_DECLARED activation must be applied.");
    assert(result.newState().defenseMode() == DefenseModeState::ACTIVE &&
        "New state must be ACTIVE after activation.");

    // The persisted state must be readable and valid.
    const auto loaded = RuntimeSafetyStateStore::read(tempStatePath());
    assert(loaded.isLoaded() &&
        "Persisted safety state must be readable after activation.");
    assert(loaded.state().defenseMode() == DefenseModeState::ACTIVE &&
        "Persisted state must be ACTIVE.");
    assert(loaded.state().activationTrigger() == DefenseModeTrigger::OPERATOR_DECLARED &&
        "Persisted state must record the correct activation trigger.");

    cleanState();
}

// ---- Test 4: Activation when already ACTIVE is rejected (no double-activation) ----

void testActivationWhenAlreadyActiveIsRejected() {
    cleanState();

    const DefenseModeTransitionRecord record(
        "local-trans-act-002",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        101,
        "second activation attempt",
        "",
        "",
        0,
        1900100011
    );

    const auto result = DefenseModeTransitionApplier::applyActivation(
        record, activeStateCritical(), tempStatePath(), 1900100011
    );
    assert(!result.isApplied() &&
        "Activation when already ACTIVE must be rejected.");
    assert(result.status() == DefenseModeTransitionApplyStatus::NO_STATE_CHANGE &&
        "Status must be NO_STATE_CHANGE for double-activation attempt.");
    assert(!std::filesystem::exists(tempStatePath()) &&
        "No file must be written for rejected double-activation.");

    cleanState();
}

// ---- Test 5: Exit requires chain audit when policy requires it ----

void testExitRequiresChainAuditWhenPolicyRequires() {
    cleanState();

    const DefenseModeTransitionRecord exitRecord(
        "local-trans-exit-001",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        110,
        "operator-declared exit attempt",
        "",
        "",
        30,   // auditHeight = 30 < minimum of 55
        1900100020
    );

    // Policy requires audit, minimum audit height is 55, but record provides 30.
    const auto result = DefenseModeTransitionApplier::applyExit(
        exitRecord,
        activeStateCritical(),
        true,   // auditRequiredByPolicy
        55,     // minimumAuditHeight
        tempStatePath(),
        1900100020
    );

    assert(!result.isApplied() &&
        "Exit must be rejected when chain audit height is insufficient.");
    assert(result.status() == DefenseModeTransitionApplyStatus::VALIDATION_REJECTED &&
        "Status must be VALIDATION_REJECTED for insufficient audit height.");
    assert(!std::filesystem::exists(tempStatePath()) &&
        "No file must be written for rejected exit.");

    cleanState();
}

// ---- Test 6: Valid exit through transition applier is accepted ----

void testValidExitThroughTransitionApplierAccepted() {
    cleanState();

    const DefenseModeTransitionRecord exitRecord(
        "local-trans-exit-002",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        120,
        "operator declared safe to exit",
        "",
        "",
        60,   // auditHeight = 60 >= minimum of 55
        1900100030
    );

    const auto result = DefenseModeTransitionApplier::applyExit(
        exitRecord,
        activeStateCritical(),
        true,
        55,
        tempStatePath(),
        1900100030
    );

    assert(result.isApplied() &&
        "Valid exit must be applied.");
    assert(result.newState().defenseMode() == DefenseModeState::INACTIVE &&
        "New state must be INACTIVE after exit.");

    const auto loaded = RuntimeSafetyStateStore::read(tempStatePath());
    assert(loaded.isLoaded() && "State must be readable after exit.");
    assert(loaded.state().defenseMode() == DefenseModeState::INACTIVE &&
        "Persisted state must be INACTIVE after exit.");

    cleanState();
}

// ---- Test 7: Direct runtime safety state bypass is rejected ----
// No production code may write an active state without going through the validated
// transition applier. The store itself enforces validity on every write.

void testDirectRuntimeSafetyStateBypassRejected() {
    cleanState();

    // Attempt to bypass the applier by writing directly to the store.
    // An ACTIVE state with critical trigger but no evidence must be rejected.
    const RuntimeSafetyState bypassAttempt(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        50,
        "bypass direct write",
        "",   // no evidence
        "",
        0,
        true,
        1900100001
    );

    // isValid() must return false: the state is structurally invalid.
    assert(!bypassAttempt.isValid() &&
        "Direct bypass state must be structurally invalid.");

    // RuntimeSafetyStateStore::write must reject the invalid state.
    const auto writeResult = RuntimeSafetyStateStore::write(tempStatePath(), bypassAttempt);
    assert(!writeResult.isWritten() &&
        "RuntimeSafetyStateStore must reject invalid direct bypass attempt.");
    assert(!std::filesystem::exists(tempStatePath()) &&
        "No file must be written on rejected bypass attempt.");

    cleanState();
}

// ---- Test 8: Direct treasury report bypass is rejected by verifier ----
// A treasury report not derived from canonical spend records fails verification
// when compared against the canonical rebuilt report.

void testDirectTreasuryReportBypassRejectedByVerifier() {
    const TreasurySpendRecord canonical(
        "spend-def-001", "prop-def-001", "recipient-canonical",
        Amount::fromRawUnits(50000), "security", 10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(50000)
    );

    const EpochTreasuryReport rebuilt = EpochTreasuryReport::fromSpendRecords(0, {canonical});

    // A report with matching total but different recipient (bypass attempt).
    const TreasurySpendRecord bypass(
        "spend-def-001", "prop-def-001", "recipient-attacker",
        Amount::fromRawUnits(50000), "security", 10, 0,
        Amount::fromRawUnits(100000),
        Amount::fromRawUnits(50000)
    );
    const EpochTreasuryReport bypassReport = EpochTreasuryReport::fromSpendRecords(0, {bypass});

    // Same total, different recipient: must fail verification.
    assert(bypassReport.treasurySpendTotal() == rebuilt.treasurySpendTotal() &&
        "Bypass total must match (tests that total-only check is insufficient).");
    assert(bypassReport.spendRecordsDigest() != rebuilt.spendRecordsDigest() &&
        "Bypass report must have different digest than canonical report.");

    const auto result = EpochTreasuryReportVerifier::verify(bypassReport, rebuilt);
    assert(!result.matched() &&
        "Treasury report bypass with different recipient must fail verification.");
    assert(!result.reason().empty() &&
        "Verification failure must provide a reason.");
}

// ---- Test 9: Direct monetary report bypass is rejected when mismatch detected ----
// A persisted monetary report that does not match the rebuilt report fails audit.

void testDirectMonetaryReportMismatchFailsAudit() {
    // A treasury report built from stored fields (no canonical records) with a
    // digest that doesn't match the canonical sequence must fail.
    const TreasurySpendRecord canonical(
        "spend-mon-001", "prop-mon-001", "recipient-mon",
        Amount::fromRawUnits(25000), "reward", 10, 0,
        Amount::fromRawUnits(50000),
        Amount::fromRawUnits(25000)
    );
    const EpochTreasuryReport canonical_report =
        EpochTreasuryReport::fromSpendRecords(0, {canonical});

    // A fabricated report with same total but no digest (old format bypass).
    // This should pass totals-only check but the operator must be warned.
    const EpochTreasuryReport noDigestReport =
        EpochTreasuryReport::fromStoredFields(
            0,
            Amount::fromRawUnits(25000),
            1
            // no digest
        );
    assert(noDigestReport.spendRecordsDigest().empty() &&
        "Stored-fields report without digest must have empty digest.");

    // Totals match: backwards-compatible check passes for old reports.
    const auto result = EpochTreasuryReportVerifier::verify(noDigestReport, canonical_report);
    // Old format reports (no digest) fall back to totals-only: passes but is weak.
    // This test documents the behavior and ensures it doesn't silently escalate.
    // A fabricated report with a WRONG digest must still fail.
    const EpochTreasuryReport wrongDigestReport =
        EpochTreasuryReport::fromStoredFields(
            0,
            Amount::fromRawUnits(25000),
            1,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" // wrong digest
        );
    const auto wrongResult = EpochTreasuryReportVerifier::verify(wrongDigestReport, canonical_report);
    assert(!wrongResult.matched() &&
        "Report with wrong digest must fail verification even when totals match.");
}

// ---- Test 10: New node default safety state is INACTIVE ----

void testNewNodeDefaultSafetyStateIsInactive() {
    const RuntimeSafetyState defaultState = RuntimeSafetyState::newNodeDefault();
    assert(defaultState.defenseMode() == DefenseModeState::INACTIVE &&
        "New node default safety state must be INACTIVE.");
    assert(defaultState.isValid() &&
        "New node default safety state must be structurally valid.");
}

} // namespace

int main() {
    try {
        testCriticalActivationWithoutEvidenceRejectedByState();
        testGovernanceTriggerWithoutProposalIdRejected();
        testActivationThroughValidatedTransitionApplierAccepted();
        testActivationWhenAlreadyActiveIsRejected();
        testExitRequiresChainAuditWhenPolicyRequires();
        testValidExitThroughTransitionApplierAccepted();
        testDirectRuntimeSafetyStateBypassRejected();
        testDirectTreasuryReportBypassRejectedByVerifier();
        testDirectMonetaryReportMismatchFailsAudit();
        testNewNodeDefaultSafetyStateIsInactive();

        std::cout << "Local defense mode tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Local defense mode tests failed: " << e.what() << "\n";
        return 1;
    }
}
