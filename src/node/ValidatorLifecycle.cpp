#include "node/ValidatorLifecycle.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::uint16_t securityScoreForValidator(
    const std::string& validatorAddress,
    const std::vector<SecurityScoreRecord>& records
) {
    for (const SecurityScoreRecord& record : records) {
        if (record.validatorAddress() == validatorAddress) {
            return record.score();
        }
    }

    return SECURITY_SCORE_MIN;
}

utils::Amount lockedStakeForValidator(
    const std::string& validatorAddress,
    const std::vector<LockedStakePosition>& positions
) {
    utils::Amount total;

    for (const LockedStakePosition& position : positions) {
        if (position.ownerAddress() == validatorAddress) {
            total = total + position.amount();
        }
    }

    return total;
}

utils::Amount earnedRewardForValidator(
    const std::string& validatorAddress,
    const std::vector<ProtectionRewardSettlement>& settlements
) {
    utils::Amount total;

    for (const ProtectionRewardSettlement& settlement : settlements) {
        if (settlement.validatorAddress() == validatorAddress) {
            total = total + settlement.earnedReward();
        }
    }

    return total;
}

utils::Amount penaltyForValidator(
    const std::string& validatorAddress,
    const std::vector<StakePenaltyRecord>& penaltyRecords
) {
    utils::Amount total;

    for (const StakePenaltyRecord& penalty : penaltyRecords) {
        if (penalty.validatorAddress() == validatorAddress) {
            total = total + penalty.penaltyAmount();
        }
    }

    return total;
}

std::string lifecycleStatusFor(
    utils::Amount lockedStake,
    utils::Amount penaltyAmount,
    std::uint16_t securityScore
) {
    if (penaltyAmount.isPositive() && penaltyAmount >= lockedStake) {
        return ValidatorLifecycle::SLASHED_STATUS;
    }

    if (penaltyAmount.isPositive() || securityScore < 100) {
        return ValidatorLifecycle::JAILED_STATUS;
    }

    return ValidatorLifecycle::ACTIVE_STATUS;
}

std::string sourceDigestForValidator(
    const std::string& validatorAddress,
    const std::vector<LockedStakePosition>& positions,
    const std::vector<ProtectionRewardSettlement>& settlements,
    const std::vector<StakePenaltyRecord>& penalties
) {
    std::ostringstream digest;

    digest << "validator=" << validatorAddress;

    for (const LockedStakePosition& position : positions) {
        if (position.ownerAddress() == validatorAddress) {
            digest << "|locked=" << position.serialize();
        }
    }

    for (const ProtectionRewardSettlement& settlement : settlements) {
        if (settlement.validatorAddress() == validatorAddress) {
            digest << "|settlement=" << settlement.serialize();
        }
    }

    for (const StakePenaltyRecord& penalty : penalties) {
        if (penalty.validatorAddress() == validatorAddress) {
            digest << "|penalty=" << penalty.serialize();
        }
    }

    return digest.str();
}

utils::Amount totalLockedStake(
    const std::vector<ValidatorLifecycleRecord>& records
) {
    utils::Amount total;

    for (const ValidatorLifecycleRecord& record : records) {
        total = total + record.lockedStake();
    }

    return total;
}

utils::Amount totalEarnedRewards(
    const std::vector<ValidatorLifecycleRecord>& records
) {
    utils::Amount total;

    for (const ValidatorLifecycleRecord& record : records) {
        total = total + record.earnedReward();
    }

    return total;
}

utils::Amount totalSlashingPenalties(
    const std::vector<ValidatorLifecycleRecord>& records
) {
    utils::Amount total;

    for (const ValidatorLifecycleRecord& record : records) {
        total = total + record.slashingPenalty();
    }

    return total;
}

std::string lifecycleDigest(
    const std::vector<ValidatorLifecycleRecord>& records
) {
    std::ostringstream digest;

    if (records.empty()) {
        return "NO_VALIDATOR_LIFECYCLE_RECORDS";
    }

    for (const ValidatorLifecycleRecord& record : records) {
        digest << record.serialize();
    }

    return digest.str();
}

} // namespace

