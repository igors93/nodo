#ifndef NODO_NODE_VALIDATOR_LIFECYCLE_HPP
#define NODO_NODE_VALIDATOR_LIFECYCLE_HPP

#include "node/CryptographicSlashing.hpp"
#include "node/LockedStakePosition.hpp"
#include "node/ProtectionRewards.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::uint64_t NODO_VALIDATOR_EPOCH_BLOCKS = 43200;

class ValidatorLifecycleRecord {
public:
    ValidatorLifecycleRecord();

    ValidatorLifecycleRecord(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint64_t epochIndex,
        std::string lifecycleStatus,
        utils::Amount lockedStake,
        utils::Amount earnedReward,
        utils::Amount slashingPenalty,
        std::uint16_t securityScore,
        std::string reason,
        std::string sourceDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epochIndex() const;
    const std::string& lifecycleStatus() const;
    utils::Amount lockedStake() const;
    utils::Amount earnedReward() const;
    utils::Amount slashingPenalty() const;
    std::uint16_t securityScore() const;
    const std::string& reason() const;
    const std::string& sourceDigest() const;

    bool active() const;
    bool jailed() const;
    bool slashed() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epochIndex;
    std::string m_lifecycleStatus;
    utils::Amount m_lockedStake;
    utils::Amount m_earnedReward;
    utils::Amount m_slashingPenalty;
    std::uint16_t m_securityScore;
    std::string m_reason;
    std::string m_sourceDigest;
};

class EpochAccountingRecord {
public:
    EpochAccountingRecord();

    EpochAccountingRecord(
        std::string status,
        std::uint64_t blockHeight,
        std::uint64_t epochIndex,
        std::uint64_t epochStartBlock,
        std::uint64_t epochEndBlock,
        std::uint64_t validatorCount,
        std::uint64_t activeValidatorCount,
        utils::Amount totalLockedStake,
        utils::Amount totalEarnedRewards,
        utils::Amount totalSlashingPenalties,
        std::string reason,
        std::string sourceLifecycleDigest
    );

    static EpochAccountingRecord notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epochIndex() const;
    std::uint64_t epochStartBlock() const;
    std::uint64_t epochEndBlock() const;
    std::uint64_t validatorCount() const;
    std::uint64_t activeValidatorCount() const;
    utils::Amount totalLockedStake() const;
    utils::Amount totalEarnedRewards() const;
    utils::Amount totalSlashingPenalties() const;
    const std::string& reason() const;
    const std::string& sourceLifecycleDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epochIndex;
    std::uint64_t m_epochStartBlock;
    std::uint64_t m_epochEndBlock;
    std::uint64_t m_validatorCount;
    std::uint64_t m_activeValidatorCount;
    utils::Amount m_totalLockedStake;
    utils::Amount m_totalEarnedRewards;
    utils::Amount m_totalSlashingPenalties;
    std::string m_reason;
    std::string m_sourceLifecycleDigest;
};

class ValidatorLifecycleSummary {
public:
    ValidatorLifecycleSummary();

    ValidatorLifecycleSummary(
        std::string status,
        std::uint64_t blockHeight,
        std::uint64_t epochIndex,
        std::uint64_t activeValidatorCount,
        std::uint64_t jailedValidatorCount,
        std::uint64_t slashedValidatorCount,
        utils::Amount totalLockedStake,
        utils::Amount totalEarnedRewards,
        utils::Amount totalSlashingPenalties,
        std::string reason,
        std::string sourceEpochDigest
    );

    static ValidatorLifecycleSummary notEvaluated();

    const std::string& status() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epochIndex() const;
    std::uint64_t activeValidatorCount() const;
    std::uint64_t jailedValidatorCount() const;
    std::uint64_t slashedValidatorCount() const;
    utils::Amount totalLockedStake() const;
    utils::Amount totalEarnedRewards() const;
    utils::Amount totalSlashingPenalties() const;
    const std::string& reason() const;
    const std::string& sourceEpochDigest() const;

    bool active() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epochIndex;
    std::uint64_t m_activeValidatorCount;
    std::uint64_t m_jailedValidatorCount;
    std::uint64_t m_slashedValidatorCount;
    utils::Amount m_totalLockedStake;
    utils::Amount m_totalEarnedRewards;
    utils::Amount m_totalSlashingPenalties;
    std::string m_reason;
    std::string m_sourceEpochDigest;
};

class ValidatorLifecycle {
public:
    static constexpr const char* ACTIVE_STATUS = "ACTIVE";
    static constexpr const char* JAILED_STATUS = "JAILED";
    static constexpr const char* SLASHED_STATUS = "SLASHED";

    static constexpr const char* LIFECYCLE_REASON =
        "VALIDATOR_LIFECYCLE_ACCOUNTING";

    static constexpr const char* EPOCH_ACCOUNTING_REASON =
        "VALIDATOR_EPOCH_ACCOUNTING";

    static constexpr const char* SUMMARY_REASON =
        "VALIDATOR_LIFECYCLE_SUMMARY";

    static constexpr const char* NOT_EVALUATED_REASON =
        "VALIDATOR_LIFECYCLE_NOT_EVALUATED";

    static std::uint64_t epochIndexForBlock(
        std::uint64_t blockHeight
    );

    static std::uint64_t epochStartBlock(
        std::uint64_t epochIndex
    );

    static std::uint64_t epochEndBlock(
        std::uint64_t epochIndex
    );

    static std::vector<ValidatorLifecycleRecord> buildLifecycleRecords(
        std::uint64_t blockHeight,
        const std::vector<RewardDistribution>& rewardDistributions,
        const std::vector<LockedStakePosition>& lockedStakePositions,
        const std::vector<SecurityScoreRecord>& securityScoreRecords,
        const std::vector<ProtectionRewardSettlement>& protectionRewardSettlements,
        const std::vector<StakePenaltyRecord>& stakePenaltyRecords
    );

    static EpochAccountingRecord buildEpochAccountingRecord(
        std::uint64_t blockHeight,
        const std::vector<ValidatorLifecycleRecord>& lifecycleRecords
    );

    static ValidatorLifecycleSummary buildSummary(
        std::uint64_t blockHeight,
        const std::vector<ValidatorLifecycleRecord>& lifecycleRecords,
        const EpochAccountingRecord& epochAccountingRecord
    );

    static bool sameLifecycleRecords(
        const std::vector<ValidatorLifecycleRecord>& left,
        const std::vector<ValidatorLifecycleRecord>& right
    );

    static bool sameEpochAccounting(
        const EpochAccountingRecord& left,
        const EpochAccountingRecord& right
    );

    static bool sameSummary(
        const ValidatorLifecycleSummary& left,
        const ValidatorLifecycleSummary& right
    );
};

} // namespace nodo::node

#endif
