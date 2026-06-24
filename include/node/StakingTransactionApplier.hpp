#ifndef NODO_NODE_STAKING_TRANSACTION_APPLIER_HPP
#define NODO_NODE_STAKING_TRANSACTION_APPLIER_HPP

#include "core/Transaction.hpp"
#include "economics/StakeAccount.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::node {

/*
 * StakingTransactionApplier validates and applies STAKE_DEPOSIT and
 * STAKE_WITHDRAW transactions against the caller's StakeAccount.
 *
 * Rules:
 * - STAKE_DEPOSIT: amount > 0, sender must have sufficient balance.
 *   Bonded amount increases by tx.amount().
 * - STAKE_WITHDRAW: amount <= bonded - slashed, not jailed or tombstoned.
 *   A cooldown block delay is enforced: withdraw is only executable after
 *   UNBONDING_DELAY_BLOCKS blocks have passed since the request.
 *
 * Security principle:
 * Neither operation mutates account balance directly — the caller must credit /
 * debit the account state separately using the returned delta.  This keeps the
 * staking applier stateless and auditable.
 */
enum class StakingApplyStatus {
    APPLIED,
    INSUFFICIENT_BALANCE,
    INSUFFICIENT_STAKE,
    VALIDATOR_JAILED,
    VALIDATOR_TOMBSTONED,
    INVALID_TRANSACTION,
    COOLDOWN_NOT_ELAPSED,
    BELOW_MINIMUM_STAKE,
    ALREADY_UNBONDING
};

std::string stakingApplyStatusToString(StakingApplyStatus status);

class StakingApplyResult {
public:
    StakingApplyResult();

    static StakingApplyResult applied(
        economics::StakeAccount updatedStake,
        utils::Amount           accountBalanceDelta
    );
    static StakingApplyResult rejected(
        StakingApplyStatus status,
        std::string        reason
    );

    StakingApplyStatus             status()             const;
    const std::string&             reason()             const;
    bool                           applied()            const;
    const economics::StakeAccount& updatedStake()       const;
    utils::Amount                  accountBalanceDelta() const;
    std::string                    serialize()          const;

private:
    StakingApplyStatus    m_status;
    std::string           m_reason;
    economics::StakeAccount m_updatedStake;
    utils::Amount         m_accountBalanceDelta;
};

class StakingTransactionApplier {
public:
    static constexpr std::uint64_t UNBONDING_DELAY_BLOCKS = 21;
    static constexpr std::int64_t  MIN_STAKE_RAW_UNITS     = 1'000'000;

    /*
     * Apply a STAKE_DEPOSIT, STAKE_TOP_UP, STAKE_WITHDRAW, or
     * VALIDATOR_EXIT_REQUEST transaction.
     *
     * @param tx            The signed staking/lifecycle transaction.
     * @param stake         Current stake account for the validator.
     * @param senderBalance Current liquid balance of the sender account.
     * @param currentHeight Current block height (used for cooldown).
     * @param minimumStake  Minimum required bonded stake (0 = no check).
     */
    static StakingApplyResult apply(
        const core::Transaction&       tx,
        const economics::StakeAccount& stake,
        utils::Amount                  senderBalance,
        std::uint64_t                  currentHeight,
        utils::Amount                  minimumStake = utils::Amount()
    );
};

} // namespace nodo::node

#endif
