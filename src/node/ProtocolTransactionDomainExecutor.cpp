#include "node/ProtocolTransactionDomainExecutor.hpp"

#include "core/State.hpp"
#include "core/TransactionPayload.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "economics/BurnRecord.hpp"
#include "node/CanonicalSlashingTransition.hpp"
#include "node/FeeEconomics.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <limits>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

class ProtocolTransactionDomainExecutor final : public core::TransactionDomainExecutor {
public:
    ProtocolTransactionDomainExecutor(
        ProtocolExecutionState state,
        core::ValidatorSetHistory validatorSetHistory,
        std::string networkName,
        std::shared_ptr<ProtocolExecutionState> tracker
    ) : m_state(std::move(state)), m_validatorSetHistory(std::move(validatorSetHistory)),
        m_networkName(std::move(networkName)), m_tracker(std::move(tracker)),
        m_burnRecords(m_state.burns) {
        refreshDomains();
    }

    core::TransactionDomainExecutionResult applyBurn(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t
    ) override {
        return atomically(accounts, [&] {
            if (tx.amount() > m_state.supply) throw std::invalid_argument("Voluntary burn exceeds current supply.");
            m_state.supply = m_state.supply - tx.amount();
            m_burnRecords.emplace_back(
                tx.id(), height, 0,
                tx.fromAddress(), tx.amount(), "voluntary burn",
                economics::BurnType::VOLUNTARY_BURN
            );
        });
    }

    core::TransactionDomainExecutionResult applyStakeDeposit(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override { return stakeDeposit(tx, accounts, height, now, false); }

    core::TransactionDomainExecutionResult applyStakeTopUp(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override { return stakeDeposit(tx, accounts, height, now, true); }

    core::TransactionDomainExecutionResult applyStakeWithdraw(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            const auto* entry = m_state.validators.entryForAddress(tx.toAddress());
            if (entry != nullptr && entry->eligibleForConsensus()) {
                const auto stake = m_state.staking.accountOrDefault(tx.toAddress());
                const std::int64_t available =
                    stake.bondedAmount().rawUnits() - stake.slashedAmount().rawUnits();
                if (available < tx.amount().rawUnits()) {
                    throw std::invalid_argument("Available aggregate stake is insufficient.");
                }
                const std::int64_t remaining =
                    available - tx.amount().rawUnits();
                if (remaining < static_cast<std::int64_t>(
                        core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS)) {
                    throw std::invalid_argument(
                        "Active validator stake cannot be withdrawn below the protocol minimum."
                    );
                }
            }
            m_state.staking.withdraw(tx.fromAddress(), tx.toAddress(), tx.amount(), height);
            updateValidatorStake(tx.toAddress(), now);
        });
    }

