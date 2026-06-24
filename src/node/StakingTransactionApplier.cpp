#include "node/StakingTransactionApplier.hpp"

#include "core/TransactionType.hpp"

#include <sstream>

namespace nodo::node {

std::string stakingApplyStatusToString(StakingApplyStatus status) {
    switch (status) {
        case StakingApplyStatus::APPLIED:                return "APPLIED";
        case StakingApplyStatus::INSUFFICIENT_BALANCE:   return "INSUFFICIENT_BALANCE";
        case StakingApplyStatus::INSUFFICIENT_STAKE:     return "INSUFFICIENT_STAKE";
        case StakingApplyStatus::VALIDATOR_JAILED:       return "VALIDATOR_JAILED";
        case StakingApplyStatus::VALIDATOR_TOMBSTONED:   return "VALIDATOR_TOMBSTONED";
        case StakingApplyStatus::INVALID_TRANSACTION:    return "INVALID_TRANSACTION";
        case StakingApplyStatus::COOLDOWN_NOT_ELAPSED:   return "COOLDOWN_NOT_ELAPSED";
        case StakingApplyStatus::BELOW_MINIMUM_STAKE:    return "BELOW_MINIMUM_STAKE";
        case StakingApplyStatus::ALREADY_UNBONDING:      return "ALREADY_UNBONDING";
        default:                                          return "INVALID_TRANSACTION";
    }
}

StakingApplyResult::StakingApplyResult()
    : m_status(StakingApplyStatus::INVALID_TRANSACTION)
    , m_reason("Uninitialized.")
    , m_updatedStake()
    , m_accountBalanceDelta(utils::Amount())
{}

StakingApplyResult StakingApplyResult::applied(
    economics::StakeAccount updatedStake,
    utils::Amount           accountBalanceDelta
) {
    StakingApplyResult r;
    r.m_status              = StakingApplyStatus::APPLIED;
    r.m_reason              = "";
    r.m_updatedStake        = std::move(updatedStake);
    r.m_accountBalanceDelta = accountBalanceDelta;
    return r;
}

StakingApplyResult StakingApplyResult::rejected(
    StakingApplyStatus status,
    std::string        reason
) {
    StakingApplyResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

StakingApplyStatus             StakingApplyResult::status()              const { return m_status; }
const std::string&             StakingApplyResult::reason()              const { return m_reason; }
bool                           StakingApplyResult::applied()             const { return m_status == StakingApplyStatus::APPLIED; }
const economics::StakeAccount& StakingApplyResult::updatedStake()        const { return m_updatedStake; }
utils::Amount                  StakingApplyResult::accountBalanceDelta() const { return m_accountBalanceDelta; }

std::string StakingApplyResult::serialize() const {
    std::ostringstream oss;
    oss << "StakingApplyResult{"
        << "status=" << stakingApplyStatusToString(m_status)
        << ";reason=" << m_reason
        << ";balanceDelta=" << m_accountBalanceDelta.rawUnits()
        << "}";
    return oss.str();
}

StakingApplyResult StakingTransactionApplier::apply(
    const core::Transaction&       tx,
    const economics::StakeAccount& stake,
    utils::Amount                  senderBalance,
    std::uint64_t                  currentHeight,
    utils::Amount                  minimumStake
) {
    const bool isHandled =
        core::isStakingTransaction(tx.type()) ||
        tx.type() == core::TransactionType::VALIDATOR_EXIT_REQUEST;

    if (!isHandled) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::INVALID_TRANSACTION,
            "Transaction type is not handled by StakingTransactionApplier."
        );
    }

