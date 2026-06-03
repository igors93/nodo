#ifndef NODO_ECONOMICS_DEFENSE_MODE_TRANSITION_RECORD_HPP
#define NODO_ECONOMICS_DEFENSE_MODE_TRANSITION_RECORD_HPP

#include "economics/DefenseModeState.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * DefenseModeTransitionRecord captures a single activation or exit event for
 * defense mode. Every state change must be bound to a verifiable cause:
 * critical evidence, chain audit, or verified governance vote.
 *
 * Security principle:
 * Defense mode cannot be toggled without a record. Activation requires a
 * trigger and a reason. Exit via chain audit requires a valid audit height.
 * Governance-triggered transitions require a governance proposal id.
 */
class DefenseModeTransitionRecord {
public:
    DefenseModeTransitionRecord();

    DefenseModeTransitionRecord(
        std::string transitionId,
        DefenseModeState fromState,
        DefenseModeState toState,
        DefenseModeTrigger trigger,
        std::uint64_t blockHeight,
        std::string reason,
        std::string evidenceId,
        std::string governanceProposalId,
        std::uint64_t chainAuditHeight,
        std::int64_t createdAt
    );

    const std::string& transitionId() const;
    DefenseModeState fromState() const;
    DefenseModeState toState() const;
    DefenseModeTrigger trigger() const;
    std::uint64_t blockHeight() const;
    const std::string& reason() const;
    const std::string& evidenceId() const;
    const std::string& governanceProposalId() const;
    std::uint64_t chainAuditHeight() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_transitionId;
    DefenseModeState m_fromState;
    DefenseModeState m_toState;
    DefenseModeTrigger m_trigger;
    std::uint64_t m_blockHeight;
    std::string m_reason;
    std::string m_evidenceId;
    std::string m_governanceProposalId;
    std::uint64_t m_chainAuditHeight;
    std::int64_t m_createdAt;

    mutable bool m_validated;
    mutable bool m_valid;
    mutable std::string m_rejectionReason;

    void validate() const;
};

enum class DefenseModeTransitionStatus {
    ACCEPTED,
    INVALID_RECORD,
    MISSING_EVIDENCE_FOR_ACTIVATION,
    MISSING_GOVERNANCE_CONTEXT,
    CHAIN_AUDIT_NOT_COMPLETE,
    NO_STATE_CHANGE
};

std::string defenseModeTransitionStatusToString(DefenseModeTransitionStatus status);

class DefenseModeTransitionResult {
public:
    DefenseModeTransitionResult();

    static DefenseModeTransitionResult accepted();
    static DefenseModeTransitionResult rejected(
        DefenseModeTransitionStatus status,
        std::string reason
    );

    bool isAccepted() const;
    DefenseModeTransitionStatus status() const;
    const std::string& reason() const;

private:
    DefenseModeTransitionStatus m_status;
    std::string m_reason;
};

/*
 * DefenseModeTransitionValidator enforces that activation records carry
 * evidence and exit records carry a completed chain audit when required.
 *
 * Security principle:
 * Any activation without evidence or governance context is rejected.
 * Any exit without a chain audit when policy requires it is rejected.
 */
class DefenseModeTransitionValidator {
public:
    // Validate an activation (INACTIVE → ACTIVE) record.
    static DefenseModeTransitionResult validateActivation(
        const DefenseModeTransitionRecord& record
    );

    // Validate an exit (ACTIVE → INACTIVE) record.
    // auditRequiredByPolicy: whether the current policy mandates a chain audit.
    // minimumRequiredAuditHeight: the height up to which the audit must have run.
    static DefenseModeTransitionResult validateExit(
        const DefenseModeTransitionRecord& record,
        bool auditRequiredByPolicy,
        std::uint64_t minimumRequiredAuditHeight
    );
};

} // namespace nodo::economics

#endif
