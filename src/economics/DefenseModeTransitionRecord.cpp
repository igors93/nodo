#include "economics/DefenseModeTransitionRecord.hpp"

#include <sstream>

namespace nodo::economics {

namespace {

bool isSafeIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (const char c : value) {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

} // namespace

DefenseModeTransitionRecord::DefenseModeTransitionRecord()
    : m_transitionId(""),
      m_fromState(DefenseModeState::INACTIVE),
      m_toState(DefenseModeState::INACTIVE),
      m_trigger(DefenseModeTrigger::OPERATOR_DECLARED),
      m_blockHeight(0),
      m_reason(""),
      m_evidenceId(""),
      m_governanceProposalId(""),
      m_chainAuditHeight(0),
      m_createdAt(0),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

DefenseModeTransitionRecord::DefenseModeTransitionRecord(
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
)
    : m_transitionId(std::move(transitionId)),
      m_fromState(fromState),
      m_toState(toState),
      m_trigger(trigger),
      m_blockHeight(blockHeight),
      m_reason(std::move(reason)),
      m_evidenceId(std::move(evidenceId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_chainAuditHeight(chainAuditHeight),
      m_createdAt(createdAt),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

const std::string& DefenseModeTransitionRecord::transitionId() const {
    return m_transitionId;
}

DefenseModeState DefenseModeTransitionRecord::fromState() const {
    return m_fromState;
}

DefenseModeState DefenseModeTransitionRecord::toState() const {
    return m_toState;
}

DefenseModeTrigger DefenseModeTransitionRecord::trigger() const {
    return m_trigger;
}

std::uint64_t DefenseModeTransitionRecord::blockHeight() const {
    return m_blockHeight;
}

const std::string& DefenseModeTransitionRecord::reason() const {
    return m_reason;
}

const std::string& DefenseModeTransitionRecord::evidenceId() const {
    return m_evidenceId;
}

const std::string& DefenseModeTransitionRecord::governanceProposalId() const {
    return m_governanceProposalId;
}

std::uint64_t DefenseModeTransitionRecord::chainAuditHeight() const {
    return m_chainAuditHeight;
}

std::int64_t DefenseModeTransitionRecord::createdAt() const {
    return m_createdAt;
}

bool DefenseModeTransitionRecord::isValid() const {
    if (!m_validated) {
        validate();
    }
    return m_valid;
}

const std::string& DefenseModeTransitionRecord::rejectionReason() const {
    if (!m_validated) {
        validate();
    }
    return m_rejectionReason;
}

std::string DefenseModeTransitionRecord::serialize() const {
    std::ostringstream oss;
    oss << "DefenseModeTransitionRecord{"
        << "transitionId=" << m_transitionId
        << ";fromState=" << defenseModeStateToString(m_fromState)
        << ";toState=" << defenseModeStateToString(m_toState)
        << ";trigger=" << defenseModeTriggerToString(m_trigger)
        << ";blockHeight=" << m_blockHeight
        << ";reason=" << m_reason
        << ";evidenceId=" << m_evidenceId
        << ";governanceProposalId=" << m_governanceProposalId
        << ";chainAuditHeight=" << m_chainAuditHeight
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

void DefenseModeTransitionRecord::validate() const {
    m_validated = true;

    if (!isSafeIdentifier(m_transitionId)) {
        m_valid = false;
        m_rejectionReason = "transitionId is empty or contains unsafe characters";
        return;
    }

    if (m_fromState == m_toState) {
        m_valid = false;
        m_rejectionReason = "fromState and toState must differ";
        return;
    }

    if (m_blockHeight == 0) {
        m_valid = false;
        m_rejectionReason = "blockHeight must be non-zero";
        return;
    }

    if (m_reason.empty()) {
        m_valid = false;
        m_rejectionReason = "reason must not be empty";
        return;
    }

    if (m_createdAt <= 0) {
        m_valid = false;
        m_rejectionReason = "createdAt must be positive";
        return;
    }

    m_valid = true;
    m_rejectionReason = "";
}

// --- DefenseModeTransitionResult ---

std::string defenseModeTransitionStatusToString(
    DefenseModeTransitionStatus status
) {
    switch (status) {
        case DefenseModeTransitionStatus::ACCEPTED:
            return "ACCEPTED";
        case DefenseModeTransitionStatus::INVALID_RECORD:
            return "INVALID_RECORD";
        case DefenseModeTransitionStatus::MISSING_EVIDENCE_FOR_ACTIVATION:
            return "MISSING_EVIDENCE_FOR_ACTIVATION";
        case DefenseModeTransitionStatus::MISSING_GOVERNANCE_CONTEXT:
            return "MISSING_GOVERNANCE_CONTEXT";
        case DefenseModeTransitionStatus::CHAIN_AUDIT_NOT_COMPLETE:
            return "CHAIN_AUDIT_NOT_COMPLETE";
        case DefenseModeTransitionStatus::NO_STATE_CHANGE:
            return "NO_STATE_CHANGE";
        default:
            return "UNKNOWN";
    }
}

DefenseModeTransitionResult::DefenseModeTransitionResult()
    : m_status(DefenseModeTransitionStatus::INVALID_RECORD),
      m_reason("") {}

DefenseModeTransitionResult DefenseModeTransitionResult::accepted() {
    DefenseModeTransitionResult r;
    r.m_status = DefenseModeTransitionStatus::ACCEPTED;
    r.m_reason = "";
    return r;
}

DefenseModeTransitionResult DefenseModeTransitionResult::rejected(
    DefenseModeTransitionStatus status,
    std::string reason
) {
    DefenseModeTransitionResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool DefenseModeTransitionResult::isAccepted() const {
    return m_status == DefenseModeTransitionStatus::ACCEPTED;
}

DefenseModeTransitionStatus DefenseModeTransitionResult::status() const {
    return m_status;
}

const std::string& DefenseModeTransitionResult::reason() const {
    return m_reason;
}

// --- DefenseModeTransitionValidator ---

DefenseModeTransitionResult DefenseModeTransitionValidator::validateActivation(
    const DefenseModeTransitionRecord& record
) {
    if (!record.isValid()) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::INVALID_RECORD,
            "activation record is invalid: " + record.rejectionReason()
        );
    }

    if (record.fromState() != DefenseModeState::INACTIVE ||
        record.toState() != DefenseModeState::ACTIVE) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::NO_STATE_CHANGE,
            "activation record must transition INACTIVE -> ACTIVE"
        );
    }

    // Governance-voted activation requires a governance proposal id.
    if (record.trigger() == DefenseModeTrigger::GOVERNANCE_VOTED &&
        record.governanceProposalId().empty()) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::MISSING_GOVERNANCE_CONTEXT,
            "GOVERNANCE_VOTED activation requires a non-empty governanceProposalId"
        );
    }

