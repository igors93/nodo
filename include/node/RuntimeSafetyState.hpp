#ifndef NODO_NODE_RUNTIME_SAFETY_STATE_HPP
#define NODO_NODE_RUNTIME_SAFETY_STATE_HPP

#include "economics/DefenseModeState.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

/*
 * RuntimeSafetyState is the persistent record of the node's current protocol
 * safety posture. It is loaded at startup and used by readiness checks and
 * economic gates to determine whether the node is operating in a safe state.
 *
 * Security principle:
 * A missing safety state file on a new data directory means the node has no
 * prior history, so the default (INACTIVE) is safe. A corrupt or unreadable
 * state must fail readiness — never silently treat it as INACTIVE.
 */
class RuntimeSafetyState {
public:
    RuntimeSafetyState();

    RuntimeSafetyState(
        economics::DefenseModeState defenseMode,
        economics::DefenseModeTrigger activationTrigger,
        std::uint64_t activationHeight,
        std::string activationReason,
        std::string evidenceId,
        std::uint64_t lastChainAuditHeight,
        bool exitRequiresChainAudit,
        std::int64_t updatedAt
    );

    economics::DefenseModeState defenseMode() const;
    economics::DefenseModeTrigger activationTrigger() const;
    std::uint64_t activationHeight() const;
    const std::string& activationReason() const;
    const std::string& evidenceId() const;
    std::uint64_t lastChainAuditHeight() const;
    bool exitRequiresChainAudit() const;
    std::int64_t updatedAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

    // Returns the safe canonical default for a new data directory.
    static RuntimeSafetyState newNodeDefault();

private:
    economics::DefenseModeState m_defenseMode;
    economics::DefenseModeTrigger m_activationTrigger;
    std::uint64_t m_activationHeight;
    std::string m_activationReason;
    std::string m_evidenceId;
    std::uint64_t m_lastChainAuditHeight;
    bool m_exitRequiresChainAudit;
    std::int64_t m_updatedAt;

    mutable bool m_validated;
    mutable bool m_valid;
    mutable std::string m_rejectionReason;

    void validate() const;
};

} // namespace nodo::node

#endif
