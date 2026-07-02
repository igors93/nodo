#include "node/ProtocolStateTransition.hpp"

#include "config/NetworkParameters.hpp"
#include "core/State.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/StateTransitionEngine.hpp"
#include "economics/StakeAccount.hpp"
#include "node/FeeEconomics.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/ValidatorLifecycle.hpp"
#include "node/ValidatorStakeWeightUpdater.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::vector<economics::BurnRecord> finalizedBurns(const RuntimeSupplyState& supply) {
    std::vector<economics::BurnRecord> burns;
    for (const auto& delta : supply.finalizedDeltas()) {
        burns.insert(burns.end(), delta.burnRecords().begin(), delta.burnRecords().end());
    }
    return burns;
}

core::AccountStateView credit(
    core::AccountStateView accounts,
    const std::string& address,
    utils::Amount amount
) {
    if (amount.isZero()) return accounts;
    const core::AccountState current = accounts.accountOrDefault(address);
    if (!accounts.putAccount(core::AccountState(
            address, current.balance() + amount, current.nonce()))) {
        throw std::logic_error("Fee settlement produced an invalid account.");
    }
    return accounts;
}

core::AccountStateView accountViewFromAccounts(
    const std::vector<core::AccountState>& accounts
) {
    core::AccountStateView view;
    for (const core::AccountState& account : accounts) {
        if (!view.putAccount(account)) {
            throw std::logic_error("State replay produced an invalid account view.");
        }
    }
    return view;
}

std::int64_t checkedMinimumFee(std::uint64_t raw) {
    if (raw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Minimum fee exceeds supported Amount range.");
    }
    return static_cast<std::int64_t>(raw);
}

core::ValidatorSetHistory initialValidatorSetHistory(
    const core::ValidatorRegistry& validators
) {
    core::ValidatorSetHistory history;
    if (!history.recordSet(1, validators)) {
        throw std::logic_error("Unable to record genesis validator set for replay.");
    }
    return history;
}

void seedGenesisStakeAccounts(
    StakingRegistry& staking,
    const core::ValidatorRegistry& validators
) {
    for (const std::string& address : validators.activeValidatorAddresses()) {
        const core::ValidatorRegistryEntry* entry =
            validators.entryForAddress(address);
        if (entry != nullptr && entry->stakeAmount() > 0 &&
            !staking.hasAccount(address)) {
            staking.setAccount(address, economics::StakeAccount(
                address,
                utils::Amount::fromRawUnits(
                    static_cast<std::int64_t>(entry->stakeAmount())
                )
            ));
        }
    }
}

} // namespace

core::AccountStateView ProtocolStateTransition::initialAccountStateView(
    const config::GenesisConfig& genesisConfig
) {
    core::AccountStateView view;

    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot build account state from invalid genesis config.");
    }

    for (const config::GenesisAccountConfig& account :
         genesisConfig.genesisAccounts()) {
        if (!view.putAccount(
                core::AccountState(
                    account.address(),
                    account.balance(),
                    account.nonce()
                )
            )) {
            throw std::logic_error("Genesis account allocation produced invalid account state.");
        }
    }

    return view;
}

ProtocolExecutionState ProtocolStateTransition::initialExecutionState(
    const config::GenesisConfig& genesisConfig
) {
    const config::GenesisBuildResult genesis =
        config::GenesisBuilder::build(genesisConfig);
    if (!genesis.built()) {
        throw std::invalid_argument(
            "Cannot build protocol replay state from invalid genesis: " +
            genesis.reason()
        );
    }

    StakingRegistry staking;
    seedGenesisStakeAccounts(staking, genesis.validatorRegistry());

    return ProtocolExecutionState{
        GovernanceExecutor(),
        genesis.validatorRegistry(),
        consensus::ValidatorPenaltyLedger(),
        staking,
        MonetaryFirewall::genesisSupply(genesisConfig),
        {}
    };
}

ProtocolReplayState ProtocolStateTransition::initialReplayState(
    const config::GenesisConfig& genesisConfig
) {
    const ProtocolExecutionState execution = initialExecutionState(genesisConfig);
    ProtocolReplayState state;
    state.accounts = initialAccountStateView(genesisConfig);
    state.execution = execution;
    state.validatorSetHistory = initialValidatorSetHistory(execution.validators);
    state.stateRoot = core::StateRootCalculator::calculateProtocolStateRoot(
        state.accounts,
        protocolExecutionDomains(state.execution)
    );
    if (state.stateRoot.empty()) {
        throw std::logic_error("Genesis protocol replay produced an empty state root.");
    }
    return state;
}