ValidatorLifecycleRecord::ValidatorLifecycleRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_epochIndex(0),
      m_lifecycleStatus(""),
      m_lockedStake(),
      m_earnedReward(),
      m_slashingPenalty(),
      m_securityScore(0),
      m_reason(""),
      m_sourceDigest("") {}

ValidatorLifecycleRecord::ValidatorLifecycleRecord(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_epochIndex(epochIndex),
      m_lifecycleStatus(std::move(lifecycleStatus)),
      m_lockedStake(lockedStake),
      m_earnedReward(earnedReward),
      m_slashingPenalty(slashingPenalty),
      m_securityScore(securityScore),
      m_reason(std::move(reason)),
      m_sourceDigest(std::move(sourceDigest)) {}

const std::string& ValidatorLifecycleRecord::validatorAddress() const { return m_validatorAddress; }
std::uint64_t ValidatorLifecycleRecord::blockHeight() const { return m_blockHeight; }
std::uint64_t ValidatorLifecycleRecord::epochIndex() const { return m_epochIndex; }
const std::string& ValidatorLifecycleRecord::lifecycleStatus() const { return m_lifecycleStatus; }
utils::Amount ValidatorLifecycleRecord::lockedStake() const { return m_lockedStake; }
utils::Amount ValidatorLifecycleRecord::earnedReward() const { return m_earnedReward; }
utils::Amount ValidatorLifecycleRecord::slashingPenalty() const { return m_slashingPenalty; }
std::uint16_t ValidatorLifecycleRecord::securityScore() const { return m_securityScore; }
const std::string& ValidatorLifecycleRecord::reason() const { return m_reason; }
const std::string& ValidatorLifecycleRecord::sourceDigest() const { return m_sourceDigest; }

bool ValidatorLifecycleRecord::active() const { return m_lifecycleStatus == ValidatorLifecycle::ACTIVE_STATUS; }
bool ValidatorLifecycleRecord::jailed() const { return m_lifecycleStatus == ValidatorLifecycle::JAILED_STATUS; }
bool ValidatorLifecycleRecord::slashed() const { return m_lifecycleStatus == ValidatorLifecycle::SLASHED_STATUS; }

bool ValidatorLifecycleRecord::isValid() const {
    return !m_validatorAddress.empty() &&
           m_blockHeight > 0 &&
           (active() || jailed() || slashed()) &&
           !m_lockedStake.isNegative() &&
           !m_earnedReward.isNegative() &&
           !m_slashingPenalty.isNegative() &&
           m_slashingPenalty <= m_lockedStake &&
           m_securityScore >= SECURITY_SCORE_MIN &&
           m_securityScore <= SECURITY_SCORE_MAX &&
           m_reason == ValidatorLifecycle::LIFECYCLE_REASON &&
           !m_sourceDigest.empty();
}

std::string ValidatorLifecycleRecord::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorLifecycleRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";epochIndex=" << m_epochIndex
        << ";lifecycleStatus=" << m_lifecycleStatus
        << ";lockedStakeRawUnits=" << m_lockedStake.rawUnits()
        << ";earnedRewardRawUnits=" << m_earnedReward.rawUnits()
        << ";slashingPenaltyRawUnits=" << m_slashingPenalty.rawUnits()
        << ";securityScore=" << m_securityScore
        << ";reason=" << m_reason
        << ";sourceDigest=" << m_sourceDigest
        << "}";

    return oss.str();
}

EpochAccountingRecord::EpochAccountingRecord()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_epochIndex(0),
      m_epochStartBlock(0),
      m_epochEndBlock(0),
      m_validatorCount(0),
      m_activeValidatorCount(0),
      m_totalLockedStake(),
      m_totalEarnedRewards(),
      m_totalSlashingPenalties(),
      m_reason(ValidatorLifecycle::NOT_EVALUATED_REASON),
      m_sourceLifecycleDigest("") {}

