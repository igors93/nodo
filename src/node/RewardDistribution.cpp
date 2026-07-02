#include "node/RewardDistribution.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

utils::Amount lockedPortion(
    utils::Amount totalReward
) {
    const std::int64_t raw =
        totalReward.rawUnits();

    return utils::Amount::fromRawUnits(
        (raw * VALIDATOR_REWARD_LOCKED_NUMERATOR)
        / VALIDATOR_REWARD_LOCKED_DENOMINATOR
    );
}

std::vector<std::string> canonicalValidatorAddresses(
    const std::vector<std::string>& validatorAddresses
) {
    std::set<std::string> uniqueAddresses;

    for (const std::string& address : validatorAddresses) {
        if (address.empty()) {
            throw std::invalid_argument("Reward distribution validator address is empty.");
        }

        uniqueAddresses.insert(address);
    }

    return std::vector<std::string>(
        uniqueAddresses.begin(),
        uniqueAddresses.end()
    );
}

} // namespace

RewardDistribution::RewardDistribution()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_totalReward(),
      m_liquidReward(),
      m_lockedReward(),
      m_reason("") {}

RewardDistribution::RewardDistribution(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    utils::Amount totalReward,
    utils::Amount liquidReward,
    utils::Amount lockedReward,
    std::string reason
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_totalReward(totalReward),
      m_liquidReward(liquidReward),
      m_lockedReward(lockedReward),
      m_reason(std::move(reason)) {}

const std::string& RewardDistribution::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t RewardDistribution::blockHeight() const {
    return m_blockHeight;
}

utils::Amount RewardDistribution::totalReward() const {
    return m_totalReward;
}

utils::Amount RewardDistribution::liquidReward() const {
    return m_liquidReward;
}

utils::Amount RewardDistribution::lockedReward() const {
    return m_lockedReward;
}

const std::string& RewardDistribution::reason() const {
    return m_reason;
}

bool RewardDistribution::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        (m_reason != RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON &&
         m_reason != RewardDistributionCalculator::EPOCH_PROTECTION_REWARD_REASON) ||
        !m_totalReward.isPositive() ||
        m_liquidReward.isNegative() ||
        m_lockedReward.isNegative()) {
        return false;
    }

    try {
        return (m_liquidReward + m_lockedReward) == m_totalReward;
    } catch (const std::exception&) {
        return false;
    }
}

std::string RewardDistribution::serialize() const {
    std::ostringstream oss;

    oss << "RewardDistribution{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";totalRewardRawUnits=" << m_totalReward.rawUnits()
        << ";liquidRewardRawUnits=" << m_liquidReward.rawUnits()
        << ";lockedRewardRawUnits=" << m_lockedReward.rawUnits()
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

std::vector<RewardDistribution> RewardDistributionCalculator::buildForVoters(
    utils::Amount totalReward,
    const std::vector<std::string>& validatorAddresses,
    std::uint64_t blockHeight
) {
    if (totalReward.isNegative()) {
        throw std::invalid_argument("Cannot distribute a negative validator reward.");
    }

    if (totalReward.isZero()) {
        return {};
    }

    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot distribute validator rewards for genesis height.");
    }

    const std::vector<std::string> recipients =
        canonicalValidatorAddresses(validatorAddresses);

    if (recipients.empty()) {
        throw std::invalid_argument("Cannot distribute validator rewards without recipients.");
    }

    const std::int64_t totalRaw =
        totalReward.rawUnits();

    const std::int64_t baseReward =
        totalRaw / static_cast<std::int64_t>(recipients.size());

    const std::int64_t remainder =
        totalRaw % static_cast<std::int64_t>(recipients.size());

    std::vector<RewardDistribution> distributions;
    distributions.reserve(recipients.size());

    for (std::size_t index = 0; index < recipients.size(); ++index) {
        const std::int64_t totalForValidator =
            baseReward + (static_cast<std::int64_t>(index) < remainder ? 1 : 0);

        if (totalForValidator <= 0) {
            continue;
        }

        const utils::Amount total =
            utils::Amount::fromRawUnits(totalForValidator);

        const utils::Amount locked =
            lockedPortion(total);

        const utils::Amount liquid =
            total - locked;

        distributions.emplace_back(
            recipients[index],
            blockHeight,
            total,
            liquid,
            locked,
            BLOCK_FINALIZATION_FEE_REASON
        );
    }

    return distributions;
}

std::vector<RewardDistribution> RewardDistributionCalculator::buildFromQuorumCertificate(
    utils::Amount totalReward,
    const consensus::QuorumCertificate& certificate,
    std::uint64_t blockHeight
) {
    std::vector<std::string> validatorAddresses;

    for (const consensus::ValidatorVoteRecord& vote : certificate.votes()) {
        validatorAddresses.push_back(
            vote.validatorAddress()
        );
    }

    return buildForVoters(
        totalReward,
        validatorAddresses,
        blockHeight
    );
}

std::vector<RewardDistribution> RewardDistributionCalculator::buildFromEpochRewards(
    const std::vector<economics::GenesisRewardRecord>& rewardRecords,
    std::uint64_t settlementBlockHeight
) {
    if (settlementBlockHeight == 0) {
        throw std::invalid_argument("Cannot distribute epoch rewards at genesis height.");
    }
    std::set<std::string> recipients;
    std::vector<RewardDistribution> distributions;
    distributions.reserve(rewardRecords.size());
    for (const auto& reward : rewardRecords) {
        if (!reward.isValid() || !recipients.insert(reward.validatorAddress()).second) {
            throw std::invalid_argument("Invalid or duplicate canonical epoch reward.");
        }
        distributions.emplace_back(
            reward.validatorAddress(), settlementBlockHeight,
            reward.amount(), reward.amount(), utils::Amount(),
            EPOCH_PROTECTION_REWARD_REASON
        );
    }
    return distributions;
}

utils::Amount RewardDistributionCalculator::totalReward(
    const std::vector<RewardDistribution>& distributions
) {
    utils::Amount total;

    for (const RewardDistribution& distribution : distributions) {
        if (!distribution.isValid()) {
            throw std::invalid_argument("Cannot sum invalid reward distribution.");
        }

        total = total + distribution.totalReward();
    }

    return total;
}

bool RewardDistributionCalculator::sameDistributions(
    const std::vector<RewardDistribution>& left,
    const std::vector<RewardDistribution>& right
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
