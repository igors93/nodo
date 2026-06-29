#include "node/ProtocolStateTransition.hpp"

#include "core/State.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/FeeEconomics.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/StakingRegistry.hpp"
#include "node/StakingTransactionApplier.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

core::AccountStateView credit(
    core::AccountStateView accounts,
    const std::string& address,
    utils::Amount amount
) {
    if (amount.isZero()) {
        return accounts;
    }
    const core::AccountState current = accounts.accountOrDefault(address);
    if (!accounts.putAccount(core::AccountState(
            address, current.balance() + amount, current.nonce()
        ))) {
        throw std::logic_error("Fee settlement produced an invalid account.");
    }
    return accounts;
}

core::ValidatorRegistry projectValidatorSet(
    const core::ValidatorRegistry& current,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp
) {
    core::ValidatorRegistry projected = current;
    ProtocolStateTransition::applyValidatorEpochTransition(
        projected, blockHeight, blockTimestamp
    );
    return projected;
}

std::map<std::string, std::string> protocolDomains(
    const GovernanceExecutor& governance,
    utils::Amount supply,
    const core::ValidatorRegistry& validators,
    const consensus::ValidatorPenaltyLedger& penaltyLedger,
    const StakingRegistry& stakingRegistry
) {
    std::map<std::string, std::string> domains = {
        {"governance", governance.serialize()},
        {"supply", "RuntimeSupply{latestRawUnits="
            + std::to_string(supply.rawUnits()) + "}"},
        {"validators", validators.serialize()}
    };
    if (penaltyLedger.size() > 0) {
        domains.emplace("slashing", penaltyLedger.serialize());
    }
    if (stakingRegistry.size() > 0) {
        domains.emplace("staking", stakingRegistry.serialize());
    }
    return domains;
}

} // namespace

void ProtocolStateTransition::applyValidatorEpochTransition(
    core::ValidatorRegistry& validators,
    std::uint64_t blockHeight,
    std::int64_t blockTimestamp
) {
    if (blockHeight == 0 ||
        blockHeight % NODO_VALIDATOR_EPOCH_BLOCKS != 0) {
        return;
    }

    const std::uint64_t epoch =
        ValidatorLifecycle::epochIndexForBlock(blockHeight);
    const std::vector<std::string> pending =
        validators.pendingValidatorAddresses();
    for (const std::string& address : pending) {
        const core::ValidatorRegistryEntry* entry =
            validators.entryForAddress(address);
        if (entry != nullptr &&
            entry->registrationRecord().activationEpoch() <= epoch) {
            const core::ValidatorRegistryUpdateResult result =
                validators.activateValidator(address, epoch, blockTimestamp);
            if (!result.success()) {
                throw std::logic_error("Projected validator activation failed.");
            }
        }
    }

    for (const std::string& address :
         validators.exitRequestedValidatorAddresses()) {
        const core::ValidatorRegistryEntry* entry =
            validators.entryForAddress(address);
        if (entry != nullptr &&
            entry->exitRequestHeight() + NODO_VALIDATOR_EPOCH_BLOCKS
                <= blockHeight) {
            const core::ValidatorRegistryUpdateResult result =
                validators.completeExit(address, blockTimestamp);
            if (!result.success()) {
                throw std::logic_error("Projected validator exit failed.");
            }
        }
    }
}

