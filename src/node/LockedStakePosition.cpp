#include "node/LockedStakePosition.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::string boolToCanonicalString(
    bool value
) {
    return value ? "true" : "false";
}

} // namespace

LockedStakePosition::LockedStakePosition()
    : m_ownerAddress(""),
      m_amount(),
      m_createdAtHeight(0),
      m_unlockAtHeight(0),
      m_slashable(false),
      m_sourceRewardId("") {}

LockedStakePosition::LockedStakePosition(
    std::string ownerAddress,
    utils::Amount amount,
    std::uint64_t createdAtHeight,
    std::uint64_t unlockAtHeight,
    bool slashable,
    std::string sourceRewardId
)
    : m_ownerAddress(std::move(ownerAddress)),
      m_amount(amount),
      m_createdAtHeight(createdAtHeight),
      m_unlockAtHeight(unlockAtHeight),
      m_slashable(slashable),
      m_sourceRewardId(std::move(sourceRewardId)) {}

const std::string& LockedStakePosition::ownerAddress() const {
    return m_ownerAddress;
}

utils::Amount LockedStakePosition::amount() const {
    return m_amount;
}

std::uint64_t LockedStakePosition::createdAtHeight() const {
    return m_createdAtHeight;
}

std::uint64_t LockedStakePosition::unlockAtHeight() const {
    return m_unlockAtHeight;
}

bool LockedStakePosition::slashable() const {
    return m_slashable;
}

const std::string& LockedStakePosition::sourceRewardId() const {
    return m_sourceRewardId;
}

bool LockedStakePosition::isMatureAt(
    std::uint64_t height
) const {
    return isValid() &&
           height >= m_unlockAtHeight;
}

bool LockedStakePosition::isValid() const {
    return !m_ownerAddress.empty() &&
           m_amount.isPositive() &&
           m_createdAtHeight > 0 &&
           m_unlockAtHeight > m_createdAtHeight &&
           !m_sourceRewardId.empty();
}

std::string LockedStakePosition::serialize() const {
    std::ostringstream oss;

    oss << "LockedStakePosition{"
        << "ownerAddress=" << m_ownerAddress
        << ";amountRawUnits=" << m_amount.rawUnits()
        << ";createdAtHeight=" << m_createdAtHeight
        << ";unlockAtHeight=" << m_unlockAtHeight
        << ";slashable=" << boolToCanonicalString(m_slashable)
        << ";sourceRewardId=" << m_sourceRewardId
        << "}";

    return oss.str();
}

std::string LockedStakePositionBuilder::sourceRewardId(
    const RewardDistribution& rewardDistribution
) {
    if (!rewardDistribution.isValid()) {
        throw std::invalid_argument("Cannot build locked stake source id from invalid reward distribution.");
    }

    std::ostringstream oss;

    oss << "reward:"
        << rewardDistribution.blockHeight()
        << ":"
        << rewardDistribution.validatorAddress()
        << ":"
        << rewardDistribution.totalReward().rawUnits()
        << ":"
        << rewardDistribution.liquidReward().rawUnits()
        << ":"
        << rewardDistribution.lockedReward().rawUnits()
        << ":"
        << rewardDistribution.reason();

    return oss.str();
}

LockedStakePosition LockedStakePositionBuilder::buildFromRewardDistribution(
    const RewardDistribution& rewardDistribution,
    std::uint64_t lockPeriodBlocks
) {
    if (!rewardDistribution.isValid()) {
        throw std::invalid_argument("Cannot build locked stake from invalid reward distribution.");
    }

    if (rewardDistribution.lockedReward().isZero()) {
        throw std::invalid_argument("Cannot build locked stake from zero locked reward.");
    }

    if (lockPeriodBlocks == 0) {
        throw std::invalid_argument("Locked stake period must be greater than zero.");
    }

    if (rewardDistribution.blockHeight() >
        std::numeric_limits<std::uint64_t>::max() - lockPeriodBlocks) {
        throw std::overflow_error("Locked stake unlock height would overflow.");
    }

    return LockedStakePosition(
        rewardDistribution.validatorAddress(),
        rewardDistribution.lockedReward(),
        rewardDistribution.blockHeight(),
        rewardDistribution.blockHeight() + lockPeriodBlocks,
        true,
        sourceRewardId(rewardDistribution)
    );
}

std::vector<LockedStakePosition> LockedStakePositionBuilder::buildFromRewardDistributions(
    const std::vector<RewardDistribution>& rewardDistributions,
    std::uint64_t lockPeriodBlocks
) {
    std::vector<LockedStakePosition> positions;

    for (const RewardDistribution& reward : rewardDistributions) {
        if (!reward.isValid()) {
            throw std::invalid_argument("Cannot build locked stake positions from invalid reward distribution.");
        }

        if (reward.lockedReward().isZero()) {
            continue;
        }

        positions.push_back(
            buildFromRewardDistribution(
                reward,
                lockPeriodBlocks
            )
        );
    }

    return positions;
}

utils::Amount LockedStakePositionBuilder::totalLockedAmount(
    const std::vector<LockedStakePosition>& positions
) {
    utils::Amount total;

    for (const LockedStakePosition& position : positions) {
        if (!position.isValid()) {
            throw std::invalid_argument("Cannot sum invalid locked stake position.");
        }

        total = total + position.amount();
    }

    return total;
}

bool LockedStakePositionBuilder::samePositions(
    const std::vector<LockedStakePosition>& left,
    const std::vector<LockedStakePosition>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::node
