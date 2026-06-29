#include "node/ProtocolStateTransition.hpp"

#include "core/State.hpp"
#include "core/TransactionExecutionRouter.hpp"
#include "node/FeeEconomics.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/ValidatorLifecycle.hpp"

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

} // namespace

void ProtocolStateTransition::applyValidatorEpochTransition(
    core::ValidatorRegistry& validators,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp
) {
    if (blockHeight == 0 || blockHeight % NODO_VALIDATOR_EPOCH_BLOCKS != 0) return;
    const std::uint64_t epoch = ValidatorLifecycle::epochIndexForBlock(blockHeight);
    for (const std::string& address : validators.pendingValidatorAddresses()) {
        const auto* entry = validators.entryForAddress(address);
        if (entry != nullptr && entry->registrationRecord().activationEpoch() <= epoch) {
            const auto result = validators.activateValidator(address, epoch, blockTimestamp);
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

std::pair<core::StateTransitionPreviewContext, std::shared_ptr<ProtocolExecutionState>>
ProtocolStateTransition::contextForNextBlockWithState(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    if (!runtime.isValid()) {
        throw std::invalid_argument("Cannot build protocol transition from invalid runtime.");
    }
    const std::uint64_t blockHeight = runtime.blockchain().size();
    const config::GenesisConfig& genesis = runtime.config().genesisConfig();
    const std::uint64_t genesisMinimumFeeRaw =
        genesis.networkParameters().minimumFeeRawUnits();
    if (genesisMinimumFeeRaw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Genesis minimum fee exceeds supported Amount range.");
    }

    ProtocolExecutionState initial{
        runtime.governanceExecutor(),
        runtime.validatorRegistry(),
        runtime.validatorPenaltyLedger(),
        runtime.stakingRegistry(),
        runtime.supplyState().latestSupply(),
        finalizedBurns(runtime.supplyState())
    };
    auto tracker = std::make_shared<ProtocolExecutionState>(initial);
    const auto domains = protocolExecutionDomains(initial);
    core::StateTransitionPreviewContext context(
        minimumFeeRawUnits,
        RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesis, runtime.blockchain(),
            static_cast<std::int64_t>(genesisMinimumFeeRaw)
        ),
        false,
        true,
        "",
        wallClockNow,
        genesis.networkParameters().chainId(),
        genesis.networkParameters().networkName(),
        domains,
        {},
        makeProtocolDomainExecutorFactory(
            initial, runtime.validatorSetHistory(),
            genesis.networkParameters().networkName(), tracker
        ),
        true
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

ProtocolExecutionState ProtocolStateTransition::replayFinalizedBlockDomains(
    const NodeRuntime& runtime,
    const core::Block& block
) {
    ProtocolExecutionState initial{
        runtime.governanceExecutor(), runtime.validatorRegistry(),
        runtime.validatorPenaltyLedger(), runtime.stakingRegistry(),
        runtime.supplyState().latestSupply(),
        finalizedBurns(runtime.supplyState())
    };
    auto tracker = std::make_shared<ProtocolExecutionState>(initial);
    auto factory = makeProtocolDomainExecutorFactory(
        initial, runtime.validatorSetHistory(),
        runtime.config().genesisConfig().networkParameters().networkName(), tracker
    );
    std::unique_ptr<core::TransactionDomainExecutor> executor = factory();
    core::AccountStateView accounts;
    utils::Amount totalFee;
    std::vector<core::LedgerRecord> protocolRecords;
    for (const auto& record : block.records()) {
        if (record.type() != core::LedgerRecordType::TRANSACTION) {
            protocolRecords.push_back(record);
            continue;
        }
        const core::Transaction tx = core::Transaction::deserialize(record.payload());
        const core::TransactionExecutionResult result = core::TransactionExecutionRouter::execute(
            tx,
            core::TransactionExecutionContext(
                accounts, block.index(), block.timestamp(), false, true, true,
                "", nullptr, executor.get()
            )
        );
        if (!result.success()) {
            throw std::logic_error("Protocol domain replay failed: " + result.reason());
        }
        accounts = result.accounts();
        totalFee = totalFee + tx.fee();
    }
    const auto finalized = executor->finalizeBlock(
        accounts, totalFee, protocolRecords, block.index(), block.timestamp()
    );
    if (!finalized.applied()) {
        throw std::logic_error("Protocol domain replay finalization failed: " + finalized.reason());
    }
    return *tracker;
}

} // namespace nodo::node