namespace {

// Takes stakingRegistry by reference so the caller can read the post-block
// staking state after execution completes (via a shared_ptr in the closure).
core::DeterministicStateTransitionResult transitionFullState(
    const core::AccountStateView& transactionAccounts,
    utils::Amount totalFee,
    const std::vector<core::Transaction>& transactions,
    const std::vector<core::LedgerRecord>& protocolRecords,
    std::int64_t blockTimestamp,
    std::uint64_t blockHeight,
    utils::Amount supplyBefore,
    GovernanceExecutor governance,
    core::ValidatorRegistry validators,
    core::ValidatorSetHistory validatorSetHistory,
    consensus::ValidatorPenaltyLedger penaltyLedger,
    StakingRegistry& stakingRegistry,
    std::string networkName
) {
    try {
        const FeeEconomicBalance feeBalance =
            FeeEconomics::buildFeeEconomicBalance(blockHeight, totalFee);
        core::AccountStateView accounts = ProtocolStateTransition::settleFees(
            transactionAccounts, blockHeight, totalFee
        );

        for (const core::Transaction& transaction : transactions) {
            if (transaction.type() == core::TransactionType::GOVERNANCE_PROPOSE) {
                const GovernanceExecutionResult result = governance.executeProposal(
                    transaction.id(),
                    transaction.toAddress(),
                    blockHeight,
                    blockTimestamp
                );
                if (!result.isApplied() && !result.isPending()) {
                    return core::DeterministicStateTransitionResult::rejected(
                        "Governance proposal failed deterministic execution: "
                            + result.detail()
                    );
                }
                continue;
            }

            if (core::isStakingTransaction(transaction.type()) ||
                transaction.type() == core::TransactionType::VALIDATOR_EXIT_REQUEST ||
                transaction.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST) {

                const std::string& validatorAddress = transaction.toAddress();
                const economics::StakeAccount stake =
                    stakingRegistry.accountOrDefault(validatorAddress);
                const core::AccountState senderState =
                    accounts.accountOrDefault(transaction.fromAddress());

                if (transaction.type() == core::TransactionType::STAKE_DEPOSIT ||
                    transaction.type() == core::TransactionType::STAKE_TOP_UP) {
                    // previewBlock already deducted (amount + fee) from sender.
                    // Only update the StakeAccount bonded amount here.
                    if (transaction.amount().rawUnits() <= 0) {
                        return core::DeterministicStateTransitionResult::rejected(
                            "Staking deposit amount must be positive at height "
                                + std::to_string(blockHeight) + "."
                        );
                    }
                    economics::StakeAccount next(
                        stake.validatorAddress(),
                        utils::Amount::fromRawUnits(
                            stake.bondedAmount().rawUnits() +
                            transaction.amount().rawUnits()
                        )
                    );
                    if (stake.jailed())     next.jail();
                    if (stake.tombstoned()) next.tombstone();
                    stakingRegistry.setAccount(validatorAddress, std::move(next));

                } else if (transaction.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST) {
                    economics::StakeAccount next = stake;
                    next.unjail();
                    stakingRegistry.setAccount(validatorAddress, std::move(next));

                } else {
                    // STAKE_WITHDRAW and VALIDATOR_EXIT_REQUEST: use applier for
                    // stake-domain validation (cooldown, stake sufficiency, jailed check).
                    const StakingApplyResult result = StakingTransactionApplier::apply(
                        transaction,
                        stake,
                        senderState.balance(),
                        blockHeight
                    );
                    if (!result.applied()) {
                        return core::DeterministicStateTransitionResult::rejected(
                            "Staking operation failed at height "
                                + std::to_string(blockHeight) + ": " + result.reason()
                        );
                    }
                    stakingRegistry.setAccount(validatorAddress, result.updatedStake());

                    if (transaction.type() == core::TransactionType::STAKE_WITHDRAW) {
                        // Credit sender with the withdrawn amount (fee already
                        // deducted by previewBlock; delta from applier excludes fee).
                        const core::AccountState updated(
                            senderState.address(),
                            senderState.balance() + transaction.amount(),
                            senderState.nonce()
                        );
                        if (!accounts.putAccount(updated)) {
                            return core::DeterministicStateTransitionResult::rejected(
                                "Stake withdrawal credit failed at height "
                                    + std::to_string(blockHeight) + "."
                            );
                        }
                    }
                }
            }
        }

        governance.advanceToHeight(blockHeight + 1, blockTimestamp);

        const crypto::ProtocolCryptoContext cryptoContext =
            crypto::ProtocolCryptoContext::fromNetworkName(networkName);
        if (!cryptoContext.isValid()) {
            throw std::logic_error(
                "Protocol crypto context is invalid during slashing transition."
            );
        }
        CanonicalSlashingTransition::applyEvidenceRecords(
            protocolRecords,
            blockHeight,
            blockTimestamp,
            validatorSetHistory,
            cryptoContext.policy(),
            cryptoContext.signatureProvider(),
            penaltyLedger,
            validators
        );

        const core::ValidatorRegistry projectedValidators =
            projectValidatorSet(validators, blockHeight, blockTimestamp);

        if (feeBalance.burnAmount() > supplyBefore) {
            return core::DeterministicStateTransitionResult::rejected(
                "Supply underflow: fee burn amount exceeds circulating supply."
            );
        }
        const utils::Amount supplyAfter =
            supplyBefore - feeBalance.burnAmount();

        return core::DeterministicStateTransitionResult::accepted(
            std::move(accounts),
            protocolDomains(
                governance,
                supplyAfter,
                projectedValidators,
                penaltyLedger,
                stakingRegistry
            )
        );
    } catch (const std::exception& error) {
        return core::DeterministicStateTransitionResult::rejected(
            std::string("Deterministic protocol transition failed: ")
                + error.what()
        );
    }
}

} // namespace

