#include "economics/ProtectionEconomicsState.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

ProtectionEconomicsState::ProtectionEconomicsState()
    : m_acceptedWorkByValidator(), m_latestScoreByValidator(),
      m_rewardCoinLots(), m_totalAcceptedWorkWeight(0), m_workRecordCount(0),
      m_scoreRecordCount(0), m_protectionEpochCount(0), m_genesisRewardCount(0),
      m_totalSecurityEmission(utils::Amount::fromRawUnits(0)),
      m_totalRewardPool(utils::Amount::fromRawUnits(0)),
      m_totalGenesisRewards(utils::Amount::fromRawUnits(0)) {}

void ProtectionEconomicsState::applyAcceptedWork(
    const std::string &validatorAddress, std::uint64_t workWeight) {
  if (validatorAddress.empty()) {
    throw std::invalid_argument("Validator address cannot be empty.");
  }

  if (workWeight == 0) {
    throw std::invalid_argument("Accepted work weight must be positive.");
  }

  m_acceptedWorkByValidator[validatorAddress] += workWeight;
  m_totalAcceptedWorkWeight += workWeight;
  ++m_workRecordCount;
}

void ProtectionEconomicsState::applyValidatorScore(
    const std::string &validatorAddress, std::int32_t score) {
  if (validatorAddress.empty()) {
    throw std::invalid_argument("Validator address cannot be empty.");
  }

  if (score < 0 || score > 100) {
    throw std::invalid_argument("Validator score must be between 0 and 100.");
  }

  m_latestScoreByValidator[validatorAddress] = score;
  ++m_scoreRecordCount;
}

void ProtectionEconomicsState::applyProtectionEpochTotals(
    utils::Amount securityEmission, utils::Amount rewardPool) {
  if (securityEmission.isNegative()) {
    throw std::invalid_argument("Security emission cannot be negative.");
  }

  if (rewardPool.isNegative()) {
    throw std::invalid_argument("Reward pool cannot be negative.");
  }

  if (rewardPool < securityEmission) {
    throw std::invalid_argument(
        "Reward pool cannot be lower than security emission.");
  }

  m_totalSecurityEmission = m_totalSecurityEmission + securityEmission;
  m_totalRewardPool = m_totalRewardPool + rewardPool;
  ++m_protectionEpochCount;
}

void ProtectionEconomicsState::applyGenesisReward(
    const std::string &rewardId, const std::string &validatorAddress,
    utils::Amount amount, std::uint64_t createdAtBlock,
    std::int64_t timestamp) {
  if (rewardId.empty()) {
    throw std::invalid_argument("Genesis reward id cannot be empty.");
  }

  if (validatorAddress.empty()) {
    throw std::invalid_argument("Validator address cannot be empty.");
  }

  if (!amount.isPositive()) {
    throw std::invalid_argument("Genesis reward amount must be positive.");
  }

  if (timestamp <= 0) {
    throw std::invalid_argument("Genesis reward timestamp must be positive.");
  }

  const core::CoinLot rewardLot(
      "reward_lot_" + rewardId, rewardId, validatorAddress, amount,
      core::CoinLotStatus::AVAILABLE, createdAtBlock, 0, timestamp);

  if (!rewardLot.isValid()) {
    throw std::logic_error("Genesis reward produced an invalid CoinLot.");
  }

  m_rewardCoinLots.push_back(rewardLot);
  m_totalGenesisRewards = m_totalGenesisRewards + amount;
  ++m_genesisRewardCount;
}

bool ProtectionEconomicsState::hasValidator(
    const std::string &validatorAddress) const {
  return m_acceptedWorkByValidator.find(validatorAddress) !=
             m_acceptedWorkByValidator.end() ||
         m_latestScoreByValidator.find(validatorAddress) !=
             m_latestScoreByValidator.end();
}

