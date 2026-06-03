#include "node/DefenseModeTransitionApplier.hpp"
#include "node/RuntimeSafetyStateStore.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::economics::DefenseModeTransitionRecord;
using nodo::node::DefenseModeTransitionApplier;
using nodo::node::DefenseModeTransitionApplyResult;
using nodo::node::DefenseModeTransitionApplyStatus;
using nodo::node::RuntimeSafetyState;
using nodo::node::RuntimeSafetyStateStore;

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_test_transition_applier.nodo";
}

void cleanup() {
    std::error_code ec;
    std::filesystem::remove(tempPath(), ec);
}

RuntimeSafetyState defaultInactiveState() {
    return RuntimeSafetyState::newNodeDefault();
}

RuntimeSafetyState activeState() {
    return RuntimeSafetyState(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        50,
        "chain audit failed",
        "ev-001",
        0,
        true,
        1900000001
    );
}

DefenseModeTransitionRecord validActivationRecord() {
    return DefenseModeTransitionRecord(
        "trans-act-001",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        100,
        "operator declared defense mode",
        "",   // evidenceId empty - OPERATOR_DECLARED is not a critical trigger
        "",
        0,
        1900000010
    );
}

DefenseModeTransitionRecord validExitRecord(std::uint64_t auditHeight = 60) {
    return DefenseModeTransitionRecord(
        "trans-exit-001",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        110,
        "chain audit completed",
        "",
        "",
        auditHeight,
        1900000020
    );
}

// Valid activation updates state and persists it.
void testValidActivationUpdatesAndPersists() {
    cleanup();
    const auto currentState = defaultInactiveState();
    const auto result = DefenseModeTransitionApplier::applyActivation(
        validActivationRecord(), currentState, tempPath(), 1900000010
    );
    assert(result.isApplied());
    assert(result.status() == DefenseModeTransitionApplyStatus::APPLIED);
    assert(result.newState().defenseMode() == DefenseModeState::ACTIVE);
    assert(result.newState().activationHeight() == 100);

    // Verify persisted state.
    const auto loaded = RuntimeSafetyStateStore::read(tempPath());
    assert(loaded.isLoaded());
    assert(loaded.state().defenseMode() == DefenseModeState::ACTIVE);
    cleanup();
}

// Cannot activate when already ACTIVE.
void testActivationWhenAlreadyActiveIsRejected() {
    cleanup();
    const auto result = DefenseModeTransitionApplier::applyActivation(
        validActivationRecord(), activeState(), tempPath(), 1900000010
    );
    assert(!result.isApplied());
    assert(result.status() == DefenseModeTransitionApplyStatus::NO_STATE_CHANGE);
    assert(!std::filesystem::exists(tempPath()));
    cleanup();
}

// Invalid transition (wrong direction) is rejected.
void testInvalidActivationDirectionRejected() {
    cleanup();
    const DefenseModeTransitionRecord wrongDir(
        "trans-wrong",
        DefenseModeState::ACTIVE,  // wrong direction for activation
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        100,
        "reason",
        "",
        "",
        0,
        1900000010
    );
    const auto result = DefenseModeTransitionApplier::applyActivation(
        wrongDir, defaultInactiveState(), tempPath(), 1900000010
    );
    assert(!result.isApplied());
    assert(!std::filesystem::exists(tempPath()));
    cleanup();
}

// Valid exit updates state and persists it.
void testValidExitUpdatesAndPersists() {
    cleanup();
    const auto result = DefenseModeTransitionApplier::applyExit(
        validExitRecord(60), activeState(), true, 55, tempPath(), 1900000020
    );
    assert(result.isApplied());
    assert(result.newState().defenseMode() == DefenseModeState::INACTIVE);

    const auto loaded = RuntimeSafetyStateStore::read(tempPath());
    assert(loaded.isLoaded());
    assert(loaded.state().defenseMode() == DefenseModeState::INACTIVE);
    cleanup();
}

// Exit fails when chain audit height is insufficient.
void testExitWithInsufficientAuditFails() {
    cleanup();
    const auto result = DefenseModeTransitionApplier::applyExit(
        validExitRecord(30), activeState(), true, 55, tempPath(), 1900000020
    );
    assert(!result.isApplied());
    assert(result.status() == DefenseModeTransitionApplyStatus::VALIDATION_REJECTED);
    assert(!std::filesystem::exists(tempPath()));
    cleanup();
}

// Exit when already INACTIVE is rejected.
void testExitWhenAlreadyInactiveRejected() {
    cleanup();
    const auto result = DefenseModeTransitionApplier::applyExit(
        validExitRecord(60), defaultInactiveState(), false, 0, tempPath(), 1900000020
    );
    assert(!result.isApplied());
    assert(result.status() == DefenseModeTransitionApplyStatus::NO_STATE_CHANGE);
    cleanup();
}

// Status to string.
void testStatusToString() {
    using nodo::node::defenseModeTransitionApplyStatusToString;
    assert(defenseModeTransitionApplyStatusToString(
               DefenseModeTransitionApplyStatus::APPLIED) == "APPLIED");
    assert(defenseModeTransitionApplyStatusToString(
               DefenseModeTransitionApplyStatus::VALIDATION_REJECTED) ==
           "VALIDATION_REJECTED");
    assert(defenseModeTransitionApplyStatusToString(
               DefenseModeTransitionApplyStatus::PERSIST_FAILED) ==
           "PERSIST_FAILED");
}

} // namespace

int main() {
    testValidActivationUpdatesAndPersists();
    testActivationWhenAlreadyActiveIsRejected();
    testInvalidActivationDirectionRejected();
    testValidExitUpdatesAndPersists();
    testExitWithInsufficientAuditFails();
    testExitWhenAlreadyInactiveRejected();
    testStatusToString();
    return 0;
}