EpochAccountingRecord::EpochAccountingRecord(
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
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_epochIndex(epochIndex),
      m_epochStartBlock(epochStartBlock),
      m_epochEndBlock(epochEndBlock),
      m_validatorCount(validatorCount),
      m_activeValidatorCount(activeValidatorCount),
      m_totalLockedStake(totalLockedStake),
      m_totalEarnedRewards(totalEarnedRewards),
      m_totalSlashingPenalties(totalSlashingPenalties),
      m_reason(std::move(reason)),
      m_sourceLifecycleDigest(std::move(sourceLifecycleDigest)) {}

EpochAccountingRecord EpochAccountingRecord::notEvaluated() { return EpochAccountingRecord(); }
const std::string& EpochAccountingRecord::status() const { return m_status; }
std::uint64_t EpochAccountingRecord::blockHeight() const { return m_blockHeight; }
std::uint64_t EpochAccountingRecord::epochIndex() const { return m_epochIndex; }
std::uint64_t EpochAccountingRecord::epochStartBlock() const { return m_epochStartBlock; }
std::uint64_t EpochAccountingRecord::epochEndBlock() const { return m_epochEndBlock; }
std::uint64_t EpochAccountingRecord::validatorCount() const { return m_validatorCount; }
std::uint64_t EpochAccountingRecord::activeValidatorCount() const { return m_activeValidatorCount; }
utils::Amount EpochAccountingRecord::totalLockedStake() const { return m_totalLockedStake; }
utils::Amount EpochAccountingRecord::totalEarnedRewards() const { return m_totalEarnedRewards; }
utils::Amount EpochAccountingRecord::totalSlashingPenalties() const { return m_totalSlashingPenalties; }
const std::string& EpochAccountingRecord::reason() const { return m_reason; }
const std::string& EpochAccountingRecord::sourceLifecycleDigest() const { return m_sourceLifecycleDigest; }

bool EpochAccountingRecord::active() const { return m_status == "ACTIVE" && isValid(); }

bool EpochAccountingRecord::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_reason == ValidatorLifecycle::NOT_EVALUATED_REASON;
    }

    return m_status == "ACTIVE" &&
           m_blockHeight > 0 &&
           m_epochStartBlock > 0 &&
           m_epochEndBlock >= m_epochStartBlock &&
           m_blockHeight >= m_epochStartBlock &&
           m_blockHeight <= m_epochEndBlock &&
           m_activeValidatorCount <= m_validatorCount &&
           !m_totalLockedStake.isNegative() &&
           !m_totalEarnedRewards.isNegative() &&
           !m_totalSlashingPenalties.isNegative() &&
           m_reason == ValidatorLifecycle::EPOCH_ACCOUNTING_REASON &&
           !m_sourceLifecycleDigest.empty();
}

std::string EpochAccountingRecord::serialize() const {
    std::ostringstream oss;

    oss << "EpochAccountingRecord{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";epochIndex=" << m_epochIndex
        << ";epochStartBlock=" << m_epochStartBlock
        << ";epochEndBlock=" << m_epochEndBlock
        << ";validatorCount=" << m_validatorCount
        << ";activeValidatorCount=" << m_activeValidatorCount
        << ";totalLockedStakeRawUnits=" << m_totalLockedStake.rawUnits()
        << ";totalEarnedRewardsRawUnits=" << m_totalEarnedRewards.rawUnits()
        << ";totalSlashingPenaltiesRawUnits=" << m_totalSlashingPenalties.rawUnits()
        << ";reason=" << m_reason
        << ";sourceLifecycleDigest=" << m_sourceLifecycleDigest
        << "}";

    return oss.str();
}

ValidatorLifecycleSummary::ValidatorLifecycleSummary()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_epochIndex(0),
      m_activeValidatorCount(0),
      m_jailedValidatorCount(0),
      m_slashedValidatorCount(0),
      m_totalLockedStake(),
      m_totalEarnedRewards(),
      m_totalSlashingPenalties(),
      m_reason(ValidatorLifecycle::NOT_EVALUATED_REASON),
      m_sourceEpochDigest("") {}

