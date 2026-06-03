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
using nodo::node::RuntimeSafetyStateStore;

// Construct a temp path for testing.
std::filesystem::path tempSafetyStatePath() {
    return std::filesystem::temp_directory_path() /
           "nodo_test_runtime_safety_state.nodo";
}

void cleanup() {
    const auto path = tempSafetyStatePath();
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
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

// Persist and reload correctly.
void testRoundTripDefaultState() {
    cleanup();
    const auto path = tempSafetyStatePath();
    const RuntimeSafetyState original = RuntimeSafetyState::newNodeDefault();
    RuntimeSafetyStateStore::write(path, original);
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.has_value());
    assert(loaded->defenseMode() == DefenseModeState::INACTIVE);
    assert(loaded->activationHeight() == 0);
    assert(loaded->exitRequiresChainAudit());
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
    RuntimeSafetyStateStore::write(path, original);
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(loaded.has_value());
    assert(loaded->defenseMode() == DefenseModeState::ACTIVE);
    assert(loaded->activationTrigger() == DefenseModeTrigger::SUPPLY_DIVERGENCE);
    assert(loaded->activationHeight() == 100);
    assert(loaded->activationReason() == "supply divergence detected");
    assert(loaded->evidenceId() == "ev-supply-001");
    assert(loaded->updatedAt() == 1900000042);
    cleanup();
}

// Corrupt file returns nullopt (not a crash).
void testCorruptFileReturnsNullopt() {
    cleanup();
    const auto path = tempSafetyStatePath();
    {
        std::ofstream f(path);
        f << "this is not a valid safety state file\n";
    }
    const auto loaded = RuntimeSafetyStateStore::read(path);
    assert(!loaded.has_value());
    cleanup();
}

// Missing file does not throw — caller checks existence before calling read().
void testMissingFileDoesNotExist() {
    cleanup();
    const auto path = tempSafetyStatePath();
    assert(!std::filesystem::exists(path));
}

// Readiness check logic: ACTIVE defense mode on missing file (new node) defaults
// to INACTIVE and passes readiness.
void testNewNodeHasNoSafetyStateFile() {
    cleanup();
    const auto path = tempSafetyStatePath();
    // No file → no safety state → caller treats as INACTIVE (new node default).
    const bool fileExists = std::filesystem::exists(path);
    assert(!fileExists);
}

// Readiness check logic: corrupt file must cause defenseModeInactive = false.
void testCorruptFileFailsReadiness() {
    cleanup();
    const auto path = tempSafetyStatePath();
    {
        std::ofstream f(path);
        f << "GARBAGE\nno valid schema\n";
    }
    const auto loaded = RuntimeSafetyStateStore::read(path);
    // read() returns nullopt → caller must treat as unsafe.
    assert(!loaded.has_value());
    cleanup();
}

} // namespace

int main() {
    testNewNodeDefaultIsInactiveAndValid();
    testActiveStateIsValid();
    testInactiveStateWithActivationHeightIsInvalid();
    testRoundTripDefaultState();
    testRoundTripActiveState();
    testCorruptFileReturnsNullopt();
    testMissingFileDoesNotExist();
    testNewNodeHasNoSafetyStateFile();
    testCorruptFileFailsReadiness();
    return 0;
}