void ProtocolStateTransition::applyValidatorEpochTransition(
    core::ValidatorRegistry& validators,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp
) {
    if (blockHeight == 0 || blockHeight % NODO_VALIDATOR_EPOCH_BLOCKS != 0) return;
    const std::uint64_t effectiveEpoch =
        ValidatorStakeWeightUpdater::effectiveEpochForBoundary(blockHeight);
    for (const std::string& address : validators.pendingValidatorAddresses()) {
        const auto* entry = validators.entryForAddress(address);
        if (entry != nullptr &&
            entry->registrationRecord().activationEpoch() <= effectiveEpoch &&
            entry->stakeAmount() >=
                core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS &&
            entry->consensusWeight() > 0) {
            const auto result = validators.activateValidator(
                address,
                effectiveEpoch,
                blockTimestamp
            );
            if (!result.success()) throw std::logic_error("Projected validator activation failed.");
        }
    }
    for (const std::string& address : validators.exitRequestedValidatorAddresses()) {
        const auto* entry = validators.entryForAddress(address);
        if (entry != nullptr &&
            entry->exitRequestHeight() + NODO_VALIDATOR_EPOCH_BLOCKS <= blockHeight) {
            const auto result = validators.completeExit(address, blockTimestamp);
            if (!result.success()) throw std::logic_error("Projected validator exit failed.");
        }
    }
}

core::AccountStateView ProtocolStateTransition::settleFees(
    const core::AccountStateView& accounts,
    std::uint64_t blockHeight,
    utils::Amount totalFee
) {
    const FeeEconomicBalance balance =
        FeeEconomics::buildFeeEconomicBalance(blockHeight, totalFee);
    core::AccountStateView settled = credit(
        accounts, core::State::feePoolAddress(), balance.validatorRewardAmount());
    return credit(
        std::move(settled), ProtectionTreasury::TREASURY_ADDRESS,
        balance.treasuryAmount());
}

core::DeterministicStateDomainTransition
ProtocolStateTransition::accountSettlementForReplay(std::uint64_t blockHeight) {
    return [blockHeight](
        const core::AccountStateView& accounts,
        utils::Amount totalFee,
        const std::vector<core::Transaction>&,
        const std::vector<core::LedgerRecord>&,
        std::int64_t
    ) {
        try {
            return core::DeterministicStateTransitionResult::accepted(
                settleFees(accounts, blockHeight, totalFee), {}
            );
        } catch (const std::exception& error) {
            return core::DeterministicStateTransitionResult::rejected(error.what());
        }
    };
}

core::StateTransitionPreviewContext ProtocolStateTransition::contextFromReplayState(
    const config::GenesisConfig& genesisConfig,
    const ProtocolReplayState& state,
    std::int64_t minimumFeeRawUnits,
    std::shared_ptr<ProtocolExecutionState> resultTracker,
    std::int64_t wallClockNow
) {
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Minimum fee cannot be negative in protocol replay context.");
    }
    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot build protocol replay context from invalid genesis config.");
    }
    if (!state.accounts.isValid()) {
        throw std::invalid_argument("Cannot build protocol replay context from invalid account state.");
    }
    if (!state.execution.validators.isValid() ||
        !state.execution.penaltyLedger.isValid() ||
        !state.validatorSetHistory.isValid()) {
        throw std::invalid_argument("Cannot build protocol replay context from invalid protocol domains.");
    }
    if (!resultTracker) {
        throw std::invalid_argument("Protocol replay context requires a result tracker.");
    }

    const auto& params = genesisConfig.networkParameters();
    return core::StateTransitionPreviewContext(
        minimumFeeRawUnits,
        state.accounts,
        false,
        true,
        "",
        wallClockNow,
        params.chainId(),
        params.networkName(),
        protocolExecutionDomains(state.execution),
        {},
        makeProtocolDomainExecutorFactory(
            state.execution,
            state.validatorSetHistory,
            params.chainId(),
            params.networkName(),
            std::move(resultTracker)
        ),
        true
    );
}

