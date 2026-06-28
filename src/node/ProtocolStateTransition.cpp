#include "node/ProtocolStateTransition.hpp"

#include "core/State.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/FeeEconomics.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <limits>
#include <map>
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
    const consensus::ValidatorPenaltyLedger& penaltyLedger
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
    std::string networkName
) {
    try {
        const FeeEconomicBalance feeBalance =
            FeeEconomics::buildFeeEconomicBalance(blockHeight, totalFee);
        core::AccountStateView accounts = ProtocolStateTransition::settleFees(
            transactionAccounts, blockHeight, totalFee
        );

        for (const core::Transaction& transaction : transactions) {
            if (transaction.type() != core::TransactionType::GOVERNANCE_PROPOSE) {
                continue;
            }
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
        const utils::Amount supplyAfter =
            supplyBefore - feeBalance.burnAmount();

        return core::DeterministicStateTransitionResult::accepted(
            std::move(accounts),
            protocolDomains(
                governance,
                supplyAfter,
                projectedValidators,
                penaltyLedger
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

core::StateTransitionPreviewContext ProtocolStateTransition::contextForNextBlock(
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

    core::DeterministicStateDomainTransition transition =
        [blockHeight,
         supplyBefore,
         governance,
         validators,
         validatorSetHistory,
         penaltyLedger,
         networkName = genesis.networkParameters().networkName()](
            const core::AccountStateView& accounts,
            utils::Amount totalFee,
            const std::vector<core::Transaction>& transactions,
            const std::vector<core::LedgerRecord>& protocolRecords,
            std::int64_t blockTimestamp
        ) {
            return transitionFullState(
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
                networkName
            );
        };

    return core::StateTransitionPreviewContext(
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
            penaltyLedger
        ),
        std::move(transition)
    );
}

} // namespace nodo::node
