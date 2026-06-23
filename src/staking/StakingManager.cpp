#include "staking/StakingManager.hpp"
#include <algorithm>

namespace nodo::staking {

StakingManager::StakingManager(uint64_t unbondingPeriodBlocks)
    : m_unbondingPeriodBlocks(unbondingPeriodBlocks) {}

bool StakingManager::bondStake(const std::string& validatorAddress, uint64_t amount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (amount == 0 || validatorAddress.empty()) {
        return false;
    }
    m_bondedStakes[validatorAddress] += amount;
    return true;
}

bool StakingManager::initiateUnbond(const std::string& validatorAddress, uint64_t amount, uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (validatorAddress.empty() || amount == 0) {
        return false;
    }
    auto it = m_bondedStakes.find(validatorAddress);
    if (it == m_bondedStakes.end() || it->second < amount) {
        return false;
    }
    it->second -= amount;
    m_unbondingQueue.push_back({validatorAddress, amount, currentBlock + m_unbondingPeriodBlocks});
    return true;
}

void StakingManager::processUnbondingQueue(uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_unbondingQueue.begin(), m_unbondingQueue.end(),
        [this, currentBlock](const UnbondingRequest& req) {
            if (currentBlock >= req.unlockBlock) {
                m_unlockedBalances[req.validatorAddress] += req.amount;
                return true;
            }
            return false;
        });
    m_unbondingQueue.erase(it, m_unbondingQueue.end());
}

uint64_t StakingManager::getUnlockedBalance(const std::string& validatorAddress) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_unlockedBalances.find(validatorAddress);
    return (it != m_unlockedBalances.end()) ? it->second : 0;
}

uint64_t StakingManager::claimUnlocked(const std::string& validatorAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_unlockedBalances.find(validatorAddress);
    if (it == m_unlockedBalances.end()) {
        return 0;
    }
    const uint64_t amount = it->second;
    m_unlockedBalances.erase(it);
    return amount;
}

void StakingManager::jailValidator(const std::string& validatorAddress, uint64_t jailDurationBlocks, uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_jailedUntilBlock[validatorAddress] = currentBlock + jailDurationBlocks;
}

void StakingManager::slashValidator(const std::string& validatorAddress, double slashFraction, uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (slashFraction <= 0.0 || slashFraction > 1.0) {
        return;
    }
    auto it = m_bondedStakes.find(validatorAddress);
    if (it != m_bondedStakes.end()) {
        uint64_t slashAmount = static_cast<uint64_t>(it->second * slashFraction);
        it->second = (it->second > slashAmount) ? (it->second - slashAmount) : 0;
    }
    m_jailedUntilBlock[validatorAddress] = currentBlock + 100; // Jail validator for at least 100 blocks
}

void StakingManager::unjailValidator(const std::string& validatorAddress, uint64_t currentBlock) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jailedUntilBlock.find(validatorAddress);
    if (it != m_jailedUntilBlock.end() && currentBlock >= it->second) {
        m_jailedUntilBlock.erase(it);
    }
}

std::vector<std::string> StakingManager::getActiveValidatorSet(size_t maxCount, uint64_t currentBlock) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, uint64_t>> sortedValidators;
    for (const auto& [addr, stake] : m_bondedStakes) {
        const auto jailedUntil =
            m_jailedUntilBlock.find(addr);
        if (jailedUntil == m_jailedUntilBlock.end() ||
            currentBlock >= jailedUntil->second) {
            sortedValidators.push_back({addr, stake});
        }
    }

    std::sort(sortedValidators.begin(), sortedValidators.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

    std::vector<std::string> activeSet;
    for (size_t i = 0; i < sortedValidators.size() && i < maxCount; ++i) {
        activeSet.push_back(sortedValidators[i].first);
    }
    return activeSet;
}

uint64_t StakingManager::getValidatorStake(const std::string& validatorAddress) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bondedStakes.find(validatorAddress);
    return (it != m_bondedStakes.end()) ? it->second : 0;
}

bool StakingManager::isJailed(const std::string& validatorAddress, uint64_t currentBlock) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jailedUntilBlock.find(validatorAddress);
    if (it == m_jailedUntilBlock.end()) {
        return false;
    }
    return currentBlock < it->second;
}

} // namespace nodo::staking
