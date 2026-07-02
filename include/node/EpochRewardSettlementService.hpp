#ifndef NODO_NODE_EPOCH_REWARD_SETTLEMENT_SERVICE_HPP
#define NODO_NODE_EPOCH_REWARD_SETTLEMENT_SERVICE_HPP

#include "core/AccountStateView.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "economics/EpochRewardDistributor.hpp"
#include "node/RewardDistribution.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class NodeRuntime;

/*
 * Complete result of settling the preceding validator epoch in the first block
 * of the next epoch.  Canonical records, account credits and supply creation
 * are kept together so callers cannot commit one without validating the other.
 */
class EpochRewardSettlement {
public:
    EpochRewardSettlement();

    EpochRewardSettlement(
        economics::EpochRewardDistribution distribution,
        std::vector<core::LedgerRecord> canonicalRecords,
        std::vector<RewardDistribution> rewardDistributions,
        core::AccountStateView updatedAccounts,
        utils::Amount totalMinted,
        std::uint64_t settledAtBlock
    );

    const economics::EpochRewardDistribution& distribution() const;
    const std::vector<core::LedgerRecord>& canonicalRecords() const;
    const std::vector<RewardDistribution>& rewardDistributions() const;
    const core::AccountStateView& updatedAccounts() const;
    utils::Amount totalMinted() const;
    std::uint64_t settledAtBlock() const;
    bool isValid() const;
    std::string serialize() const;

private:
    economics::EpochRewardDistribution m_distribution;
    std::vector<core::LedgerRecord> m_canonicalRecords;
    std::vector<RewardDistribution> m_rewardDistributions;
    core::AccountStateView m_updatedAccounts;
    utils::Amount m_totalMinted;
    std::uint64_t m_settledAtBlock;
};

class EpochRewardSettlementService {
public:
    static bool isSettlementHeight(std::uint64_t candidateHeight);
    static std::uint64_t settledEpochForHeight(std::uint64_t candidateHeight);

    static EpochRewardSettlement buildForCandidate(
        const NodeRuntime& runtime,
        std::uint64_t candidateHeight,
        std::int64_t candidateTimestamp
    );

    static bool candidateRecordsMatch(
        const NodeRuntime& runtime,
        const core::Block& candidate,
        std::string& rejectionReason
    );

    static bool finalizedRecordsMatch(
        const NodeRuntime& runtime,
        const core::Block& finalizedBlock,
        utils::Amount supplyBefore,
        std::string& rejectionReason
    );

    /*
     * Replay-side validation and application.  This validates the self-contained
     * work/score/epoch/reward bundle against EpochEmissionPolicy before crediting
     * accounts.  Finalization evidence itself is independently checked by
     * candidateRecordsMatch before a validator votes.
     */
    static EpochRewardSettlement settleCanonicalRecords(
        std::uint64_t blockHeight,
        std::int64_t blockTimestamp,
        const std::vector<core::LedgerRecord>& records,
        utils::Amount supplyBefore,
        const core::AccountStateView& currentAccounts
    );

    static core::AccountStateView applyDistributions(
        const std::vector<RewardDistribution>& distributions,
        const core::AccountStateView& accounts
    );

private:
    static std::vector<core::LedgerRecord> epochRecords(
        const std::vector<core::LedgerRecord>& records
    );

    static EpochRewardSettlement buildFromFinalizedHistory(
        const NodeRuntime& runtime,
        std::uint64_t settlementHeight,
        std::int64_t settlementTimestamp,
        utils::Amount supplyBefore
    );
};

} // namespace nodo::node

#endif