    core::TransactionDomainExecutionResult applyValidatorRegister(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            if (tx.amount().rawUnits() < static_cast<std::int64_t>(
                    core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS)) {
                throw std::invalid_argument("Validator registration stake is below protocol minimum.");
            }
            const core::ValidatorRegistrationPayload payload =
                core::ValidatorRegistrationPayload::deserialize(tx.data());
            const std::string validatorAddress =
                crypto::AddressDerivation::deriveFromPublicKey(payload.validatorPublicKey()).value();
            if (tx.toAddress() != validatorAddress) {
                throw std::invalid_argument("Validator target does not match payload public key.");
            }
            m_state.staking.deposit(
                tx.fromAddress(), validatorAddress, tx.amount(), height, false
            );
            const core::ValidatorRegistrationRecord record(
                validatorAddress,
                payload.validatorPublicKey(),
                ValidatorLifecycle::epochIndexForBlock(height) + 1,
                payload.metadataHash(),
                now
            );
            const auto registered = m_state.validators.registerPendingValidator(
                record, static_cast<std::uint64_t>(tx.amount().rawUnits()), tx.fromAddress()
            );
            if (!registered.accepted()) throw std::invalid_argument(registered.reason());
        });
    }

    core::TransactionDomainExecutionResult applyValidatorExitRequest(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            const auto* entry = m_state.validators.entryForAddress(tx.toAddress());
            if (entry == nullptr || entry->ownerAddress() != tx.fromAddress()) {
                throw std::invalid_argument("Validator exit requester is not the registered owner.");
            }
            const auto result = m_state.validators.requestExit(tx.toAddress(), height, now);
            if (!result.success()) throw std::invalid_argument(result.reason());
        });
    }

    core::TransactionDomainExecutionResult applyValidatorUnjailRequest(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            const auto* entry = m_state.validators.entryForAddress(tx.toAddress());
            if (entry == nullptr || entry->ownerAddress() != tx.fromAddress() || !entry->jailed()) {
                throw std::invalid_argument("Validator unjail requester is unauthorized or validator is not jailed.");
            }
            const auto result = m_state.validators.unjailValidator(
                tx.toAddress(), ValidatorLifecycle::epochIndexForBlock(height), now
            );
            if (!result.success()) throw std::invalid_argument(result.reason());
            m_state.staking.unjail(tx.toAddress());
        });
    }

    core::TransactionDomainExecutionResult applyGovernanceProposal(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            std::string reason;
            if (!m_state.governance.submitProposal(
                    tx.id(), tx.fromAddress(), tx.data(), height, now,
                    totalEligibleVotingWeight(), reason)) {
                throw std::invalid_argument(reason);
            }
        });
    }

    core::TransactionDomainExecutionResult applyGovernanceVote(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now
    ) override {
        return atomically(accounts, [&] {
            const core::GovernanceVotePayload vote =
                core::GovernanceVotePayload::deserialize(tx.data());
            if (vote.proposalId() != tx.toAddress()) {
                throw std::invalid_argument("Governance vote target does not match payload proposal id.");
            }
            const auto* voter = m_state.validators.entryForAddress(vote.validatorAddress());
            const bool authorizedOwner = voter != nullptr &&
                (voter->ownerAddress() == tx.fromAddress() ||
                 (voter->ownerAddress().empty() &&
                  vote.validatorAddress() == tx.fromAddress()));
            if (voter == nullptr || !voter->eligibleForConsensus() ||
                !authorizedOwner) {
                throw std::invalid_argument("Governance voter is not an authorized active validator.");
            }
            const std::uint64_t weight = votingWeight(vote.validatorAddress());
            std::string reason;
            if (!m_state.governance.castVote(
                    tx.toAddress(), vote.validatorAddress(),
                    vote.choice(), weight, height, now, tx.id(), reason)) {
                throw std::invalid_argument(reason);
            }
        });
    }

    core::TransactionDomainExecutionResult finalizeBlock(
        const core::AccountStateView& accounts,
        utils::Amount totalFee,
        const std::vector<core::LedgerRecord>& protocolRecords,
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp
    ) override {
        core::AccountStateView settledAccounts =
            ProtocolStateTransition::settleFees(accounts, blockHeight, totalFee);
        return atomically(accounts, [&] {
            const FeeEconomicBalance fees = FeeEconomics::buildFeeEconomicBalance(blockHeight, totalFee);
            if (fees.burnAmount() > m_state.supply) throw std::invalid_argument("Fee burn exceeds current supply.");
            m_state.supply = m_state.supply - fees.burnAmount();
            if (fees.burnAmount().isPositive()) {
                m_burnRecords.emplace_back(
                    "fee-burn-block-" + std::to_string(blockHeight), blockHeight, 0,
                    "nodo_fee_pool", fees.burnAmount(), "fee burn",
                    economics::BurnType::FEE_BURN
                );
            }
            m_state.governance.advanceToHeight(
                blockHeight + 1, blockTimestamp, &settledAccounts);
            const crypto::ProtocolCryptoContext cryptoContext =
                crypto::ProtocolCryptoContext::fromNetworkName(m_networkName);
            if (!cryptoContext.isValid()) throw std::invalid_argument("Invalid protocol crypto context.");
            CanonicalSlashingTransition::applyEvidenceRecords(
                protocolRecords, blockHeight, blockTimestamp, m_validatorSetHistory,
                cryptoContext.policy(), cryptoContext.signatureProvider(),
                m_state.penaltyLedger, m_state.validators
            );
            synchronizePenaltyState(blockTimestamp);
            ProtocolStateTransition::applyValidatorEpochTransition(
                m_state.validators, blockHeight, blockTimestamp
            );
        }, settledAccounts);
    }

    const std::map<std::string, std::string>& domains() const override { return m_domains; }

