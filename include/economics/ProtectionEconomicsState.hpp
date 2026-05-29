#ifndef NODO_ECONOMICS_PROTECTION_ECONOMICS_STATE_HPP
#define NODO_ECONOMICS_PROTECTION_ECONOMICS_STATE_HPP

#include "core/CoinLot.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * ProtectionEconomicsState is a rebuildable view of Nodo's protection economy.
 *
 * In simple terms:
 * This is the "read result" created from blockchain history.
 *
 * It tracks:
 * - useful work accepted by validator;
 * - latest validator score;
 * - protection epoch reward totals;
 * - GenesisReward coin lots born from protection work.
 *
 * Security principle:
 * This state must be rebuildable from accepted LedgerRecords.
 */
class ProtectionEconomicsState {
public:
    ProtectionEconomicsState();

    void applyAcceptedWork(
        const std::string& validatorAddress,
        std::uint64_t workWeight
    );

    void applyValidatorScore(
        const std::string& validatorAddress,
        std::int32_t score
    );

    void applyProtectionEpochTotals(
        utils::Amount securityEmission,
        utils::Amount rewardPool
    );

    void applyGenesisReward(
        const std::string& rewardId,
        const std::string& validatorAddress,
        utils::Amount amount,
        std::uint64_t createdAtBlock,
        std::int64_t timestamp
    );

    bool hasValidator(
        const std::string& validatorAddress
    ) const;

    std::uint64_t acceptedWorkWeight(
        const std::string& validatorAddress
    ) const;

    std::int32_t validatorScore(
        const std::string& validatorAddress
    ) const;

    std::uint64_t totalAcceptedWorkWeight() const;
    std::uint64_t workRecordCount() const;
    std::uint64_t scoreRecordCount() const;
    std::uint64_t protectionEpochCount() const;
    std::uint64_t genesisRewardCount() const;

    utils::Amount totalSecurityEmission() const;
    utils::Amount totalRewardPool() const;
    utils::Amount totalGenesisRewards() const;

    const std::map<std::string, std::uint64_t>& acceptedWorkByValidator() const;
    const std::map<std::string, std::int32_t>& latestScoreByValidator() const;
    const std::vector<core::CoinLot>& rewardCoinLots() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::map<std::string, std::uint64_t> m_acceptedWorkByValidator;
    std::map<std::string, std::int32_t> m_latestScoreByValidator;
    std::vector<core::CoinLot> m_rewardCoinLots;

    std::uint64_t m_totalAcceptedWorkWeight;
    std::uint64_t m_workRecordCount;
    std::uint64_t m_scoreRecordCount;
    std::uint64_t m_protectionEpochCount;
    std::uint64_t m_genesisRewardCount;

    utils::Amount m_totalSecurityEmission;
    utils::Amount m_totalRewardPool;
    utils::Amount m_totalGenesisRewards;
};

} // namespace nodo::economics

#endif
