#include "economics/DefenseModeTransitionRecord.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::economics::DefenseModeTransitionRecord;
using nodo::economics::DefenseModeTransitionResult;
using nodo::economics::DefenseModeTransitionStatus;
using nodo::economics::DefenseModeTransitionValidator;

DefenseModeTransitionRecord validActivationRecord() {
    return DefenseModeTransitionRecord(
        "trans-activate-001",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        50,
        "chain audit failed",
        "evidence-chain-001",
        "",
        0,
        1900000001
    );
}

DefenseModeTransitionRecord validExitRecord(
    std::uint64_t auditHeight = 60
) {
    return DefenseModeTransitionRecord(
        "trans-exit-001",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        70,
        "chain audit completed",
        "",
        "",
        auditHeight,
        1900000002
    );
}

// Valid activation record is accepted.
void testValidActivationAccepted() {
    const auto result =
        DefenseModeTransitionValidator::validateActivation(validActivationRecord());
    assert(result.isAccepted());
    assert(result.status() == DefenseModeTransitionStatus::ACCEPTED);
}

// Activation record with same from/to state is invalid.
void testActivationNoStateChangeRejected() {
    const DefenseModeTransitionRecord record(
        "trans-noop",
        DefenseModeState::ACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "no-op transition",
        "",
        "",
        0,
        1900000001
    );
    // isValid() rejects it because fromState == toState.
    assert(!record.isValid());
}

// Activation that is wrong direction (ACTIVE -> INACTIVE) is rejected by validator.
void testActivationWrongDirectionRejected() {
    const DefenseModeTransitionRecord wrongDir(
        "trans-wrong",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "wrong direction",
        "",
        "",
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(wrongDir);
    assert(!result.isAccepted());
    assert(result.status() == DefenseModeTransitionStatus::NO_STATE_CHANGE);
}

// Governance-voted activation requires governanceProposalId.
void testGovernanceActivationRequiresProposalId() {
    const DefenseModeTransitionRecord noGov(
        "trans-gov-missing",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        10,
        "governance voted but no proposal id",
        "",
        "",  // empty governanceProposalId
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(noGov);
    assert(!result.isAccepted());
    assert(result.status() ==
           DefenseModeTransitionStatus::MISSING_GOVERNANCE_CONTEXT);
}

// Governance-voted activation with proposal id passes.
void testGovernanceActivationWithProposalIdPasses() {
    const DefenseModeTransitionRecord withGov(
        "trans-gov-ok",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        10,
        "governance voted to enter defense mode",
        "",
        "gov-prop-defense-001",  // governance proposal id present
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(withGov);
    assert(result.isAccepted());
}

// Valid exit record with sufficient audit height passes.
void testValidExitWithSufficientAuditPasses() {
    const auto result = DefenseModeTransitionValidator::validateExit(
        validExitRecord(60), true, 55
    );
    assert(result.isAccepted());
}

// Exit requires chain audit but audit height is too low.
void testExitWithInsufficientAuditFails() {
    const auto result = DefenseModeTransitionValidator::validateExit(
        validExitRecord(30), true, 55
    );
    assert(!result.isAccepted());
    assert(result.status() ==
           DefenseModeTransitionStatus::CHAIN_AUDIT_NOT_COMPLETE);
}

// Exit when audit not required: any chain audit height passes.
void testExitWithoutAuditRequiredPasses() {
    const auto result = DefenseModeTransitionValidator::validateExit(
        validExitRecord(0), false, 0
    );
    assert(result.isAccepted());
}

// Empty transitionId is rejected.
void testEmptyTransitionIdRejected() {
    const DefenseModeTransitionRecord record(
        "",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "reason",
        "",
        "",
        0,
        1900000001
    );
    assert(!record.isValid());
}

// Zero blockHeight is rejected.
void testZeroBlockHeightRejected() {
    const DefenseModeTransitionRecord record(
        "trans-zeroblk",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        0,  // zero blockHeight
        "reason",
        "",
        "",
        0,
        1900000001
    );
    assert(!record.isValid());
}

void testStatusToString() {
    assert(nodo::economics::defenseModeTransitionStatusToString(
               DefenseModeTransitionStatus::ACCEPTED) == "ACCEPTED");
    assert(nodo::economics::defenseModeTransitionStatusToString(
               DefenseModeTransitionStatus::CHAIN_AUDIT_NOT_COMPLETE) ==
           "CHAIN_AUDIT_NOT_COMPLETE");
    assert(nodo::economics::defenseModeTransitionStatusToString(
               DefenseModeTransitionStatus::MISSING_GOVERNANCE_CONTEXT) ==
           "MISSING_GOVERNANCE_CONTEXT");
}

} // namespace

int main() {
    testValidActivationAccepted();
    testActivationNoStateChangeRejected();
    testActivationWrongDirectionRejected();
    testGovernanceActivationRequiresProposalId();
    testGovernanceActivationWithProposalIdPasses();
    testValidExitWithSufficientAuditPasses();
    testExitWithInsufficientAuditFails();
    testExitWithoutAuditRequiredPasses();
    testEmptyTransitionIdRejected();
    testZeroBlockHeightRejected();
    testStatusToString();
    return 0;
}