    if (tx.fromAddress().empty()) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::INVALID_TRANSACTION,
            "Staking transaction missing required fields."
        );
    }

    // STAKE_DEPOSIT and STAKE_TOP_UP: add to bonded amount.
    const bool isDepositLike =
        tx.type() == core::TransactionType::STAKE_DEPOSIT ||
        tx.type() == core::TransactionType::STAKE_TOP_UP;

    if (isDepositLike) {
        if (tx.amount().rawUnits() <= 0) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::INVALID_TRANSACTION,
                "Deposit amount must be positive."
            );
        }

        // Enforce minimum stake if provided.
        const std::int64_t projectedBonded =
            stake.bondedAmount().rawUnits() + tx.amount().rawUnits();
        if (minimumStake.isPositive() && projectedBonded < minimumStake.rawUnits()) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::BELOW_MINIMUM_STAKE,
                "Resulting bonded stake below minimum required."
            );
        }

        const utils::Amount total = utils::Amount::fromRawUnits(
            tx.amount().rawUnits() + tx.fee().rawUnits()
        );
        if (senderBalance.rawUnits() < total.rawUnits()) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::INSUFFICIENT_BALANCE,
                "Sender balance insufficient for stake deposit."
            );
        }

        economics::StakeAccount next(
            stake.validatorAddress(),
            utils::Amount::fromRawUnits(
                stake.bondedAmount().rawUnits() + tx.amount().rawUnits()
            )
        );
        if (stake.jailed())     next.jail();
        if (stake.tombstoned()) next.tombstone();

        // Caller deducts (amount + fee) from sender balance separately.
        return StakingApplyResult::applied(std::move(next), utils::Amount());
    }

    // VALIDATOR_EXIT_REQUEST: records intent, stake account unchanged, sender pays fee.
    if (tx.type() == core::TransactionType::VALIDATOR_EXIT_REQUEST) {
        if (stake.tombstoned()) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::VALIDATOR_TOMBSTONED,
                "Tombstoned validator cannot request exit."
            );
        }
        if (stake.jailed()) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::VALIDATOR_JAILED,
                "Jailed validator cannot request exit directly; unjail first."
            );
        }

        const std::int64_t bondedRaw  = stake.bondedAmount().rawUnits();
        const std::int64_t slashedRaw = stake.slashedAmount().rawUnits();
        if ((bondedRaw <= slashedRaw) || bondedRaw == 0) {
            return StakingApplyResult::rejected(
                StakingApplyStatus::INSUFFICIENT_STAKE,
                "Validator has no stake to exit."
            );
        }

        // Caller deducts fee from sender balance separately.
        return StakingApplyResult::applied(stake, utils::Amount());
    }

    // STAKE_WITHDRAW
    if (tx.amount().rawUnits() <= 0) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::INVALID_TRANSACTION,
            "Withdraw amount must be positive."
        );
    }

    if (stake.tombstoned()) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::VALIDATOR_TOMBSTONED,
            "Tombstoned validator cannot withdraw stake."
        );
    }
    if (stake.jailed()) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::VALIDATOR_JAILED,
            "Jailed validator cannot withdraw stake."
        );
    }

    const std::int64_t bondedRaw  = stake.bondedAmount().rawUnits();
    const std::int64_t slashedRaw = stake.slashedAmount().rawUnits();
    const std::int64_t availableRaw = (bondedRaw > slashedRaw) ? (bondedRaw - slashedRaw) : 0;

    if (tx.amount().rawUnits() > availableRaw) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::INSUFFICIENT_STAKE,
            "Withdraw amount exceeds available bonded stake."
        );
    }

    // Enforce unbonding delay: nonce field carries the requested-at block height.
    if (currentHeight < tx.nonce() + UNBONDING_DELAY_BLOCKS) {
        return StakingApplyResult::rejected(
            StakingApplyStatus::COOLDOWN_NOT_ELAPSED,
            "Unbonding delay has not elapsed. Wait until block "
            + std::to_string(tx.nonce() + UNBONDING_DELAY_BLOCKS) + "."
        );
    }

    economics::StakeAccount next(
        stake.validatorAddress(),
        utils::Amount::fromRawUnits(
            stake.bondedAmount().rawUnits() - tx.amount().rawUnits()
        )
    );
    if (stake.jailed())     next.jail();
    if (stake.tombstoned()) next.tombstone();

    return StakingApplyResult::applied(
        std::move(next),
        utils::Amount::fromRawUnits(tx.amount().rawUnits() - tx.fee().rawUnits())
    );
}

} // namespace nodo::node