std::uint64_t ProtectionEconomicsState::acceptedWorkWeight(
    const std::string &validatorAddress) const {
  const auto it = m_acceptedWorkByValidator.find(validatorAddress);

  if (it == m_acceptedWorkByValidator.end()) {
    return 0;
  }

  return it->second;
}

std::int32_t ProtectionEconomicsState::validatorScore(
    const std::string &validatorAddress) const {
  const auto it = m_latestScoreByValidator.find(validatorAddress);

  if (it == m_latestScoreByValidator.end()) {
    return 0;
  }

  return it->second;
}

std::uint64_t ProtectionEconomicsState::totalAcceptedWorkWeight() const {
  return m_totalAcceptedWorkWeight;
}

std::uint64_t ProtectionEconomicsState::workRecordCount() const {
  return m_workRecordCount;
}

std::uint64_t ProtectionEconomicsState::scoreRecordCount() const {
  return m_scoreRecordCount;
}

std::uint64_t ProtectionEconomicsState::protectionEpochCount() const {
  return m_protectionEpochCount;
}

std::uint64_t ProtectionEconomicsState::genesisRewardCount() const {
  return m_genesisRewardCount;
}

utils::Amount ProtectionEconomicsState::totalSecurityEmission() const {
  return m_totalSecurityEmission;
}

utils::Amount ProtectionEconomicsState::totalRewardPool() const {
  return m_totalRewardPool;
}

utils::Amount ProtectionEconomicsState::totalGenesisRewards() const {
  return m_totalGenesisRewards;
}

const std::map<std::string, std::uint64_t> &
ProtectionEconomicsState::acceptedWorkByValidator() const {
  return m_acceptedWorkByValidator;
}

const std::map<std::string, std::int32_t> &
ProtectionEconomicsState::latestScoreByValidator() const {
  return m_latestScoreByValidator;
}

const std::vector<core::CoinLot> &
ProtectionEconomicsState::rewardCoinLots() const {
  return m_rewardCoinLots;
}

bool ProtectionEconomicsState::isValid() const {
  if (m_totalSecurityEmission.isNegative()) {
    return false;
  }

  if (m_totalRewardPool.isNegative()) {
    return false;
  }

  if (m_totalGenesisRewards.isNegative()) {
    return false;
  }

  for (const auto &scoreEntry : m_latestScoreByValidator) {
    if (scoreEntry.first.empty()) {
      return false;
    }

    if (scoreEntry.second < 0 || scoreEntry.second > 100) {
      return false;
    }
  }

  for (const auto &workEntry : m_acceptedWorkByValidator) {
    if (workEntry.first.empty()) {
      return false;
    }

    if (workEntry.second == 0) {
      return false;
    }
  }

  for (const auto &rewardLot : m_rewardCoinLots) {
    if (!rewardLot.isValid()) {
      return false;
    }

    if (!rewardLot.isAvailable()) {
      return false;
    }
  }

  return true;
}

std::string ProtectionEconomicsState::serialize() const {
  std::ostringstream oss;

  oss << "ProtectionEconomicsState{"
      << "validatorsWithWork=" << m_acceptedWorkByValidator.size()
      << ";validatorsWithScore=" << m_latestScoreByValidator.size()
      << ";rewardCoinLots=" << m_rewardCoinLots.size()
      << ";totalAcceptedWorkWeight=" << m_totalAcceptedWorkWeight
      << ";workRecordCount=" << m_workRecordCount
      << ";scoreRecordCount=" << m_scoreRecordCount
      << ";protectionEpochCount=" << m_protectionEpochCount
      << ";genesisRewardCount=" << m_genesisRewardCount
      << ";totalSecurityEmissionRaw=" << m_totalSecurityEmission.rawUnits()
      << ";totalRewardPoolRaw=" << m_totalRewardPool.rawUnits()
      << ";totalGenesisRewardsRaw=" << m_totalGenesisRewards.rawUnits() << "}";

  return oss.str();
}

} // namespace nodo::economics
