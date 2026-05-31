#ifndef NODO_NODE_LOCKED_STAKE_POSITION_HPP
#define NODO_NODE_LOCKED_STAKE_POSITION_HPP

#include "node/RewardDistribution.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::uint64_t VALIDATOR_REWARD_LOCK_PERIOD_BLOCKS = 43200;

class LockedStakePosition {
public:
    LockedStakePosition();

    LockedStakePosition(
        std::string ownerAddress,
        utils::Amount amount,
        std::uint64_t createdAtHeight,
        std::uint64_t unlockAtHeight,
        bool slashable,
        std::string sourceRewardId
    );

    const std::string& ownerAddress() const;
    utils::Amount amount() const;
    std::uint64_t createdAtHeight() const;
    std::uint64_t unlockAtHeight() const;
    bool slashable() const;
    const std::string& sourceRewardId() const;

    bool isMatureAt(
        std::uint64_t height
    ) const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_ownerAddress;
    utils::Amount m_amount;
    std::uint64_t m_createdAtHeight;
    std::uint64_t m_unlockAtHeight;
    bool m_slashable;
    std::string m_sourceRewardId;
};

class LockedStakePositionBuilder {
public:
    static std::string sourceRewardId(
        const RewardDistribution& rewardDistribution
    );

    static LockedStakePosition buildFromRewardDistribution(
        const RewardDistribution& rewardDistribution,
        std::uint64_t lockPeriodBlocks = VALIDATOR_REWARD_LOCK_PERIOD_BLOCKS
    );

    static std::vector<LockedStakePosition> buildFromRewardDistributions(
        const std::vector<RewardDistribution>& rewardDistributions,
        std::uint64_t lockPeriodBlocks = VALIDATOR_REWARD_LOCK_PERIOD_BLOCKS
    );

    static utils::Amount totalLockedAmount(
        const std::vector<LockedStakePosition>& positions
    );

    static bool samePositions(
        const std::vector<LockedStakePosition>& left,
        const std::vector<LockedStakePosition>& right
    );
};

} // namespace nodo::node

#endif