ProtocolReplayState ProtocolStateTransition::replayBlock(
    const config::GenesisConfig& genesisConfig,
    const ProtocolReplayState& previousState,
    const core::Block& block,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    if (block.isGenesisBlock()) {
        throw std::invalid_argument("Protocol replayBlock cannot replay genesis as a finalized transition.");
    }

    auto tracker = std::make_shared<ProtocolExecutionState>(previousState.execution);
    const core::StateTransitionPreviewContext context = contextFromReplayState(
        genesisConfig,
        previousState,
        minimumFeeRawUnits,
        tracker,
        wallClockNow
    );

    const core::StateTransitionPreviewResult execution =
        core::StateTransitionEngine::executeBlock(block, context);
    if (!execution.accepted()) {
        throw std::logic_error(
            "Canonical protocol replay rejected block " +
            std::to_string(block.index()) + ": " + execution.reason()
        );
    }
    if (execution.stateRoot().empty() || execution.receiptsRoot().empty()) {
        throw std::logic_error("Canonical protocol replay produced empty commitments.");
    }

    ProtocolReplayState next;
    next.accounts = accountViewFromAccounts(execution.resultingAccounts());
    next.execution = *tracker;
    next.validatorSetHistory = previousState.validatorSetHistory;
    if (!next.validatorSetHistory.recordSet(block.index() + 1, next.execution.validators)) {
        throw std::logic_error(
            "Validator set history conflict after replaying block " +
            std::to_string(block.index())
        );
    }
    next.stateRoot = execution.stateRoot();
    next.receiptsRoot = execution.receiptsRoot();
    next.totalFee = execution.totalFee();
    return next;
}

ProtocolReplayState ProtocolStateTransition::replayToTip(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    std::int64_t minimumFeeRawUnits
) {
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Minimum fee cannot be negative when replaying protocol state.");
    }
    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot replay protocol state from invalid genesis config.");
    }
    if (blockchain.empty() || !blockchain.isValid(false)) {
        throw std::invalid_argument("Cannot replay protocol state from invalid blockchain.");
    }

    const config::GenesisBuildResult genesis =
        config::GenesisBuilder::build(genesisConfig);
    if (!genesis.built()) {
        throw std::invalid_argument(
            "Cannot verify replay genesis chain: " + genesis.reason()
        );
    }
    if (genesis.blockchain().latestBlock().hash() !=
        blockchain.blocks().front().hash()) {
        throw std::invalid_argument("Blockchain genesis block does not match genesis config.");
    }

    ProtocolReplayState state = initialReplayState(genesisConfig);
    for (const core::Block& block : blockchain.blocks()) {
        if (block.isGenesisBlock()) {
            continue;
        }
        state = replayBlock(
            genesisConfig,
            state,
            block,
            minimumFeeRawUnits,
            block.timestamp()
        );
    }
    return state;
}

ProtocolReplayState ProtocolStateTransition::replayNextBlock(
    const NodeRuntime& runtime,
    const core::Block& block,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    const ProtocolReplayState tip = replayToTip(
        runtime.config().genesisConfig(),
        runtime.blockchain(),
        minimumFeeRawUnits
    );
    if (!runtime.blockchain().empty() &&
        block.index() == runtime.blockchain().latestBlock().index() &&
        block.hash() == runtime.blockchain().latestBlock().hash()) {
        return tip;
    }
    return replayBlock(
        runtime.config().genesisConfig(),
        tip,
        block,
        minimumFeeRawUnits,
        wallClockNow
    );
}

std::pair<core::StateTransitionPreviewContext, std::shared_ptr<ProtocolExecutionState>>
ProtocolStateTransition::contextForNextBlockWithState(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    if (!runtime.isValid()) {
        throw std::invalid_argument("Cannot build protocol transition from invalid runtime.");
    }
    if (minimumFeeRawUnits < 0) {
        throw std::invalid_argument("Minimum fee cannot be negative in protocol transition.");
    }

    const std::int64_t replayMinimumFee = checkedMinimumFee(
        runtime.config().genesisConfig().networkParameters().minimumFeeRawUnits()
    );
    const ProtocolReplayState tip = replayToTip(
        runtime.config().genesisConfig(),
        runtime.blockchain(),
        replayMinimumFee
    );
    auto tracker = std::make_shared<ProtocolExecutionState>(tip.execution);
    core::StateTransitionPreviewContext context = contextFromReplayState(
        runtime.config().genesisConfig(),
        tip,
        minimumFeeRawUnits,
        tracker,
        wallClockNow
    );
    return {std::move(context), std::move(tracker)};
}

core::StateTransitionPreviewContext ProtocolStateTransition::contextForNextBlock(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    return contextForNextBlockWithState(runtime, minimumFeeRawUnits, wallClockNow).first;
}

void ProtocolStateTransition::applyReplayDomainsToRuntime(
    NodeRuntime& runtime,
    const ProtocolReplayState& state
) {
    runtime.mutableGovernanceExecutor() = state.execution.governance;
    runtime.mutableValidatorRegistry() = state.execution.validators;
    runtime.mutableValidatorPenaltyLedger() = state.execution.penaltyLedger;
    runtime.mutableStakingRegistry() = state.execution.staking;
    runtime.invalidateAccountStateCache();
}

} // namespace nodo::node
