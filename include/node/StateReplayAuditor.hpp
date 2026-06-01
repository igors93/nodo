#ifndef NODO_NODE_STATE_REPLAY_AUDITOR_HPP
#define NODO_NODE_STATE_REPLAY_AUDITOR_HPP

#include "core/AccountStateView.hpp"
#include "core/LedgerRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/StateSnapshot.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class StateReplayAuditStatus {
    VALID,
    INVALID_INPUT,
    ROOT_MISMATCH
};

std::string stateReplayAuditStatusToString(
    StateReplayAuditStatus status
);

class StateReplayAuditResult {
public:
    StateReplayAuditResult();

    StateReplayAuditResult(
        StateReplayAuditStatus status,
        std::string reason,
        std::string expectedRoot,
        std::string actualRoot
    );

    StateReplayAuditStatus status() const;
    const std::string& reason() const;
    const std::string& expectedRoot() const;
    const std::string& actualRoot() const;
    bool valid() const;
    std::string serialize() const;

private:
    StateReplayAuditStatus m_status;
    std::string m_reason;
    std::string m_expectedRoot;
    std::string m_actualRoot;
};

class StateReplayAuditor {
public:
    static StateReplayAuditResult auditSnapshot(
        const StateSnapshot& snapshot,
        const core::AccountStateView& replayedAccountStateView,
        const core::ValidatorRegistry& replayedValidatorRegistry,
        const std::vector<core::LedgerRecord>& replayedLedgerRecords
    );
};

} // namespace nodo::node

#endif
