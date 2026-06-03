#include "economics/DefenseModeTransitionRecord.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::DefenseModeState;
using nodo::economics::DefenseModeTrigger;
using nodo::economics::DefenseModeTransitionRecord;
using nodo::economics::DefenseModeTransitionStatus;
using nodo::economics::DefenseModeTransitionValidator;

DefenseModeTransitionRecord criticalActivationWithEvidence(
    DefenseModeTrigger trigger,
    const std::string& evidenceId
) {
    return DefenseModeTransitionRecord(
        "trans-crit-001",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        trigger,
        10,
        "critical event detected",
        evidenceId,
        "",
        0,
        1900000001
    );
}

// Critical trigger with evidence is accepted.
void testCriticalTriggerWithEvidenceAccepted() {
    const auto triggers = {
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT,
        DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        DefenseModeTrigger::STORAGE_CORRUPTION
    };
    for (const auto trigger : triggers) {
        const auto record = criticalActivationWithEvidence(trigger, "ev-001");
        const auto result =
            DefenseModeTransitionValidator::validateActivation(record);
        assert(result.isAccepted());
    }
}

// Critical trigger without evidence or audit reference is rejected.
void testCriticalTriggerWithoutEvidenceRejected() {
    const auto triggers = {
        DefenseModeTrigger::SUPPLY_DIVERGENCE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        DefenseModeTrigger::STORAGE_CORRUPTION
    };
    for (const auto trigger : triggers) {
        const DefenseModeTransitionRecord record(
            "trans-no-ev",
            DefenseModeState::INACTIVE,
            DefenseModeState::ACTIVE,
            trigger,
            10,
            "critical trigger but no evidence",
            "",  // empty evidenceId
            "",
            0,   // zero chainAuditHeight
            1900000001
        );
        const auto result =
            DefenseModeTransitionValidator::validateActivation(record);
        assert(!result.isAccepted());
        assert(result.status() ==
               DefenseModeTransitionStatus::MISSING_EVIDENCE_FOR_ACTIVATION);
    }
}

// Critical trigger with chain audit height (not evidence id) is accepted.
void testCriticalTriggerWithAuditHeightAccepted() {
    const DefenseModeTransitionRecord record(
        "trans-crit-audit",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::CHAIN_AUDIT_FAILURE,
        10,
        "chain audit failed at height 10",
        "",   // no evidenceId
        "",
        8,    // non-zero chainAuditHeight satisfies the requirement
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(record);
    assert(result.isAccepted());
}

// OPERATOR_DECLARED does not require evidence (not a critical trigger).
void testOperatorDeclaredWithoutEvidenceAccepted() {
    const DefenseModeTransitionRecord record(
        "trans-op",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        10,
        "operator declared defense mode",
        "",  // no evidence required for OPERATOR_DECLARED
        "",
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(record);
    assert(result.isAccepted());
}

// Governance-voted activation requires proposalId.
void testGovernanceVotedRequiresProposalId() {
    const DefenseModeTransitionRecord noProposal(
        "trans-gov-no-prop",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        10,
        "governance voted",
        "",
        "",  // missing proposalId
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(noProposal);
    assert(!result.isAccepted());
    assert(result.status() ==
           DefenseModeTransitionStatus::MISSING_GOVERNANCE_CONTEXT);
}

// Governance-voted activation with proposalId is accepted.
void testGovernanceVotedWithProposalIdAccepted() {
    const DefenseModeTransitionRecord withProposal(
        "trans-gov-ok",
        DefenseModeState::INACTIVE,
        DefenseModeState::ACTIVE,
        DefenseModeTrigger::GOVERNANCE_VOTED,
        10,
        "governance voted",
        "",
        "gov-prop-defense-001",  // proposalId present
        0,
        1900000001
    );
    const auto result =
        DefenseModeTransitionValidator::validateActivation(withProposal);
    assert(result.isAccepted());
}

// Exit with sufficient audit passes.
void testExitWithSufficientAuditPasses() {
    const DefenseModeTransitionRecord exitRecord(
        "trans-exit",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        20,
        "audit completed",
        "",
        "",
        15,
        1900000002
    );
    const auto result =
        DefenseModeTransitionValidator::validateExit(exitRecord, true, 10);
    assert(result.isAccepted());
}

// Exit with insufficient audit fails.
void testExitWithInsufficientAuditFails() {
    const DefenseModeTransitionRecord exitRecord(
        "trans-exit-low",
        DefenseModeState::ACTIVE,
        DefenseModeState::INACTIVE,
        DefenseModeTrigger::OPERATOR_DECLARED,
        20,
        "audit attempted",
        "",
        "",
        5,   // too low
        1900000002
    );
    const auto result =
        DefenseModeTransitionValidator::validateExit(exitRecord, true, 10);
    assert(!result.isAccepted());
    assert(result.status() ==
           DefenseModeTransitionStatus::CHAIN_AUDIT_NOT_COMPLETE);
}

} // namespace

int main() {
    testCriticalTriggerWithEvidenceAccepted();
    testCriticalTriggerWithoutEvidenceRejected();
    testCriticalTriggerWithAuditHeightAccepted();
    testOperatorDeclaredWithoutEvidenceAccepted();
    testGovernanceVotedRequiresProposalId();
    testGovernanceVotedWithProposalIdAccepted();
    testExitWithSufficientAuditPasses();
    testExitWithInsufficientAuditFails();
    return 0;
}