ValidatorLifecycleSummary::ValidatorLifecycleSummary(
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
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_epochIndex(epochIndex),
      m_activeValidatorCount(activeValidatorCount),
      m_jailedValidatorCount(jailedValidatorCount),
      m_slashedValidatorCount(slashedValidatorCount),
      m_totalLockedStake(totalLockedStake),
      m_totalEarnedRewards(totalEarnedRewards),
      m_totalSlashingPenalties(totalSlashingPenalties),
      m_reason(std::move(reason)),
      m_sourceEpochDigest(std::move(sourceEpochDigest)) {}

ValidatorLifecycleSummary ValidatorLifecycleSummary::notEvaluated() { return ValidatorLifecycleSummary(); }
const std::string& ValidatorLifecycleSummary::status() const { return m_status; }
std::uint64_t ValidatorLifecycleSummary::blockHeight() const { return m_blockHeight; }
std::uint64_t ValidatorLifecycleSummary::epochIndex() const { return m_epochIndex; }
std::uint64_t ValidatorLifecycleSummary::activeValidatorCount() const { return m_activeValidatorCount; }
std::uint64_t ValidatorLifecycleSummary::jailedValidatorCount() const { return m_jailedValidatorCount; }
std::uint64_t ValidatorLifecycleSummary::slashedValidatorCount() const { return m_slashedValidatorCount; }
utils::Amount ValidatorLifecycleSummary::totalLockedStake() const { return m_totalLockedStake; }
utils::Amount ValidatorLifecycleSummary::totalEarnedRewards() const { return m_totalEarnedRewards; }
utils::Amount ValidatorLifecycleSummary::totalSlashingPenalties() const { return m_totalSlashingPenalties; }
const std::string& ValidatorLifecycleSummary::reason() const { return m_reason; }
const std::string& ValidatorLifecycleSummary::sourceEpochDigest() const { return m_sourceEpochDigest; }

bool ValidatorLifecycleSummary::active() const { return m_status == "ACTIVE" && isValid(); }

bool ValidatorLifecycleSummary::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_reason == ValidatorLifecycle::NOT_EVALUATED_REASON;
    }

    return m_status == "ACTIVE" &&
           m_blockHeight > 0 &&
           !m_totalLockedStake.isNegative() &&
           !m_totalEarnedRewards.isNegative() &&
           !m_totalSlashingPenalties.isNegative() &&
           m_reason == ValidatorLifecycle::SUMMARY_REASON &&
           !m_sourceEpochDigest.empty();
}

std::string ValidatorLifecycleSummary::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorLifecycleSummary{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";epochIndex=" << m_epochIndex
        << ";activeValidatorCount=" << m_activeValidatorCount
        << ";jailedValidatorCount=" << m_jailedValidatorCount
        << ";slashedValidatorCount=" << m_slashedValidatorCount
        << ";totalLockedStakeRawUnits=" << m_totalLockedStake.rawUnits()
        << ";totalEarnedRewardsRawUnits=" << m_totalEarnedRewards.rawUnits()
        << ";totalSlashingPenaltiesRawUnits=" << m_totalSlashingPenalties.rawUnits()
        << ";reason=" << m_reason
        << ";sourceEpochDigest=" << m_sourceEpochDigest
        << "}";

    return oss.str();
}

std::uint64_t ValidatorLifecycle::epochIndexForBlock(
    std::uint64_t blockHeight
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot calculate validator epoch for genesis height.");
    }

    return ((blockHeight - 1) / NODO_VALIDATOR_EPOCH_BLOCKS) + 1;
}

std::uint64_t ValidatorLifecycle::epochStartBlock(
    std::uint64_t epochIndex
) {
    if (epochIndex == 0) {
        throw std::invalid_argument("Validator epoch index must be positive.");
    }

    return ((epochIndex - 1) * NODO_VALIDATOR_EPOCH_BLOCKS) + 1;
}

std::uint64_t ValidatorLifecycle::epochEndBlock(
    std::uint64_t epochIndex
) {
    return epochStartBlock(epochIndex) + NODO_VALIDATOR_EPOCH_BLOCKS - 1;
}

