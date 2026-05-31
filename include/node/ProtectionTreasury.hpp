#ifndef NODO_NODE_PROTECTION_TREASURY_HPP
#define NODO_NODE_PROTECTION_TREASURY_HPP

#include "config/NetworkParameters.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::int64_t NODO_TREASURY_PROTECTION_BUDGET_BASIS_POINTS = 1000;

class GenesisTreasurySnapshot {
public:
    GenesisTreasurySnapshot();

    GenesisTreasurySnapshot(
        std::string status,
        std::string treasuryAddress,
        std::uint64_t blockHeight,
        utils::Amount genesisTreasuryBalance,
        utils::Amount protectedReserve,
        utils::Amount protectionBudget,
        utils::Amount availableBalance,
        std::string reason
    );

    static GenesisTreasurySnapshot notEvaluated();

    const std::string& status() const;
    const std::string& treasuryAddress() const;
    std::uint64_t blockHeight() const;
    utils::Amount genesisTreasuryBalance() const;
    utils::Amount protectedReserve() const;
    utils::Amount protectionBudget() const;
    utils::Amount availableBalance() const;
    const std::string& reason() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::string m_treasuryAddress;
    std::uint64_t m_blockHeight;
    utils::Amount m_genesisTreasuryBalance;
    utils::Amount m_protectedReserve;
    utils::Amount m_protectionBudget;
    utils::Amount m_availableBalance;
    std::string m_reason;
};

class ProtectionRewardBudget {
public:
    ProtectionRewardBudget();

    ProtectionRewardBudget(
        std::string status,
        std::uint64_t blockHeight,
        std::string treasuryAddress,
        utils::Amount availableBudget,
        utils::Amount plannedTotal,
        utils::Amount remainingBudget,
        std::uint64_t beneficiaryCount,
        std::string reason,
        std::string sourceTreasuryDigest
    );

    static ProtectionRewardBudget notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    const std::string& treasuryAddress() const;
    utils::Amount availableBudget() const;
    utils::Amount plannedTotal() const;
    utils::Amount remainingBudget() const;
    std::uint64_t beneficiaryCount() const;
    const std::string& reason() const;
    const std::string& sourceTreasuryDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::string m_treasuryAddress;
    utils::Amount m_availableBudget;
    utils::Amount m_plannedTotal;
    utils::Amount m_remainingBudget;
    std::uint64_t m_beneficiaryCount;
    std::string m_reason;
    std::string m_sourceTreasuryDigest;
};

class ProtectionRewardGrant {
public:
    ProtectionRewardGrant();

    ProtectionRewardGrant(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        utils::Amount plannedReward,
        std::uint16_t securityScore,
        std::string reason,
        std::string sourceBudgetDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    utils::Amount plannedReward() const;
    std::uint16_t securityScore() const;
    const std::string& reason() const;
    const std::string& sourceBudgetDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    utils::Amount m_plannedReward;
    std::uint16_t m_securityScore;
    std::string m_reason;
    std::string m_sourceBudgetDigest;
};

class ProtectionTreasury {
public:
    static constexpr const char* TREASURY_ADDRESS =
        "treasury-protocol-account";

    static constexpr const char* TREASURY_SNAPSHOT_REASON =
        "GENESIS_TREASURY_PROTECTION_RESERVE";

    static constexpr const char* PROTECTION_BUDGET_REASON =
        "INITIAL_PROTECTION_REWARD_BUDGET";

    static constexpr const char* PROTECTION_GRANT_REASON =
        "PLANNED_PROTECTION_REWARD";

    static constexpr const char* NOT_EVALUATED_REASON =
        "PROTECTION_TREASURY_NOT_EVALUATED";

    static utils::Amount treasuryBalanceFromGenesis(
        const config::GenesisConfig& genesisConfig
    );

    static GenesisTreasurySnapshot buildGenesisTreasurySnapshot(
        const config::GenesisConfig& genesisConfig,
        std::uint64_t blockHeight
    );

    static ProtectionRewardBudget buildProtectionRewardBudget(
        const GenesisTreasurySnapshot& treasurySnapshot,
        const std::vector<RewardDistribution>& rewardDistributions
    );

    static std::vector<ProtectionRewardGrant> buildProtectionRewardGrants(
        const ProtectionRewardBudget& budget,
        const std::vector<RewardDistribution>& rewardDistributions,
        const std::vector<SecurityScoreRecord>& securityScoreRecords
    );

    static bool sameTreasurySnapshot(
        const GenesisTreasurySnapshot& left,
        const GenesisTreasurySnapshot& right
    );

    static bool sameBudget(
        const ProtectionRewardBudget& left,
        const ProtectionRewardBudget& right
    );

    static bool sameGrants(
        const std::vector<ProtectionRewardGrant>& left,
        const std::vector<ProtectionRewardGrant>& right
    );
};

} // namespace nodo::node

#endif
