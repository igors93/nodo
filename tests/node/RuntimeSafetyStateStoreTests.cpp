#include "node/RuntimeSafetyStateStore.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::node::RuntimeSafetyState;
using nodo::node::RuntimeSafetyStateReadResult;
using nodo::node::RuntimeSafetyStateReadStatus;
using nodo::node::RuntimeSafetyStateStore;
using nodo::node::RuntimeSafetyStateWriteResult;
using nodo::node::RuntimeSafetyStateWriteStatus;

std::filesystem::path tempPath() {
    return std::filesystem::temp_directory_path() /
           "nodo_test_safety_store.nodo";
}

void cleanup() {
    std::error_code ec;
    std::filesystem::remove(tempPath(), ec);
}

// Write returns WRITTEN for valid state.
void testWriteValidStateReturnsWritten() {
    cleanup();
    const RuntimeSafetyState state = RuntimeSafetyState::newNodeDefault();
    const auto result = RuntimeSafetyStateStore::write(tempPath(), state);
    assert(result.isWritten());
    assert(result.status() == RuntimeSafetyStateWriteStatus::WRITTEN);
    cleanup();
}

// Write rejects invalid state (INACTIVE with non-zero activationHeight).
void testWriteInvalidStateReturnsInvalidState() {
    cleanup();
    const RuntimeSafetyState badState(
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        99,  // non-zero activationHeight while INACTIVE is invalid
        "reason",
        "",
        "",
        0,
        true,
        1900000001
    );
    const auto result = RuntimeSafetyStateStore::write(tempPath(), badState);
    assert(!result.isWritten());
    assert(result.status() == RuntimeSafetyStateWriteStatus::INVALID_STATE);
    // File must not have been created.
    assert(!std::filesystem::exists(tempPath()));
    cleanup();
}

// Read on missing file returns MISSING.
void testReadMissingFileReturnsMissing() {
    cleanup();
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(result.isMissing());
    assert(result.status() == RuntimeSafetyStateReadStatus::MISSING);
}

// Round-trip: write then read returns LOADED with same values.
void testRoundTripDefaultState() {
    cleanup();
    const RuntimeSafetyState original = RuntimeSafetyState::newNodeDefault();
    RuntimeSafetyStateStore::write(tempPath(), original);
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(result.isLoaded());
    assert(result.status() == RuntimeSafetyStateReadStatus::LOADED);
    assert(result.state().defenseMode() == DefenseModeState::INACTIVE);
    assert(result.state().activationHeight() == 0);
    cleanup();
}

// Round-trip for ACTIVE state.
void testRoundTripActiveState() {
    cleanup();
    const RuntimeSafetyState original(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        50,
        "chain audit failed",
        "ev-audit-001",
        "",    // governanceProposalId empty for non-governance trigger
        45,
        true,
        1900000099
    );
    RuntimeSafetyStateStore::write(tempPath(), original);
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(result.isLoaded());
    assert(result.state().defenseMode() == DefenseModeState::ACTIVE);
    assert(result.state().activationHeight() == 50);
    assert(result.state().activationReason() == "chain audit failed");
    assert(result.state().evidenceId() == "ev-audit-001");
    assert(result.state().lastChainAuditHeight() == 45);
    assert(result.state().updatedAt() == 1900000099);
    cleanup();
}

// Malformed file returns MALFORMED.
void testMalformedFileReturnsMalformed() {
    cleanup();
    {
        std::ofstream f(tempPath());
        f << "this is not a valid key-value file\n";
    }
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(!result.isLoaded());
    assert(!result.isMissing());
    assert(result.status() == RuntimeSafetyStateReadStatus::MALFORMED ||
           result.status() == RuntimeSafetyStateReadStatus::SCHEMA_MISMATCH);
    cleanup();
}

// File with wrong schema version returns SCHEMA_MISMATCH.
void testSchemaMismatchReturnsSchemaError() {
    cleanup();
    {
        std::ofstream f(tempPath());
        f << "NODO_WRONG_SCHEMA_VERSION\n";
        f << "defenseMode=INACTIVE\n";
    }
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(!result.isLoaded());
    assert(!result.isMissing());
    assert(result.status() == RuntimeSafetyStateReadStatus::SCHEMA_MISMATCH ||
           result.status() == RuntimeSafetyStateReadStatus::MALFORMED);
    cleanup();
}

// Corrupt safety state fails readiness (isFailure() true).
void testCorruptStateIsFailure() {
    cleanup();
    {
        std::ofstream f(tempPath());
        f << "GARBAGE_DATA\nno_valid_content=true\n";
    }
    const auto result = RuntimeSafetyStateStore::read(tempPath());
    assert(!result.isLoaded());
    assert(result.isFailure());
    cleanup();
}

// Active defense mode requires non-empty activation reason.
void testActiveDefenseModeRequiresReason() {
    cleanup();
    // Construct by hand without going through the safe constructor.
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "",  // empty reason is invalid for ACTIVE
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
    // write() should reject this.
    const auto writeResult = RuntimeSafetyStateStore::write(tempPath(), state);
    assert(!writeResult.isWritten());
    cleanup();
}

// Active defense mode requires non-zero activation height.
void testActiveDefenseModeRequiresActivationHeight() {
    const RuntimeSafetyState state(
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        0,  // zero activation height is invalid for ACTIVE
        "some reason",
        "",
        "",
        0,
        true,
        1900000001
    );
    assert(!state.isValid());
}

// Status to string covers all cases.
void testStatusToString() {
    using nodo::node::runtimeSafetyStateReadStatusToString;
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::LOADED) == "LOADED");
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::MISSING) == "MISSING");
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::MALFORMED) == "MALFORMED");
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::INVALID) == "INVALID");
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::SCHEMA_MISMATCH) == "SCHEMA_MISMATCH");
    assert(runtimeSafetyStateReadStatusToString(
               RuntimeSafetyStateReadStatus::IO_FAILURE) == "IO_FAILURE");
}

} // namespace

int main() {
    testWriteValidStateReturnsWritten();
    testWriteInvalidStateReturnsInvalidState();
    testReadMissingFileReturnsMissing();
    testRoundTripDefaultState();
    testRoundTripActiveState();
    testMalformedFileReturnsMalformed();
    testSchemaMismatchReturnsSchemaError();
    testCorruptStateIsFailure();
    testActiveDefenseModeRequiresReason();
    testActiveDefenseModeRequiresActivationHeight();
    testStatusToString();
    return 0;
}
