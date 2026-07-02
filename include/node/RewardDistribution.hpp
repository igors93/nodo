#ifndef NODO_NODE_REWARD_DISTRIBUTION_HPP
#define NODO_NODE_REWARD_DISTRIBUTION_HPP

#include "consensus/QuorumCertificate.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::int64_t VALIDATOR_REWARD_LOCKED_NUMERATOR = 1;
constexpr std::int64_t VALIDATOR_REWARD_LOCKED_DENOMINATOR = 10;

class RewardDistribution {
public:
    RewardDistribution();

    RewardDistribution(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        utils::Amount totalReward,
        utils::Amount liquidReward,
        utils::Amount lockedReward,
        std::string reason
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    utils::Amount totalReward() const;
    utils::Amount liquidReward() const;
    utils::Amount lockedReward() const;
    const std::string& reason() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    utils::Amount m_totalReward;
    utils::Amount m_liquidReward;
    utils::Amount m_lockedReward;
    std::string m_reason;
};

class RewardDistributionCalculator {
public:
    static constexpr const char* BLOCK_FINALIZATION_FEE_REASON =
        "BLOCK_FINALIZATION_FEE";

    static constexpr const char* EPOCH_PROTECTION_REWARD_REASON =
        "EPOCH_PROTECTION_REWARD";

    static std::vector<RewardDistribution> buildForVoters(
        utils::Amount totalReward,
        const std::vector<std::string>& validatorAddresses,
        std::uint64_t blockHeight
    );

    static std::vector<RewardDistribution> buildFromQuorumCertificate(
        utils::Amount totalReward,
        const consensus::QuorumCertificate& certificate,
        std::uint64_t blockHeight
    );

    static std::vector<RewardDistribution> buildFromEpochRewards(
        const std::vector<economics::GenesisRewardRecord>& rewardRecords,
        std::uint64_t settlementBlockHeight
    );

    static utils::Amount totalReward(
        const std::vector<RewardDistribution>& distributions
    );

    static bool sameDistributions(
        const std::vector<RewardDistribution>& left,
        const std::vector<RewardDistribution>& right
    );
};

} // namespace nodo::node

#endif