core::AccountStateView ProtocolStateTransition::settleFees(
    const core::AccountStateView& accounts,
    std::uint64_t blockHeight,
    utils::Amount totalFee
) {
    const FeeEconomicBalance balance =
        FeeEconomics::buildFeeEconomicBalance(blockHeight, totalFee);
    core::AccountStateView settled = credit(
        accounts,
        core::State::feePoolAddress(),
        balance.validatorRewardAmount()
    );
    return credit(
        std::move(settled),
        ProtectionTreasury::TREASURY_ADDRESS,
        balance.treasuryAmount()
    );
}

core::DeterministicStateDomainTransition
ProtocolStateTransition::accountSettlementForReplay(
    std::uint64_t blockHeight
) {
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
            return core::DeterministicStateTransitionResult::rejected(
                error.what()
            );
        }
    };
}

std::pair<core::StateTransitionPreviewContext, std::shared_ptr<StakingRegistry>>
ProtocolStateTransition::contextForNextBlockWithRegistry(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    if (!runtime.isValid()) {
        throw std::invalid_argument("Cannot build protocol transition from invalid runtime.");
    }
    const std::uint64_t blockHeight = runtime.blockchain().size();
    const utils::Amount supplyBefore = runtime.supplyState().latestSupply();
    const GovernanceExecutor governance = runtime.governanceExecutor();
    const core::ValidatorRegistry validators = runtime.validatorRegistry();
    const core::ValidatorSetHistory validatorSetHistory =
        runtime.validatorSetHistory();
    const consensus::ValidatorPenaltyLedger penaltyLedger =
        runtime.validatorPenaltyLedger();
    const config::GenesisConfig& genesis = runtime.config().genesisConfig();
    const std::uint64_t genesisMinimumFeeRaw =
        genesis.networkParameters().minimumFeeRawUnits();
    if (genesisMinimumFeeRaw > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error(
            "Genesis minimum fee exceeds supported Amount range."
        );
    }

    // Snapshot of the staking state BEFORE this block.
    //
    // initialRegistry is used for two purposes:
    //   1. The initial protocolDomains passed to the context constructor so that
    //      previewContextAtTip / latestStateRootForRuntime always produce the
    //      correct tip state root for the CURRENT runtime (post last-committed block).
    //   2. The starting point for each closure invocation — making the transition
    //      idempotent: multiple calls with the same context (e.g. the production
    //      pass and the certification pass in produceAndFinalizeNextBlock) always
    //      start from the same baseline and produce the same state root.
    //
    // sharedRegistry is overwritten by each invocation so applyCertifiedBlock can
    // read the post-block staking state without having to re-execute the block.
    StakingRegistry initialRegistry = runtime.stakingRegistry();
    auto sharedRegistry = std::make_shared<StakingRegistry>();

    core::DeterministicStateDomainTransition transition =
        [blockHeight,
         supplyBefore,
         governance,
         validators,
         validatorSetHistory,
         penaltyLedger,
         sharedRegistry,
         initialRegistry,
         networkName = genesis.networkParameters().networkName()](
            const core::AccountStateView& accounts,
            utils::Amount totalFee,
            const std::vector<core::Transaction>& transactions,
            const std::vector<core::LedgerRecord>& protocolRecords,
            std::int64_t blockTimestamp
        ) {
            StakingRegistry localRegistry = initialRegistry;
            auto result = transitionFullState(
                accounts,
                totalFee,
                transactions,
                protocolRecords,
                blockTimestamp,
                blockHeight,
                supplyBefore,
                governance,
                validators,
                validatorSetHistory,
                penaltyLedger,
                localRegistry,
                networkName
            );
            *sharedRegistry = localRegistry;
            return result;
        };

    // Separate registry for the pre-validator: tracks intermediate staking state
    // across consecutive transactions within the same block during previewBlock,
    // so deposit-then-withdraw in the same block is correctly handled.
    auto prevalidatorRegistry =
        std::make_shared<StakingRegistry>(runtime.stakingRegistry());

    core::StateTransitionPreviewContext context(
        minimumFeeRawUnits,
        RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesis,
            runtime.blockchain(),
            static_cast<std::int64_t>(genesisMinimumFeeRaw)
        ),
        false,
        true,
        "",
        wallClockNow,
        genesis.networkParameters().chainId(),
        genesis.networkParameters().networkName(),
        protocolDomains(
            governance,
            supplyBefore,
            validators,
            penaltyLedger,
            initialRegistry
        ),
        std::move(transition)
    );

    context.setDomainTransactionPreValidator(
        [prevalidatorRegistry, blockHeight](const core::Transaction& tx) -> bool {
            const std::string& validatorAddr = tx.toAddress();
            economics::StakeAccount stake =
                prevalidatorRegistry->accountOrDefault(validatorAddr);

            if (tx.type() == core::TransactionType::STAKE_DEPOSIT ||
                tx.type() == core::TransactionType::STAKE_TOP_UP) {
                // Always valid at domain level; track for subsequent transactions.
                economics::StakeAccount next(
                    stake.validatorAddress(),
                    utils::Amount::fromRawUnits(
                        stake.bondedAmount().rawUnits() + tx.amount().rawUnits()
                    )
                );
                if (stake.jailed())     next.jail();
                if (stake.tombstoned()) next.tombstone();
                prevalidatorRegistry->setAccount(validatorAddr, std::move(next));
                return true;
            }

            if (tx.type() == core::TransactionType::STAKE_WITHDRAW) {
                if (stake.jailed() || stake.tombstoned()) return false;
                const std::int64_t bondedRaw  = stake.bondedAmount().rawUnits();
                const std::int64_t slashedRaw = stake.slashedAmount().rawUnits();
                const std::int64_t availableRaw =
                    (bondedRaw > slashedRaw) ? (bondedRaw - slashedRaw) : 0;
                if (tx.amount().rawUnits() > availableRaw) return false;
                if (blockHeight < tx.nonce() +
                        StakingTransactionApplier::UNBONDING_DELAY_BLOCKS) {
                    return false;
                }
                economics::StakeAccount next(
                    stake.validatorAddress(),
                    utils::Amount::fromRawUnits(bondedRaw - tx.amount().rawUnits())
                );
                if (stake.jailed())     next.jail();
                if (stake.tombstoned()) next.tombstone();
                prevalidatorRegistry->setAccount(validatorAddr, std::move(next));
                return true;
            }

            if (tx.type() == core::TransactionType::VALIDATOR_EXIT_REQUEST) {
                if (stake.jailed() || stake.tombstoned()) return false;
                const std::int64_t bondedRaw  = stake.bondedAmount().rawUnits();
                const std::int64_t slashedRaw = stake.slashedAmount().rawUnits();
                if (bondedRaw == 0 || bondedRaw <= slashedRaw) return false;
                // Exit request does not mutate the stake account.
                return true;
            }

            if (tx.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST) {
                if (!stake.jailed() || stake.tombstoned()) return false;
                economics::StakeAccount next = stake;
                next.unjail();
                prevalidatorRegistry->setAccount(validatorAddr, std::move(next));
                return true;
            }

            return true;
        }
    );

    return {std::move(context), sharedRegistry};
}

core::StateTransitionPreviewContext ProtocolStateTransition::contextForNextBlock(
    const NodeRuntime& runtime,
    std::int64_t minimumFeeRawUnits,
    std::int64_t wallClockNow
) {
    return contextForNextBlockWithRegistry(runtime, minimumFeeRawUnits, wallClockNow).first;
}

} // namespace nodo::node
