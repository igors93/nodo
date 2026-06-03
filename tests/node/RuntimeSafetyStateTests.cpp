#include "node/RuntimeSafetyState.hpp"
#include "node/RuntimeSafetyStateStore.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::node::RuntimeSafetyState;
using nodo::node::RuntimeSafetyStateReadStatus;
using nodo::node::RuntimeSafetyStateStore;

std::filesystem::path tempSafetyStatePath() {
    return std::filesystem::temp_directory_path() /
           "nodo_test_runtime_safety_state.nodo";
}

void cleanup() {
    std::error_code ec;
    std::filesystem::remove(tempSafetyStatePath(), ec);
}

// Default-constructed state is valid with INACTIVE defense mode.
void testNewNodeDefaultIsInactiveAndValid() {
    const auto state = RuntimeSafetyState::newNodeDefault();
    assert(state.isValid());
    assert(state.defenseMode() == DefenseModeState::INACTIVE);
    assert(state.activationHeight() == 0);
    assert(state.activationReason().empty());
    assert(state.evidenceId().empty());
    assert(state.governanceProposalId().empty());
    assert(state.lastChainAuditHeight() == 0);
    assert(state.exitRequiresChainAudit());
}

// ACTIVE state with critical trigger and valid evidence is accepted.
void testActiveStateIsValid() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        42,
        "chain audit failed at block 42",
        "evidence-001",
        "",    // governanceProposalId: empty for non-governance trigger
        0,
        true,
        1900000001
    );
    assert(state.isValid());
    assert(state.defenseMode() == DefenseModeState::ACTIVE);
    assert(state.activationHeight() == 42);
    assert(state.activationTrigger() == DefenseModeTrigger::CHAIN_AUDIT_FAILURE);
    assert(!state.activationReason().empty());
    assert(state.evidenceId() == "evidence-001");
    assert(state.governanceProposalId().empty());
}

// Test 1: Active state with critical trigger and no evidence is rejected.
void testActiveCriticalTriggerWithNoEvidenceRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        100,
        "supply divergence detected",
        "",    // empty evidenceId with critical trigger is rejected
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
    assert(!state.rejectionReason().empty());
    assert(state.rejectionReason().find("evidenceId") != std::string::npos);
}

// Test 1 (all critical triggers): double-sign mass event requires evidence.
void testActiveMassDoubleSignWithNoEvidenceRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT,
        50,
        "mass double-sign event",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// Test 1: unauthorized treasury spend attempt requires evidence.
void testActiveUnauthorizedTreasuryWithNoEvidenceRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT,
        75,
        "unauthorized treasury spend attempt",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// Test 1: storage corruption requires evidence.
void testActiveStorageCorruptionWithNoEvidenceRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::STORAGE_CORRUPTION,
        90,
        "storage corruption detected",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// Test 2: Active state with valid evidence is accepted.
void testActiveCriticalTriggerWithEvidenceAccepted() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        100,
        "supply divergence",
        "ev-supply-001",
        "",
        0,
        true,
        1900000001
    );
    assert(state.isValid());
}

// Test 3: Governance-triggered defense state without proposal context is rejected.
void testActiveGovernanceTriggerWithNoProposalRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        200,
        "governance vote activated defense mode",
        "",
        "",    // empty governanceProposalId is rejected for GOVERNANCE_VOTED
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
    assert(!state.rejectionReason().empty());
    assert(state.rejectionReason().find("governanceProposalId") != std::string::npos);
}

// Governance-triggered with proposal id is accepted.
void testActiveGovernanceTriggerWithProposalAccepted() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        200,
        "governance vote activated defense mode",
        "",
        "gov-proposal-001",
        0,
        true,
        1900000001
    );
    assert(state.isValid());
    assert(state.governanceProposalId() == "gov-proposal-001");
}

// OPERATOR_DECLARED requires a reason (already validated).
void testActiveOperatorDeclaredWithoutReasonRejected() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// INACTIVE state with non-zero activationHeight is invalid.
void testInactiveStateWithActivationHeightIsInvalid() {
    const RuntimeSafetyState state(
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "reason",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
    assert(!state.rejectionReason().empty());
}

// Persist and reload correctly.
void testRoundTripDefaultState() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const RuntimeSafetyState original = RuntimeSafetyState::newNodeDefault();
    const auto writeResult = RuntimeSafetyStateStore::write(path, original);
    assert(writeResult.isWritten());
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.isLoaded());
    assert(loaded.state().defenseMode() == DefenseModeState::INACTIVE);
    assert(loaded.state().activationHeight() == 0);
    assert(loaded.state().exitRequiresChainAudit());
    assert(loaded.state().governanceProposalId().empty());
    cleanup();
}

