#include "node/RuntimeSafetyState.hpp"

#include <sstream>

namespace nodo::node {

RuntimeSafetyState::RuntimeSafetyState()
    : m_defenseMode(economics::DefenseModeState::INACTIVE),
      m_activationTrigger(economics::DefenseModeTrigger::OPERATOR_DECLARED),
      m_activationHeight(0),
      m_activationReason(""),
      m_evidenceId(""),
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
    std::uint64_t lastChainAuditHeight,
    bool exitRequiresChainAudit,
    std::int64_t updatedAt
)
    : m_defenseMode(defenseMode),
      m_activationTrigger(activationTrigger),
      m_activationHeight(activationHeight),
      m_activationReason(std::move(activationReason)),
      m_evidenceId(std::move(evidenceId)),
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
        0,
        true,
        0
    );
}

void RuntimeSafetyState::validate() const {
    m_validated = true;

    // A zero updatedAt is acceptable only for a new-node default (activationHeight=0).
    if (m_updatedAt < 0) {
        m_valid = false;
        m_rejectionReason = "updatedAt must not be negative";
        return;
    }

    // activationHeight must be zero when defense mode is INACTIVE.
    if (m_defenseMode == economics::DefenseModeState::INACTIVE &&
        m_activationHeight != 0) {
        m_valid = false;
        m_rejectionReason =
            "activationHeight must be zero when defense mode is INACTIVE";
        return;
    }

    m_valid = true;
    m_rejectionReason = "";
}

} // namespace nodo::node