    // Critical triggers require evidence or a chain audit reference so that
    // activation can be independently verified.
    const bool isCriticalTrigger =
        record.trigger() == DefenseModeTrigger::SUPPLY_DIVERGENCE ||
        record.trigger() == DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT ||
        record.trigger() == DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT ||
        record.trigger() == DefenseModeTrigger::CHAIN_AUDIT_FAILURE ||
        record.trigger() == DefenseModeTrigger::STORAGE_CORRUPTION;

    if (isCriticalTrigger &&
        record.evidenceId().empty() &&
        record.chainAuditHeight() == 0) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::MISSING_EVIDENCE_FOR_ACTIVATION,
            "critical trigger '" +
            defenseModeTriggerToString(record.trigger()) +
            "' requires non-empty evidenceId or non-zero chainAuditHeight"
        );
    }

    return DefenseModeTransitionResult::accepted();
}

DefenseModeTransitionResult DefenseModeTransitionValidator::validateExit(
    const DefenseModeTransitionRecord& record,
    bool auditRequiredByPolicy,
    std::uint64_t minimumRequiredAuditHeight
) {
    if (!record.isValid()) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::INVALID_RECORD,
            "exit record is invalid: " + record.rejectionReason()
        );
    }

    if (record.fromState() != DefenseModeState::ACTIVE ||
        record.toState() != DefenseModeState::INACTIVE) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::NO_STATE_CHANGE,
            "exit record must transition ACTIVE -> INACTIVE"
        );
    }

    if (auditRequiredByPolicy &&
        record.chainAuditHeight() < minimumRequiredAuditHeight) {
        return DefenseModeTransitionResult::rejected(
            DefenseModeTransitionStatus::CHAIN_AUDIT_NOT_COMPLETE,
            "chain audit is required for exit: auditHeight=" +
            std::to_string(record.chainAuditHeight()) +
            " minimumRequired=" +
            std::to_string(minimumRequiredAuditHeight)
        );
    }

    return DefenseModeTransitionResult::accepted();
}

} // namespace nodo::economics
