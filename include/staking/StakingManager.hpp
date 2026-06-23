#ifndef NODO_STAKING_STAKING_MANAGER_HPP
#define NODO_STAKING_STAKING_MANAGER_HPP

#include "core/CoinLot.hpp"
#include "crypto/Address.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>

namespace nodo::staking {

struct UnbondingRequest {
    std::string validatorAddress;
    uint64_t amount;
    uint64_t unlockBlock;
};

class StakingManager {
public:
    explicit StakingManager(uint64_t unbondingPeriodBlocks = 100);

    bool bondStake(const std::string& validatorAddress, uint64_t amount);
    bool initiateUnbond(const std::string& validatorAddress, uint64_t amount, uint64_t currentBlock);
    void processUnbondingQueue(uint64_t currentBlock);

    uint64_t getUnlockedBalance(const std::string& validatorAddress) const;
    uint64_t claimUnlocked(const std::string& validatorAddress);

    void jailValidator(const std::string& validatorAddress, uint64_t jailDurationBlocks, uint64_t currentBlock);
    void slashValidator(const std::string& validatorAddress, double slashFraction, uint64_t currentBlock);
    void unjailValidator(const std::string& validatorAddress, uint64_t currentBlock);

    std::vector<std::string> getActiveValidatorSet(size_t maxCount, uint64_t currentBlock = 0) const;
    uint64_t getValidatorStake(const std::string& validatorAddress) const;
    bool isJailed(const std::string& validatorAddress, uint64_t currentBlock) const;

private:
    uint64_t m_unbondingPeriodBlocks;
    std::map<std::string, uint64_t> m_bondedStakes;
    std::map<std::string, uint64_t> m_jailedUntilBlock;
    std::vector<UnbondingRequest> m_unbondingQueue;
    std::map<std::string, uint64_t> m_unlockedBalances;
    mutable std::mutex m_mutex;
};

} // namespace nodo::staking

#endif