// Persist ACTIVE state with evidence and reload correctly.
void testRoundTripActiveState() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const RuntimeSafetyState original(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        100,
        "supply divergence detected",
        "ev-supply-001",
        "",    // governanceProposalId empty for non-governance trigger
        0,
        true,
        1900000042
    );
    const auto writeResult = RuntimeSafetyStateStore::write(path, original);
    assert(writeResult.isWritten());
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.isLoaded());
    assert(loaded.state().defenseMode() == DefenseModeState::ACTIVE);
    assert(loaded.state().activationTrigger() == DefenseModeTrigger::SUPPLY_DIVERGENCE);
    assert(loaded.state().activationHeight() == 100);
    assert(loaded.state().activationReason() == "supply divergence detected");
    assert(loaded.state().evidenceId() == "ev-supply-001");
    assert(loaded.state().governanceProposalId().empty());
    assert(loaded.state().updatedAt() == 1900000042);
    cleanup();
}

// Persist ACTIVE governance state and reload correctly.
void testRoundTripGovernanceActiveState() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const RuntimeSafetyState original(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        200,
        "governance voted for defense mode",
        "",
        "gov-prop-abc-001",
        0,
        true,
        1900000100
    );
    const auto writeResult = RuntimeSafetyStateStore::write(path, original);
    assert(writeResult.isWritten());
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.isLoaded());
    assert(loaded.state().governanceProposalId() == "gov-prop-abc-001");
    assert(loaded.state().evidenceId().empty());
    cleanup();
}

// Corrupt file returns failure status (not loaded, not missing).
void testCorruptFileReturnsFailure() {
    cleanup();
    const auto path = tempSafetyStatePath();
    {
        std::ofstream f(path);
        f << "this is not a valid safety state file\n";
    }
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(!loaded.isLoaded());
    assert(!loaded.isMissing());
    assert(loaded.isFailure());
    cleanup();
}

// Missing file returns MISSING status.
void testMissingFileReturnsMissing() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(!loaded.isLoaded());
    assert(loaded.isMissing());
    assert(loaded.status() == RuntimeSafetyStateReadStatus::MISSING);
}

// Readiness logic: corrupt file means fail closed.
void testCorruptFileFailsReadiness() {
    cleanup();
    const auto path = tempSafetyStatePath();
    {
        std::ofstream f(path);
        f << "GARBAGE\nno valid schema\n";
    }
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.isFailure());
    cleanup();
}

// Test 5: Invalid state write does not persist.
void testInvalidStateWriteRejected() {
    cleanup();
    const RuntimeSafetyState invalid(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        0,  // activationHeight zero is invalid for ACTIVE
        "reason",
        "ev-001",
        "",
        0,
        true,
        1900000001
    );
    assert(!invalid.isValid());
    const auto writeResult = RuntimeSafetyStateStore::write(tempSafetyStatePath(), invalid);
    assert(!writeResult.isWritten());
    assert(!std::filesystem::exists(tempSafetyStatePath()));
    cleanup();
}

} // namespace

int main() {
    testNewNodeDefaultIsInactiveAndValid();
    testActiveStateIsValid();
    testActiveCriticalTriggerWithNoEvidenceRejected();
    testActiveMassDoubleSignWithNoEvidenceRejected();
    testActiveUnauthorizedTreasuryWithNoEvidenceRejected();
    testActiveStorageCorruptionWithNoEvidenceRejected();
    testActiveCriticalTriggerWithEvidenceAccepted();
    testActiveGovernanceTriggerWithNoProposalRejected();
    testActiveGovernanceTriggerWithProposalAccepted();
    testActiveOperatorDeclaredWithoutReasonRejected();
    testInactiveStateWithActivationHeightIsInvalid();
    testRoundTripDefaultState();
    testRoundTripActiveState();
    testRoundTripGovernanceActiveState();
    testCorruptFileReturnsFailure();
    testMissingFileReturnsMissing();
    testCorruptFileFailsReadiness();
    testInvalidStateWriteRejected();
    return 0;
}
