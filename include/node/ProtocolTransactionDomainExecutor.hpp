#ifndef NODO_NODE_PROTOCOL_TRANSACTION_DOMAIN_EXECUTOR_HPP
#define NODO_NODE_PROTOCOL_TRANSACTION_DOMAIN_EXECUTOR_HPP

#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/TransactionExecutionContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"
#include "economics/BurnRecord.hpp"

#include <memory>
#include <map>
#include <string>
#include <vector>

namespace nodo::node {

struct ProtocolExecutionState {
    GovernanceExecutor governance;
    core::ValidatorRegistry validators;
    consensus::ValidatorPenaltyLedger penaltyLedger;
    StakingRegistry staking;
    utils::Amount supply;
    std::vector<economics::BurnRecord> burns;
};

core::TransactionDomainExecutorFactory makeProtocolDomainExecutorFactory(
    ProtocolExecutionState initialState,
    core::ValidatorSetHistory validatorSetHistory,
    std::string networkName,
    std::shared_ptr<ProtocolExecutionState> resultTracker
);

std::map<std::string, std::string> protocolExecutionDomains(
    const ProtocolExecutionState& state
);

} // namespace nodo::node

#endif