std::vector<ValidatorLifecycleRecord> ValidatorLifecycle::buildLifecycleRecords(
    std::uint64_t blockHeight,
    const std::vector<RewardDistribution>& rewardDistributions,
    const std::vector<LockedStakePosition>& lockedStakePositions,
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<ProtectionRewardSettlement>& protectionRewardSettlements,
    const std::vector<StakePenaltyRecord>& stakePenaltyRecords
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build validator lifecycle records at genesis height.");
    }

    std::vector<ValidatorLifecycleRecord> records;
    records.reserve(rewardDistributions.size());

    const std::uint64_t epochIndex = epochIndexForBlock(blockHeight);

    for (const RewardDistribution& reward : rewardDistributions) {
        const std::string& validatorAddress = reward.validatorAddress();
        const utils::Amount lockedStake = lockedStakeForValidator(validatorAddress, lockedStakePositions);
        const utils::Amount earnedReward = earnedRewardForValidator(validatorAddress, protectionRewardSettlements);
        const utils::Amount slashingPenalty = penaltyForValidator(validatorAddress, stakePenaltyRecords);
        const std::uint16_t score = securityScoreForValidator(validatorAddress, securityScoreRecords);

        records.emplace_back(
            validatorAddress,
            blockHeight,
            epochIndex,
            lifecycleStatusFor(lockedStake, slashingPenalty, score),
            lockedStake,
            earnedReward,
            slashingPenalty,
            score,
            LIFECYCLE_REASON,
            sourceDigestForValidator(
                validatorAddress,
                lockedStakePositions,
                protectionRewardSettlements,
                stakePenaltyRecords
            )
        );
    }

    return records;
}

EpochAccountingRecord ValidatorLifecycle::buildEpochAccountingRecord(
    std::uint64_t blockHeight,
    const std::vector<ValidatorLifecycleRecord>& lifecycleRecords
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build epoch accounting at genesis height.");
    }

    std::uint64_t activeCount = 0;

    for (const ValidatorLifecycleRecord& record : lifecycleRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot build epoch accounting from invalid lifecycle record.");
        }

        if (record.active()) {
            ++activeCount;
        }
    }

    const std::uint64_t epochIndex = epochIndexForBlock(blockHeight);

    return EpochAccountingRecord(
        "ACTIVE",
        blockHeight,
        epochIndex,
        epochStartBlock(epochIndex),
        epochEndBlock(epochIndex),
        static_cast<std::uint64_t>(lifecycleRecords.size()),
        activeCount,
        totalLockedStake(lifecycleRecords),
        totalEarnedRewards(lifecycleRecords),
        totalSlashingPenalties(lifecycleRecords),
        EPOCH_ACCOUNTING_REASON,
        lifecycleDigest(lifecycleRecords)
    );
}

ValidatorLifecycleSummary ValidatorLifecycle::buildSummary(
    std::uint64_t blockHeight,
    const std::vector<ValidatorLifecycleRecord>& lifecycleRecords,
    const EpochAccountingRecord& epochAccountingRecord
) {
    if (!epochAccountingRecord.active()) {
        throw std::invalid_argument("Cannot build validator lifecycle summary from inactive epoch accounting.");
    }

    std::uint64_t activeCount = 0;
    std::uint64_t jailedCount = 0;
    std::uint64_t slashedCount = 0;

    for (const ValidatorLifecycleRecord& record : lifecycleRecords) {
        if (record.active()) {
            ++activeCount;
        } else if (record.jailed()) {
            ++jailedCount;
        } else if (record.slashed()) {
            ++slashedCount;
        }
    }

    return ValidatorLifecycleSummary(
        "ACTIVE",
        blockHeight,
        epochAccountingRecord.epochIndex(),
        activeCount,
        jailedCount,
        slashedCount,
        epochAccountingRecord.totalLockedStake(),
        epochAccountingRecord.totalEarnedRewards(),
        epochAccountingRecord.totalSlashingPenalties(),
        SUMMARY_REASON,
        epochAccountingRecord.serialize()
    );
}

bool ValidatorLifecycle::sameLifecycleRecords(
    const std::vector<ValidatorLifecycleRecord>& left,
    const std::vector<ValidatorLifecycleRecord>& right
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

bool ValidatorLifecycle::sameEpochAccounting(
    const EpochAccountingRecord& left,
    const EpochAccountingRecord& right
) {
    return left.serialize() == right.serialize();
}

bool ValidatorLifecycle::sameSummary(
    const ValidatorLifecycleSummary& left,
    const ValidatorLifecycleSummary& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