private:
    ProtocolExecutionState m_state;
    core::ValidatorSetHistory m_validatorSetHistory;
    std::string m_networkName;
    std::shared_ptr<ProtocolExecutionState> m_tracker;
    std::vector<economics::BurnRecord> m_burnRecords;
    std::map<std::string, std::string> m_domains;

    void refreshDomains() {
        m_state.burns = m_burnRecords;
        m_domains = protocolExecutionDomains(m_state);
        if (m_tracker) *m_tracker = m_state;
    }

    template <typename Mutation>
    core::TransactionDomainExecutionResult atomically(
        const core::AccountStateView& accounts,
        Mutation mutation
    ) {
        return atomically(accounts, std::move(mutation), accounts);
    }

    template <typename Mutation>
    core::TransactionDomainExecutionResult atomically(
        const core::AccountStateView&,
        Mutation mutation,
        const core::AccountStateView& resultingAccounts
    ) {
        const ProtocolExecutionState before = m_state;
        const auto burnsBefore = m_burnRecords;
        try {
            mutation();
            refreshDomains();
            return core::TransactionDomainExecutionResult::accepted(resultingAccounts, m_domains);
        } catch (const std::exception& error) {
            m_state = before;
            m_burnRecords = burnsBefore;
            refreshDomains();
            return core::TransactionDomainExecutionResult::rejected("DOMAIN_RULE_VIOLATION", error.what());
        }
    }

    core::TransactionDomainExecutionResult stakeDeposit(
        const core::Transaction& tx, const core::AccountStateView& accounts,
        std::uint64_t height, std::int64_t now, bool topUp
    ) {
        return atomically(accounts, [&] {
            if (m_state.staking.accountOrDefault(tx.toAddress()).tombstoned()) {
                throw std::invalid_argument("Cannot add stake to a tombstoned validator.");
            }
            m_state.staking.deposit(tx.fromAddress(), tx.toAddress(), tx.amount(), height, topUp);
            updateValidatorStake(tx.toAddress(), now);
        });
    }

    void updateValidatorStake(const std::string& validatorAddress, std::int64_t now) {
        const auto* entry = m_state.validators.entryForAddress(validatorAddress);
        if (entry == nullptr) return;
        const auto stake =
            m_state.staking.accountOrDefault(validatorAddress);
        const std::int64_t raw =
            stake.bondedAmount().rawUnits() - stake.slashedAmount().rawUnits();
        if (raw < 0) throw std::logic_error("Negative validator stake.");
        const auto result = m_state.validators.updateStake(
            validatorAddress, static_cast<std::uint64_t>(raw), now
        );
        if (!result.success()) throw std::invalid_argument(result.reason());
    }

    std::uint64_t votingWeight(const std::string& validatorAddress) const {
        const auto stake = m_state.staking.accountOrDefault(validatorAddress);
        const std::int64_t available = stake.bondedAmount().rawUnits() - stake.slashedAmount().rawUnits();
        if (available > 0) {
            return core::ValidatorRegistry::consensusWeightFromStake(
                static_cast<std::uint64_t>(available)
            );
        }
        const auto* entry = m_state.validators.entryForAddress(validatorAddress);
        if (entry == nullptr || !entry->eligibleForConsensus()) return 0;
        return entry->consensusWeight();
    }

    std::uint64_t totalEligibleVotingWeight() const {
        std::uint64_t total = 0;
        for (const auto& address : m_state.validators.eligibleValidatorAddresses()) {
            const std::uint64_t candidate = votingWeight(address);
            if (std::numeric_limits<std::uint64_t>::max() - total < candidate) {
                throw std::overflow_error("Governance voting weight overflow.");
            }
            total += candidate;
        }
        return total;
    }

    void synchronizePenaltyState(std::int64_t now) {
        for (const auto& [address, current] : m_state.staking.accounts()) {
            const std::int64_t totalSlash =
                m_state.penaltyLedger.totalSlashAmountForValidator(address);
            const std::int64_t boundedSlash = std::min(
                std::max<std::int64_t>(totalSlash, 0),
                current.bondedAmount().rawUnits()
            );
            const bool tombstoned = current.tombstoned() ||
                m_state.penaltyLedger.validatorIsTombstoned(address);
            const bool jailed = tombstoned || current.jailed() ||
                m_state.penaltyLedger.validatorIsJailed(address);
            m_state.staking.setAccount(address, economics::StakeAccount(
                address, current.bondedAmount(),
                utils::Amount::fromRawUnits(boundedSlash), jailed, tombstoned
            ));
            updateValidatorStake(address, now);
        }
    }
};

} // namespace

std::map<std::string, std::string> protocolExecutionDomains(
    const ProtocolExecutionState& state
) {
    std::vector<economics::BurnRecord> burns = state.burns;
    std::sort(burns.begin(), burns.end(), [](const auto& left, const auto& right) {
        return left.burnId() < right.burnId();
    });
    std::ostringstream burnLedger;
    burnLedger << "BurnLedger{count=" << burns.size() << ";records=[";
    for (std::size_t i = 0; i < burns.size(); ++i) {
        if (i != 0) burnLedger << ',';
        burnLedger << burns[i].serialize();
    }
    burnLedger << "]}";
    return {
        {"burns", burnLedger.str()},
        {"governance", state.governance.serialize()},
        {"slashing", state.penaltyLedger.serialize()},
        {"staking", state.staking.serialize()},
        {"supply", "RuntimeSupply{latestRawUnits=" + std::to_string(state.supply.rawUnits()) + "}"},
        {"validators", state.validators.serialize()}
    };
}

core::TransactionDomainExecutorFactory makeProtocolDomainExecutorFactory(
    ProtocolExecutionState initialState,
    core::ValidatorSetHistory validatorSetHistory,
    std::string networkName,
    std::shared_ptr<ProtocolExecutionState> resultTracker
) {
    return [state = std::move(initialState), history = std::move(validatorSetHistory),
            network = std::move(networkName), tracker = std::move(resultTracker)]() mutable {
        return std::make_unique<ProtocolTransactionDomainExecutor>(
            state, history, network, tracker
        );
    };
}

} // namespace nodo::node
