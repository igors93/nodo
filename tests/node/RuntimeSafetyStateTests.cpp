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
    assert(state.lastChainAuditHeight() == 0);
    assert(state.exitRequiresChainAudit());
}

// ACTIVE state with valid fields is valid.
void testActiveStateIsValid() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        42,
        "chain audit failed at block 42",
        "evidence-001",
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
}

// INACTIVE state with non-zero activationHeight is invalid.
void testInactiveStateWithActivationHeightIsInvalid() {
    const RuntimeSafetyState state(
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,  // non-zero activationHeight while INACTIVE is invalid
        "reason",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
    assert(!state.rejectionReason().empty());
}

// ACTIVE state without reason is invalid.
void testActiveStateWithoutReasonIsInvalid() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "",  // empty reason is invalid for ACTIVE
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// ACTIVE state without activationHeight is invalid.
void testActiveStateWithoutActivationHeightIsInvalid() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        0,  // zero activationHeight is invalid for ACTIVE
        "reason",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
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
    cleanup();
}

// Persist ACTIVE state and reload correctly.
void testRoundTripActiveState() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const RuntimeSafetyState original(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        100,
        "supply divergence detected",
        "ev-supply-001",
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
    assert(loaded.state().updatedAt() == 1900000042);
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

// Readiness logic: corrupt file means fail closed (isFailure=true, not isMissing).
void testCorruptFileFailsReadiness() {
    cleanup();
    const auto path = tempSafetyStatePath();
    {
        std::ofstream f(path);
        f << "GARBAGE\nno valid schema\n";
    }
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.isFailure());
    // Callers must NOT treat this as INACTIVE (defenseModeInactive = false).
    cleanup();
}

} // namespace

int main() {
    testNewNodeDefaultIsInactiveAndValid();
    testActiveStateIsValid();
    testInactiveStateWithActivationHeightIsInvalid();
    testActiveStateWithoutReasonIsInvalid();
    testActiveStateWithoutActivationHeightIsInvalid();
    testRoundTripDefaultState();
    testRoundTripActiveState();
    testCorruptFileReturnsFailure();
    testMissingFileReturnsMissing();
    testCorruptFileFailsReadiness();
    return 0;
}
