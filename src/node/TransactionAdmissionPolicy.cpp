#include "node/TransactionAdmissionPolicy.hpp"

#include "core/TransactionPayload.hpp"
#include "core/TransactionTypePolicy.hpp"
#include "crypto/AddressDerivation.hpp"
#include "node/ValidatorLifecycle.hpp"

namespace nodo::node {

namespace {

bool governanceVoterAuthorized(
    const core::ValidatorRegistryEntry& entry,
    const std::string& validatorAddress,
    const std::string& senderAddress
) {
    return entry.ownerAddress() == senderAddress ||
           (entry.ownerAddress().empty() && validatorAddress == senderAddress);
}

std::uint64_t governanceVotingWeight(
    const core::ValidatorRegistryEntry& entry
) {
    return entry.eligibleForConsensus() ? entry.consensusWeight() : 0;
}

} // namespace

TransactionAdmissionContext::TransactionAdmissionContext(
    const core::AccountStateView& accounts,
    const mempool::Mempool& mempool,
    const StakingRegistry& staking,
    const core::ValidatorRegistry& validators,
    const GovernanceExecutor& governance,
    std::uint64_t nextBlockHeight
) : m_accounts(&accounts), m_mempool(&mempool), m_staking(&staking),
    m_validators(&validators), m_governance(&governance),
    m_nextBlockHeight(nextBlockHeight) {}

const core::AccountStateView& TransactionAdmissionContext::accounts() const { return *m_accounts; }
const mempool::Mempool& TransactionAdmissionContext::mempool() const { return *m_mempool; }
const StakingRegistry& TransactionAdmissionContext::staking() const { return *m_staking; }
const core::ValidatorRegistry& TransactionAdmissionContext::validators() const { return *m_validators; }
const GovernanceExecutor& TransactionAdmissionContext::governance() const { return *m_governance; }
std::uint64_t TransactionAdmissionContext::nextBlockHeight() const { return m_nextBlockHeight; }

bool TransactionAdmissionPolicy::validateTypeAndPayload(
    const core::Transaction& transaction,
    std::string& reason
) {
    if (!core::TransactionTypePolicyRegistry::isMempoolType(transaction.type()) ||
        !core::TransactionTypePolicyRegistry::validateShape(transaction, reason)) {
        if (reason.empty()) reason = "Transaction type is not accepted by the user mempool.";
        return false;
    }
    try {
        if (transaction.type() == core::TransactionType::VALIDATOR_REGISTER) {
            (void)core::ValidatorRegistrationPayload::deserialize(transaction.data());
        } else if (transaction.type() == core::TransactionType::GOVERNANCE_VOTE) {
            (void)core::GovernanceVotePayload::deserialize(transaction.data());
        } else if (transaction.type() == core::TransactionType::GOVERNANCE_PROPOSE) {
            std::string detail;
            if (!GovernanceExecutor::validateProposalPayload(transaction.data(), detail)) {
                reason = detail;
                return false;
            }
        }
    } catch (const std::exception& error) {
        reason = error.what();
        return false;
    }
    reason.clear();
    return true;
}

bool TransactionAdmissionPolicy::validateDomain(
    const core::Transaction& transaction,
    const TransactionAdmissionContext& context,
    std::string& reason
) {
    try {
        const std::vector<core::Transaction> pending =
            context.mempool().transactionsForBlock(context.mempool().size());
        switch (transaction.type()) {
            case core::TransactionType::STAKE_DEPOSIT:
                if (context.staking().accountOrDefault(transaction.toAddress()).tombstoned()) {
                    reason = "Cannot add stake to a tombstoned validator.";
                    return false;
                }
                break;
            case core::TransactionType::STAKE_TOP_UP:
                if (!context.staking().ownedStake(
                        transaction.fromAddress(), transaction.toAddress()).isPositive() ||
                    context.staking().accountOrDefault(transaction.toAddress()).tombstoned()) {
                    reason = "Stake top-up requires an existing owner position.";
                    return false;
                }
                break;
            case core::TransactionType::STAKE_UNLOCK: {
                const auto account = context.staking().accountOrDefault(transaction.toAddress());
                if (account.jailed() || account.tombstoned()) {
                    reason = "Jailed or tombstoned validator stake cannot be unlocked.";
                    return false;
                }
                const utils::Amount active = context.staking().activeStake(
                    transaction.fromAddress(), transaction.toAddress());
                utils::Amount reserved;
                for (const auto& queued : pending) {
                    if (queued.type() == core::TransactionType::STAKE_UNLOCK &&
                        queued.fromAddress() == transaction.fromAddress() &&
                        queued.toAddress() == transaction.toAddress() &&
                        queued.nonce() != transaction.nonce()) {
                        reserved = reserved + queued.amount();
                    }
                }
                if (active < reserved + transaction.amount()) {
                    reason = "Stake unlock exceeds active ownership.";
                    return false;
                }
                const auto* entry =
                    context.validators().entryForAddress(transaction.toAddress());
                if (entry != nullptr && entry->eligibleForConsensus()) {
                    const std::int64_t available =
                        context.staking().activeStakeFor(transaction.toAddress()).rawUnits();
                    const std::int64_t requested =
                        (reserved + transaction.amount()).rawUnits();
                    if (available < requested ||
                        available - requested < static_cast<std::int64_t>(
                            core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS)) {
                        reason = "Active validator stake cannot be unlocked below the protocol minimum.";
                        return false;
                    }
                }
                break;
            }
            case core::TransactionType::STAKE_WITHDRAW: {
                const utils::Amount withdrawable = context.staking().withdrawableStake(
                    transaction.fromAddress(), transaction.toAddress(), context.nextBlockHeight());
                utils::Amount reserved;
                for (const auto& queued : pending) {
                    if (queued.type() == core::TransactionType::STAKE_WITHDRAW &&
                        queued.fromAddress() == transaction.fromAddress() &&
                        queued.toAddress() == transaction.toAddress() &&
                        queued.nonce() != transaction.nonce()) {
                        reserved = reserved + queued.amount();
                    }
                }
                if (withdrawable < reserved + transaction.amount()) {
                    reason = "Stake withdrawal exceeds withdrawable unbonded stake or is still cooling down.";
                    return false;
                }
                const auto account = context.staking().accountOrDefault(transaction.toAddress());
                if (account.jailed() || account.tombstoned()) {
                    reason = "Jailed or tombstoned validator stake cannot be withdrawn.";
                    return false;
                }
                break;
            }
            case core::TransactionType::VALIDATOR_REGISTER: {
                const auto payload = core::ValidatorRegistrationPayload::deserialize(transaction.data());
                const std::string address = crypto::AddressDerivation::deriveFromPublicKey(
                    payload.validatorPublicKey()).value();
                if (transaction.toAddress() != address || context.validators().hasValidator(address) ||
                    transaction.amount().rawUnits() < static_cast<std::int64_t>(
                        core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS)) {
                    reason = "Validator registration target, uniqueness, or minimum stake is invalid.";
                    return false;
                }
                for (const auto& queued : pending) {
                    if (queued.type() == core::TransactionType::VALIDATOR_REGISTER &&
                        queued.toAddress() == address && queued.nonce() != transaction.nonce()) {
                        reason = "Validator already has a pending registration transaction.";
                        return false;
                    }
                }
                break;
            }
            case core::TransactionType::VALIDATOR_EXIT_REQUEST:
            case core::TransactionType::VALIDATOR_UNJAIL_REQUEST: {
                const auto* entry = context.validators().entryForAddress(transaction.toAddress());
                if (entry == nullptr || entry->ownerAddress() != transaction.fromAddress()) {
                    reason = "Validator lifecycle request is not authorized by its owner.";
                    return false;
                }
                if (transaction.type() == core::TransactionType::VALIDATOR_EXIT_REQUEST &&
                    entry->status() != core::ValidatorRegistrationStatus::ACTIVE &&
                    entry->status() != core::ValidatorRegistrationStatus::JAILED) {
                    reason = "Validator is not in a state that permits exit.";
                    return false;
                }
                if (transaction.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST && !entry->jailed()) {
                    reason = "Validator is not jailed.";
                    return false;
                }
                if (transaction.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST &&
                    (context.nextBlockHeight() == 0 ||
                     ValidatorLifecycle::epochIndexForBlock(context.nextBlockHeight()) <
                        entry->jailUntilEpoch())) {
                    reason = "Validator unjail cooldown has not elapsed.";
                    return false;
                }
                if (transaction.type() == core::TransactionType::VALIDATOR_UNJAIL_REQUEST &&
                    context.staking().accountOrDefault(transaction.toAddress()).tombstoned()) {
                    reason = "Tombstoned validator cannot be unjailed.";
                    return false;
                }
                for (const auto& queued : pending) {
                    if (queued.type() == transaction.type() &&
                        queued.toAddress() == transaction.toAddress() &&
                        queued.nonce() != transaction.nonce()) {
                        reason = "Validator lifecycle operation already has a pending conflict.";
                        return false;
                    }
                }
                break;
            }
            case core::TransactionType::GOVERNANCE_PROPOSE:
                if (context.governance().hasProposal(transaction.id())) {
                    reason = "Governance proposal already exists.";
                    return false;
                }
                break;
            case core::TransactionType::GOVERNANCE_VOTE: {
                const auto vote = core::GovernanceVotePayload::deserialize(transaction.data());
                if (vote.proposalId() != transaction.toAddress()) {
                    reason = "Governance vote target does not match payload proposal id.";
                    return false;
                }
                const auto* entry = context.validators().entryForAddress(vote.validatorAddress());
                const std::uint64_t votingWeight = entry == nullptr
                    ? 0
                    : governanceVotingWeight(*entry);
                if (!context.governance().hasProposal(transaction.toAddress()) || entry == nullptr ||
                    !entry->eligibleForConsensus() ||
                    !governanceVoterAuthorized(
                        *entry, vote.validatorAddress(), transaction.fromAddress()) ||
                    votingWeight == 0 ||
                    context.governance().hasVote(transaction.toAddress(), vote.validatorAddress())) {
                    reason = "Governance vote proposal, validator authorization, or uniqueness is invalid.";
                    return false;
                }
                if (!context.governance().proposalOpenForVoting(
                        transaction.toAddress(), context.nextBlockHeight())) {
                    reason = "Governance proposal is not open for voting at the next block height.";
                    return false;
                }
                for (const auto& queued : pending) {
                    if (queued.type() != core::TransactionType::GOVERNANCE_VOTE ||
                        queued.toAddress() != transaction.toAddress() ||
                        queued.nonce() == transaction.nonce()) continue;
                    const auto queuedVote = core::GovernanceVotePayload::deserialize(queued.data());
                    if (queuedVote.validatorAddress() == vote.validatorAddress()) {
                        reason = "Validator already has a pending vote for this proposal.";
                        return false;
                    }
                }
                break;
            }
            default:
                break;
        }
    } catch (const std::exception& error) {
        reason = error.what();
        return false;
    }
    reason.clear();
    return true;
}

} // namespace nodo::node
