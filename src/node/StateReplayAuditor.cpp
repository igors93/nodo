#include "node/StateReplayAuditor.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string stateReplayAuditStatusToString(
    StateReplayAuditStatus status
) {
    switch (status) {
        case StateReplayAuditStatus::VALID:
            return "VALID";
        case StateReplayAuditStatus::INVALID_INPUT:
            return "INVALID_INPUT";
        case StateReplayAuditStatus::ROOT_MISMATCH:
            return "ROOT_MISMATCH";
        default:
            return "INVALID_INPUT";
    }
}

StateReplayAuditResult::StateReplayAuditResult()
    : m_status(StateReplayAuditStatus::INVALID_INPUT),
      m_reason("Uninitialized state replay audit result."),
      m_expectedRoot(""),
      m_actualRoot("") {}

StateReplayAuditResult::StateReplayAuditResult(
    StateReplayAuditStatus status,
    std::string reason,
    std::string expectedRoot,
    std::string actualRoot
) : m_status(status),
    m_reason(std::move(reason)),
    m_expectedRoot(std::move(expectedRoot)),
    m_actualRoot(std::move(actualRoot)) {}

StateReplayAuditStatus StateReplayAuditResult::status() const {
    return m_status;
}

const std::string& StateReplayAuditResult::reason() const {
    return m_reason;
}

const std::string& StateReplayAuditResult::expectedRoot() const {
    return m_expectedRoot;
}

const std::string& StateReplayAuditResult::actualRoot() const {
    return m_actualRoot;
}

bool StateReplayAuditResult::valid() const {
    return m_status == StateReplayAuditStatus::VALID;
}

std::string StateReplayAuditResult::serialize() const {
    std::ostringstream output;
    output << "StateReplayAuditResult{"
           << "status=" << stateReplayAuditStatusToString(m_status)
           << ";reason=" << m_reason
           << ";expectedRoot=" << m_expectedRoot
           << ";actualRoot=" << m_actualRoot
           << "}";
    return output.str();
}

StateReplayAuditResult StateReplayAuditor::auditSnapshot(
    const StateSnapshot& snapshot,
    const core::AccountStateView& replayedAccountStateView,
    const core::ValidatorRegistry& replayedValidatorRegistry,
    const std::vector<core::LedgerRecord>& replayedLedgerRecords
) {
    if (!snapshot.isValid() ||
        !replayedAccountStateView.isValid() ||
        !replayedValidatorRegistry.isValid()) {
        return StateReplayAuditResult(
            StateReplayAuditStatus::INVALID_INPUT,
            "Snapshot or replayed state input is invalid.",
            snapshot.commitment().finalizedStateRoot(),
            ""
        );
    }

    const core::StateCommitment replayedCommitment =
        core::StateCommitment::calculate(
            snapshot.blockHeight(),
            snapshot.blockHash(),
            replayedAccountStateView,
            replayedLedgerRecords,
            replayedValidatorRegistry,
            snapshot.createdAt()
        );

    if (!replayedCommitment.isValid()) {
        return StateReplayAuditResult(
            StateReplayAuditStatus::INVALID_INPUT,
            "Replayed state commitment is invalid.",
            snapshot.commitment().finalizedStateRoot(),
            replayedCommitment.finalizedStateRoot()
        );
    }

    if (replayedCommitment.finalizedStateRoot() !=
        snapshot.commitment().finalizedStateRoot()) {
        return StateReplayAuditResult(
            StateReplayAuditStatus::ROOT_MISMATCH,
            "Replayed state root does not match snapshot commitment.",
            snapshot.commitment().finalizedStateRoot(),
            replayedCommitment.finalizedStateRoot()
        );
    }

    return StateReplayAuditResult(
        StateReplayAuditStatus::VALID,
        "Replayed state matches snapshot commitment.",
        snapshot.commitment().finalizedStateRoot(),
        replayedCommitment.finalizedStateRoot()
    );
}

} // namespace nodo::node
