#include "node/RuntimeSafetyState.hpp"

#include <sstream>

namespace nodo::node {

RuntimeSafetyState::RuntimeSafetyState()
    : m_defenseMode(economics::DefenseModeState::INACTIVE),
      m_activationTrigger(economics::DefenseModeTrigger::OPERATOR_DECLARED),
      m_activationHeight(0),
      m_activationReason(""),
      m_evidenceId(""),
      m_governanceProposalId(""),
      m_lastChainAuditHeight(0),
      m_exitRequiresChainAudit(true),
      m_updatedAt(0),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

RuntimeSafetyState::RuntimeSafetyState(
    economics::DefenseModeState defenseMode,
    economics::DefenseModeTrigger activationTrigger,
    std::uint64_t activationHeight,
    std::string activationReason,
    std::string evidenceId,
    std::string governanceProposalId,
    std::uint64_t lastChainAuditHeight,
    bool exitRequiresChainAudit,
    std::int64_t updatedAt
)
    : m_defenseMode(defenseMode),
      m_activationTrigger(activationTrigger),
      m_activationHeight(activationHeight),
      m_activationReason(std::move(activationReason)),
      m_evidenceId(std::move(evidenceId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_lastChainAuditHeight(lastChainAuditHeight),
      m_exitRequiresChainAudit(exitRequiresChainAudit),
      m_updatedAt(updatedAt),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

economics::DefenseModeState RuntimeSafetyState::defenseMode() const {
    return m_defenseMode;
}

economics::DefenseModeTrigger RuntimeSafetyState::activationTrigger() const {
    return m_activationTrigger;
}

std::uint64_t RuntimeSafetyState::activationHeight() const {
    return m_activationHeight;
}

const std::string& RuntimeSafetyState::activationReason() const {
    return m_activationReason;
}

const std::string& RuntimeSafetyState::evidenceId() const {
    return m_evidenceId;
}

const std::string& RuntimeSafetyState::governanceProposalId() const {
    return m_governanceProposalId;
}

std::uint64_t RuntimeSafetyState::lastChainAuditHeight() const {
    return m_lastChainAuditHeight;
}

bool RuntimeSafetyState::exitRequiresChainAudit() const {
    return m_exitRequiresChainAudit;
}

std::int64_t RuntimeSafetyState::updatedAt() const {
    return m_updatedAt;
}

bool RuntimeSafetyState::isValid() const {
    if (!m_validated) {
        validate();
    }
    return m_valid;
}

const std::string& RuntimeSafetyState::rejectionReason() const {
    if (!m_validated) {
        validate();
    }
    return m_rejectionReason;
}

std::string RuntimeSafetyState::serialize() const {
    std::ostringstream oss;
    oss << "RuntimeSafetyState{"
        << "defenseMode=" << economics::defenseModeStateToString(m_defenseMode)
        << ";activationTrigger=" << economics::defenseModeTriggerToString(m_activationTrigger)
        << ";activationHeight=" << m_activationHeight
        << ";activationReason=" << m_activationReason
        << ";evidenceId=" << m_evidenceId
        << ";governanceProposalId=" << m_governanceProposalId
        << ";lastChainAuditHeight=" << m_lastChainAuditHeight
        << ";exitRequiresChainAudit=" << (m_exitRequiresChainAudit ? "true" : "false")
        << ";updatedAt=" << m_updatedAt
        << "}";
    return oss.str();
}

RuntimeSafetyState RuntimeSafetyState::newNodeDefault() {
    return RuntimeSafetyState(
        economics::DefenseModeState::INACTIVE,
        economics::DefenseModeTrigger::OPERATOR_DECLARED,
        0,
        "",
        "",
        "",
        0,
        true,
        0
    );
}

static bool isCriticalTrigger(economics::DefenseModeTrigger trigger) {
    return trigger == economics::DefenseModeTrigger::SUPPLY_DIVERGENCE ||
           trigger == economics::DefenseModeTrigger::DOUBLE_SIGN_MASS_EVENT ||
           trigger == economics::DefenseModeTrigger::UNAUTHORIZED_TREASURY_SPEND_ATTEMPT ||
           trigger == economics::DefenseModeTrigger::CHAIN_AUDIT_FAILURE ||
           trigger == economics::DefenseModeTrigger::STORAGE_CORRUPTION;
}

void RuntimeSafetyState::validate() const {
    m_validated = true;

    if (m_updatedAt < 0) {
        m_valid = false;
        m_rejectionReason = "updatedAt must not be negative";
        return;
    }

    if (m_defenseMode == economics::DefenseModeState::ACTIVE) {
        if (m_activationHeight == 0) {
            m_valid = false;
            m_rejectionReason =
                "activationHeight must be non-zero when defense mode is ACTIVE";
            return;
        }
        if (m_activationReason.empty()) {
            m_valid = false;
            m_rejectionReason =
                "activationReason must not be empty when defense mode is ACTIVE";
            return;
        }
        if (m_updatedAt <= 0) {
            m_valid = false;
            m_rejectionReason =
                "updatedAt must be positive when defense mode is ACTIVE";
            return;
        }

        // Critical triggers require a traceable evidence or audit reference.
        if (isCriticalTrigger(m_activationTrigger) && m_evidenceId.empty()) {
            m_valid = false;
            m_rejectionReason =
                "evidenceId must not be empty for critical trigger '" +
                economics::defenseModeTriggerToString(m_activationTrigger) +
                "' when defense mode is ACTIVE";
            return;
        }

        // Governance-triggered activation requires a governance proposal context.
        if (m_activationTrigger == economics::DefenseModeTrigger::GOVERNANCE_VOTED &&
            m_governanceProposalId.empty()) {
            m_valid = false;
            m_rejectionReason =
                "governanceProposalId must not be empty for GOVERNANCE_VOTED "
                "trigger when defense mode is ACTIVE";
            return;
        }
    } else {
        // INACTIVE: activationHeight must be zero.
        if (m_activationHeight != 0) {
            m_valid = false;
            m_rejectionReason =
                "activationHeight must be zero when defense mode is INACTIVE";
            return;
        }
    }

    m_valid = true;
    m_rejectionReason = "";
}

} // namespace nodo::node
