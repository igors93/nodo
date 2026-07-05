#ifndef NODO_NODE_PROTOCOL_TRANSACTION_DOMAIN_EXECUTOR_HPP
#define NODO_NODE_PROTOCOL_TRANSACTION_DOMAIN_EXECUTOR_HPP

#include "config/NetworkParameters.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/TransactionExecutionContext.hpp"
#include "core/ValidatorRegistry.hpp"
#include "economics/BurnRecord.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"

#include <map>
#include <memory>
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
    config::NetworkParameters networkParameters,
    std::shared_ptr<ProtocolExecutionState> resultTracker);

std::map<std::string, std::string>
protocolExecutionDomains(const ProtocolExecutionState &state);

} // namespace nodo::node

#endif
