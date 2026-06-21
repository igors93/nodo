#ifndef NODO_NODE_EPOCH_REWARD_SETTLEMENT_SERVICE_HPP
#define NODO_NODE_EPOCH_REWARD_SETTLEMENT_SERVICE_HPP

#include "consensus/QuorumCertificate.hpp"
#include "core/AccountStateView.hpp"
#include "node/RewardDistribution.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * EpochRewardSettlement holds the result of one epoch reward credit pass.
 *
 * Fields:
 * - distributions   : one entry per rewarded validator.
 * - updatedAccounts : the account state view after credits are applied.
 * - totalMinted     : sum of all liquid rewards minted into accounts.
 *
 * Security principle:
 * Rewards are applied as credits to the liquid balance only.  Locked rewards
 * are tracked but not spendable until an unlock transaction is submitted.
 * The total minted must be authorised by the monetary policy; the caller is
 * responsible for verifying this before committing the result to the ledger.
 */
class EpochRewardSettlement {
public:
    EpochRewardSettlement();

    EpochRewardSettlement(
        std::vector<RewardDistribution> distributions,
        core::AccountStateView          updatedAccounts,
        utils::Amount                   totalMinted,
        std::uint64_t                   settledAtBlock
    );

    const std::vector<RewardDistribution>& distributions()    const;
    const core::AccountStateView&          updatedAccounts()  const;
    utils::Amount                          totalMinted()      const;
    std::uint64_t                          settledAtBlock()   const;

    bool isEmpty()   const;
    bool isValid()   const;
    std::string serialize() const;

private:
    std::vector<RewardDistribution> m_distributions;
    core::AccountStateView          m_updatedAccounts;
    utils::Amount                   m_totalMinted;
    std::uint64_t                   m_settledAtBlock;
};

/*
 * EpochRewardSettlementService credits validator rewards into account balances.
 *
 * Call settle() after each block that contains a quorum certificate.  The
 * service calculates per-validator liquid rewards from the fee pool and the
 * base block reward, then credits them into a copy of the account state view.
 *
 * The caller must:
 * 1. Verify the returned totalMinted against the monetary policy cap.
 * 2. Replace the current AccountStateView with updatedAccounts.
 * 3. Record the distributions in the finalized block artifact for auditing.
 */
class EpochRewardSettlementService {
public:
    /*
     * Settle block rewards for all validators that signed the quorum
     * certificate.
     *
     * @param certificate       Quorum certificate for the finalized block.
     * @param blockHeight       Height of the finalized block.
     * @param feePool           Total fees collected in this block.
     * @param baseBlockReward   Protocol base reward (from monetary policy).
     * @param currentAccounts   Account state view before settlement.
     */
    static EpochRewardSettlement settle(
        const consensus::QuorumCertificate& certificate,
        std::uint64_t                       blockHeight,
        utils::Amount                       feePool,
        utils::Amount                       baseBlockReward,
        const core::AccountStateView&       currentAccounts
    );

    /*
     * Credit a pre-computed set of distributions into an account state view.
     * Useful for re-applying rewards during state replay.
     */
    static core::AccountStateView applyDistributions(
        const std::vector<RewardDistribution>& distributions,
        const core::AccountStateView&          accounts
    );
};

} // namespace nodo::node

#endif
