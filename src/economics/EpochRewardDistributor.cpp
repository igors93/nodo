#include "economics/EpochRewardDistributor.hpp"

#include "crypto/hash.h"

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

namespace {

struct ValidatorWorkSummary {
    std::uint64_t acceptedWorkWeight = 0;
};

struct ValidatorScoreSummary {
    bool hasScore = false;
    std::uint64_t epoch = 0;
    std::int64_t timestamp = 0;
    std::int32_t score = 0;
};

struct CandidateReward {
    std::string validatorAddress;
    std::uint64_t acceptedWorkWeight = 0;
    std::uint32_t trustFactorBasisPoints = 0;
    std::uint64_t effectiveRewardWeight = 0;
    utils::Amount rewardAmount = utils::Amount::fromRawUnits(0);
};

bool shouldReplaceScore(
    const ValidatorScoreSummary& current,
    const ValidatorScoreRecord& candidate
) {
    if (!current.hasScore) {
        return true;
    }

    if (candidate.epoch() > current.epoch) {
        return true;
    }

    if (candidate.epoch() == current.epoch &&
        candidate.timestamp() > current.timestamp) {
        return true;
    }

    return false;
}

std::uint64_t safeAddForDistributionAudit(
    std::uint64_t left,
    std::uint64_t right,
    const std::string& context
) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("Unsigned addition overflow in " + context + ".");
    }

    return left + right;
}

} // namespace

ValidatorRewardShare::ValidatorRewardShare()
    : m_validatorAddress(""),
      m_acceptedWorkWeight(0),
      m_trustFactorBasisPoints(0),
      m_effectiveRewardWeight(0),
      m_rewardAmount(utils::Amount::fromRawUnits(0)),
      m_rewardRecord() {}

ValidatorRewardShare::ValidatorRewardShare(
    std::string validatorAddress,
    std::uint64_t acceptedWorkWeight,
    std::uint32_t trustFactorBasisPoints,
    std::uint64_t effectiveRewardWeight,
    utils::Amount rewardAmount,
    GenesisRewardRecord rewardRecord
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_acceptedWorkWeight(acceptedWorkWeight),
      m_trustFactorBasisPoints(trustFactorBasisPoints),
      m_effectiveRewardWeight(effectiveRewardWeight),
      m_rewardAmount(rewardAmount),
      m_rewardRecord(std::move(rewardRecord)) {}

const std::string& ValidatorRewardShare::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorRewardShare::acceptedWorkWeight() const {
    return m_acceptedWorkWeight;
}

std::uint32_t ValidatorRewardShare::trustFactorBasisPoints() const {
    return m_trustFactorBasisPoints;
}

std::uint64_t ValidatorRewardShare::effectiveRewardWeight() const {
    return m_effectiveRewardWeight;
}

utils::Amount ValidatorRewardShare::rewardAmount() const {
    return m_rewardAmount;
}

const GenesisRewardRecord& ValidatorRewardShare::rewardRecord() const {
    return m_rewardRecord;
}

bool ValidatorRewardShare::isValid() const {
    if (m_validatorAddress.empty()) {
        return false;
    }

    if (m_acceptedWorkWeight == 0) {
        return false;
    }

    if (m_trustFactorBasisPoints > ProtectionEpoch::BASIS_POINTS_DENOMINATOR) {
        return false;
    }

    if (m_effectiveRewardWeight == 0) {
        return false;
    }

    if (!m_rewardAmount.isPositive()) {
        return false;
    }

    if (!m_rewardRecord.isValid()) {
        return false;
    }

    if (m_rewardRecord.validatorAddress() != m_validatorAddress) {
        return false;
    }

    if (m_rewardRecord.amount() != m_rewardAmount) {
        return false;
    }

    return true;
}

std::string ValidatorRewardShare::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRewardShare{"
        << "validator=" << m_validatorAddress
        << ";acceptedWorkWeight=" << m_acceptedWorkWeight
        << ";trustFactorBasisPoints=" << m_trustFactorBasisPoints
        << ";effectiveRewardWeight=" << m_effectiveRewardWeight
        << ";rewardRaw=" << m_rewardAmount.rawUnits()
        << ";rewardId=" << (m_rewardRecord.isValid() ? m_rewardRecord.deterministicId() : "INVALID")
        << "}";

    return oss.str();
}

EpochRewardDistribution::EpochRewardDistribution()
    : m_protectionEpoch(),
      m_policyVersion(""),
      m_targetWorkWeight(0),
      m_totalAcceptedWorkWeight(0),
      m_totalEffectiveRewardWeight(0),
      m_distributedSecurityEmission(utils::Amount::fromRawUnits(0)),
      m_validatorRewards() {}

EpochRewardDistribution::EpochRewardDistribution(
    ProtectionEpoch protectionEpoch,
    std::string policyVersion,
    std::uint64_t targetWorkWeight,
    std::uint64_t totalAcceptedWorkWeight,
    std::uint64_t totalEffectiveRewardWeight,
    utils::Amount distributedSecurityEmission,
    std::vector<ValidatorRewardShare> validatorRewards
)
    : m_protectionEpoch(std::move(protectionEpoch)),
      m_policyVersion(std::move(policyVersion)),
      m_targetWorkWeight(targetWorkWeight),
      m_totalAcceptedWorkWeight(totalAcceptedWorkWeight),
      m_totalEffectiveRewardWeight(totalEffectiveRewardWeight),
      m_distributedSecurityEmission(distributedSecurityEmission),
      m_validatorRewards(std::move(validatorRewards)) {}

const ProtectionEpoch& EpochRewardDistribution::protectionEpoch() const {
    return m_protectionEpoch;
}

const std::string& EpochRewardDistribution::policyVersion() const {
    return m_policyVersion;
}

std::uint64_t EpochRewardDistribution::targetWorkWeight() const {
    return m_targetWorkWeight;
}

std::uint64_t EpochRewardDistribution::totalAcceptedWorkWeight() const {
    return m_totalAcceptedWorkWeight;
}

std::uint64_t EpochRewardDistribution::totalEffectiveRewardWeight() const {
    return m_totalEffectiveRewardWeight;
}

utils::Amount EpochRewardDistribution::distributedSecurityEmission() const {
    return m_distributedSecurityEmission;
}

const std::vector<ValidatorRewardShare>& EpochRewardDistribution::validatorRewards() const {
    return m_validatorRewards;
}

std::vector<GenesisRewardRecord> EpochRewardDistribution::genesisRewardRecords() const {
    std::vector<GenesisRewardRecord> records;

    for (const auto& reward : m_validatorRewards) {
        records.push_back(reward.rewardRecord());
    }

    return records;
}

/**
 * Validates the reward distribution structure for the epoch.
 * Ensures that the amounts sum up correctly and that there are no negative values or 
 * structural mismatches within the validator and treasury allocations.
 */
bool EpochRewardDistribution::isValid() const {
    if (!m_protectionEpoch.isValid()) {
        return false;
    }

    if (m_policyVersion.empty()) {
        return false;
    }

    if (m_targetWorkWeight == 0) {
        return false;
    }

    if (m_distributedSecurityEmission.isNegative()) {
        return false;
    }

    if (m_distributedSecurityEmission > m_protectionEpoch.securityEmission()) {
        return false;
    }

    std::uint64_t recalculatedAcceptedWork = 0;
    std::uint64_t recalculatedEffectiveWeight = 0;
    utils::Amount recalculatedRewards = utils::Amount::fromRawUnits(0);
    std::set<std::string> rewardIds;

    for (const auto& reward : m_validatorRewards) {
        if (!reward.isValid()) {
            return false;
        }

        if (reward.rewardRecord().epoch() != m_protectionEpoch.epochId()) {
            return false;
        }

        if (reward.rewardRecord().policyVersion() != m_policyVersion) {
            return false;
        }

        const std::string rewardId =
            reward.rewardRecord().deterministicId();

        if (!rewardIds.insert(rewardId).second) {
            return false;
        }

        recalculatedAcceptedWork =
            safeAddForDistributionAudit(
                recalculatedAcceptedWork,
                reward.acceptedWorkWeight(),
                "EpochRewardDistribution accepted work audit"
            );

        recalculatedEffectiveWeight =
            safeAddForDistributionAudit(
                recalculatedEffectiveWeight,
                reward.effectiveRewardWeight(),
                "EpochRewardDistribution effective weight audit"
            );

        recalculatedRewards = recalculatedRewards + reward.rewardAmount();
    }

    if (recalculatedAcceptedWork > m_totalAcceptedWorkWeight) {
        return false;
    }

    if (recalculatedEffectiveWeight != m_totalEffectiveRewardWeight) {
        return false;
    }

    if (recalculatedRewards != m_distributedSecurityEmission) {
        return false;
    }

    if (m_validatorRewards.empty() &&
        (!m_distributedSecurityEmission.isZero() || m_totalEffectiveRewardWeight != 0)) {
        return false;
    }

    return true;
}

std::string EpochRewardDistribution::serialize() const {
    std::ostringstream oss;

    oss << "EpochRewardDistribution{"
        << "epochId=" << m_protectionEpoch.epochId()
        << ";policyVersion=" << m_policyVersion
        << ";targetWorkWeight=" << m_targetWorkWeight
        << ";totalAcceptedWorkWeight=" << m_totalAcceptedWorkWeight
        << ";totalEffectiveRewardWeight=" << m_totalEffectiveRewardWeight
        << ";securityEmissionRaw=" << m_protectionEpoch.securityEmission().rawUnits()
        << ";distributedSecurityEmissionRaw=" << m_distributedSecurityEmission.rawUnits()
        << ";rewardCount=" << m_validatorRewards.size()
        << "}";

    return oss.str();
}

EpochRewardDistribution EpochRewardDistributor::distribute(
    std::uint64_t epochId,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    utils::Amount currentCirculatingSupply,
    utils::Amount feesCollected,
    std::uint64_t targetWorkWeight,
    const EpochEmissionPolicy& policy,
    const std::vector<ValidationWorkRecord>& workRecords,
    const std::vector<ValidatorScoreRecord>& scoreRecords,
    const std::string& acceptedBlockHash,
    std::int64_t timestamp
) {
    if (epochId == 0) {
        throw std::invalid_argument("Epoch id must be positive.");
    }

    if (endBlock < startBlock) {
        throw std::invalid_argument("Epoch end block cannot be lower than start block.");
    }

    if (currentCirculatingSupply.isNegative()) {
        throw std::invalid_argument("Current circulating supply cannot be negative.");
    }

    if (feesCollected.isNegative()) {
        throw std::invalid_argument("Collected fees cannot be negative.");
    }

    if (targetWorkWeight == 0) {
        throw std::invalid_argument("Target work weight must be positive.");
    }

    if (!policy.isValid()) {
        throw std::invalid_argument("Invalid epoch emission policy.");
    }

    if (acceptedBlockHash.empty()) {
        throw std::invalid_argument("Accepted block hash cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Reward timestamp must be positive.");
    }

    std::map<std::string, ValidatorWorkSummary> workByValidator;
    std::set<std::string> acceptedEvidenceHashes;
    std::uint64_t totalAcceptedWorkWeight = 0;

    for (const auto& workRecord : workRecords) {
        if (!workRecord.isValid()) {
            throw std::invalid_argument("Invalid ValidationWorkRecord rejected by reward distributor.");
        }

        if (workRecord.epoch() != epochId) {
            continue;
        }

        if (!workRecord.contributesToReward()) {
            continue;
        }

        if (!acceptedEvidenceHashes.insert(workRecord.evidenceHash()).second) {
            throw std::invalid_argument("Duplicate accepted work evidence rejected.");
        }

        ValidatorWorkSummary& summary =
            workByValidator[workRecord.validatorAddress()];

        summary.acceptedWorkWeight =
            safeAdd(
                summary.acceptedWorkWeight,
                workRecord.workWeight(),
                "validator accepted work"
            );

        totalAcceptedWorkWeight =
            safeAdd(
                totalAcceptedWorkWeight,
                workRecord.workWeight(),
                "total accepted work"
            );
    }

    std::map<std::string, ValidatorScoreSummary> scoreByValidator;

    for (const auto& scoreRecord : scoreRecords) {
        if (!scoreRecord.isValid()) {
            throw std::invalid_argument("Invalid ValidatorScoreRecord rejected by reward distributor.");
        }

        if (scoreRecord.epoch() > epochId) {
            continue;
        }

        ValidatorScoreSummary& summary =
            scoreByValidator[scoreRecord.validatorAddress()];

        if (shouldReplaceScore(summary, scoreRecord)) {
            summary.hasScore = true;
            summary.epoch = scoreRecord.epoch();
            summary.timestamp = scoreRecord.timestamp();
            summary.score = scoreRecord.newScore();
        }
    }

    const std::uint32_t workDemandBasisPoints =
        calculateWorkDemandBasisPoints(
            totalAcceptedWorkWeight,
            targetWorkWeight
        );

    const utils::Amount emissionCap =
        policy.calculateNewEmissionCap(
            currentCirculatingSupply
        );

    const ProtectionEpoch protectionEpoch(
        epochId,
        startBlock,
        endBlock,
        feesCollected,
        emissionCap,
        workDemandBasisPoints,
        targetWorkWeight,
        totalAcceptedWorkWeight,
        policy.policyVersion(),
        acceptedBlockHash
    );

    if (!protectionEpoch.isValid()) {
        throw std::logic_error("Reward distributor created an invalid ProtectionEpoch.");
    }

    /*
     * Monetary safety:
     * GenesisReward records mint only securityEmission.
     * feesCollected are existing coins and must not be minted again.
     */
    const utils::Amount securityEmission =
        protectionEpoch.securityEmission();

    std::vector<CandidateReward> candidates;
    std::uint64_t totalEffectiveRewardWeight = 0;

    for (const auto& entry : workByValidator) {
        const std::string& validatorAddress =
            entry.first;

        const std::uint64_t acceptedWorkWeight =
            entry.second.acceptedWorkWeight;

        const auto scoreIt =
            scoreByValidator.find(validatorAddress);

        if (scoreIt == scoreByValidator.end() || !scoreIt->second.hasScore) {
            /*
             * Conservative default:
             * A validator with no on-chain score does not receive minted
             * security rewards. Registration/bootstrap can create score records.
             */
            continue;
        }

        const std::uint32_t trustFactorBasisPoints =
            static_cast<std::uint32_t>(
                scoreIt->second.score * 100
            );

        if (trustFactorBasisPoints == 0) {
            continue;
        }

        const std::uint64_t effectiveRewardWeight =
            safeMultiply(
                acceptedWorkWeight,
                trustFactorBasisPoints,
                "validator effective reward weight"
            );

        totalEffectiveRewardWeight =
            safeAdd(
                totalEffectiveRewardWeight,
                effectiveRewardWeight,
                "total effective reward weight"
            );

        CandidateReward candidate;
        candidate.validatorAddress = validatorAddress;
        candidate.acceptedWorkWeight = acceptedWorkWeight;
        candidate.trustFactorBasisPoints = trustFactorBasisPoints;
        candidate.effectiveRewardWeight = effectiveRewardWeight;

        candidates.push_back(candidate);
    }

    if (securityEmission.isZero() ||
        candidates.empty() ||
        totalEffectiveRewardWeight == 0) {
        EpochRewardDistribution distribution(
            protectionEpoch,
            policy.policyVersion(),
            targetWorkWeight,
            totalAcceptedWorkWeight,
            0,
            utils::Amount::fromRawUnits(0),
            {}
        );

        if (!distribution.isValid()) {
            throw std::logic_error("Empty reward distribution is invalid.");
        }

        return distribution;
    }

    utils::Amount distributedSecurityEmission =
        utils::Amount::fromRawUnits(0);

    for (auto& candidate : candidates) {
        candidate.rewardAmount =
            utils::Amount::fromRawUnits(
                multiplyDivideRawUnits(
                    securityEmission.rawUnits(),
                    candidate.effectiveRewardWeight,
                    totalEffectiveRewardWeight
                )
            );

        distributedSecurityEmission =
            distributedSecurityEmission + candidate.rewardAmount;
    }

    std::int64_t remainderRaw =
        securityEmission.rawUnits() - distributedSecurityEmission.rawUnits();

    /*
     * Remainder is deterministic and should be smaller than candidate count.
     * Give one raw unit to validators in address order until exhausted.
     */
    for (auto& candidate : candidates) {
        if (remainderRaw <= 0) {
            break;
        }

        candidate.rewardAmount =
            candidate.rewardAmount + utils::Amount::fromRawUnits(1);

        --remainderRaw;
    }

    std::vector<ValidatorRewardShare> rewards;
    distributedSecurityEmission = utils::Amount::fromRawUnits(0);

    for (const auto& candidate : candidates) {
        if (!candidate.rewardAmount.isPositive()) {
            continue;
        }

        GenesisRewardRecord rewardRecord(
            epochId,
            candidate.validatorAddress,
            candidate.rewardAmount,
            GenesisRewardReason::NETWORK_PROTECTION,
            buildWorkSummaryHash(
                epochId,
                candidate.validatorAddress,
                candidate.acceptedWorkWeight,
                candidate.trustFactorBasisPoints,
                candidate.effectiveRewardWeight,
                policy.policyVersion()
            ),
            policy.policyVersion(),
            acceptedBlockHash,
            timestamp
        );

        if (!rewardRecord.isValid()) {
            throw std::logic_error("Reward distributor created an invalid GenesisRewardRecord.");
        }

        rewards.emplace_back(
            candidate.validatorAddress,
            candidate.acceptedWorkWeight,
            candidate.trustFactorBasisPoints,
            candidate.effectiveRewardWeight,
            candidate.rewardAmount,
            rewardRecord
        );

        distributedSecurityEmission =
            distributedSecurityEmission + candidate.rewardAmount;
    }

    if (distributedSecurityEmission > securityEmission) {
        throw std::logic_error("Reward distributor exceeded security emission cap.");
    }

    EpochRewardDistribution distribution(
        protectionEpoch,
        policy.policyVersion(),
        targetWorkWeight,
        totalAcceptedWorkWeight,
        totalEffectiveRewardWeight,
        distributedSecurityEmission,
        rewards
    );

    if (!distribution.isValid()) {
        throw std::logic_error("Reward distributor created an invalid distribution.");
    }

    return distribution;
}

std::uint32_t EpochRewardDistributor::calculateWorkDemandBasisPoints(
    std::uint64_t totalAcceptedWorkWeight,
    std::uint64_t targetWorkWeight
) {
    if (targetWorkWeight == 0) {
        throw std::invalid_argument("Target work weight must be positive.");
    }

    const unsigned __int128 numerator =
        static_cast<unsigned __int128>(totalAcceptedWorkWeight) *
        ProtectionEpoch::BASIS_POINTS_DENOMINATOR;

    const unsigned __int128 value =
        numerator / targetWorkWeight;

    if (value >= ProtectionEpoch::BASIS_POINTS_DENOMINATOR) {
        return ProtectionEpoch::BASIS_POINTS_DENOMINATOR;
    }

    return static_cast<std::uint32_t>(value);
}

std::uint64_t EpochRewardDistributor::safeAdd(
    std::uint64_t left,
    std::uint64_t right,
    const std::string& context
) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("Unsigned addition overflow in " + context + ".");
    }

    return left + right;
}

std::uint64_t EpochRewardDistributor::safeMultiply(
    std::uint64_t left,
    std::uint64_t right,
    const std::string& context
) {
    if (left != 0 &&
        right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("Unsigned multiplication overflow in " + context + ".");
    }

    return left * right;
}

std::int64_t EpochRewardDistributor::multiplyDivideRawUnits(
    std::int64_t rawUnits,
    std::uint64_t numerator,
    std::uint64_t denominator
) {
    if (rawUnits < 0) {
        throw std::invalid_argument("Raw units cannot be negative.");
    }

    if (denominator == 0) {
        throw std::invalid_argument("Denominator cannot be zero.");
    }

    if (numerator > denominator) {
        throw std::invalid_argument("Reward numerator cannot exceed denominator.");
    }

    const unsigned __int128 result =
        (static_cast<unsigned __int128>(rawUnits) * numerator) / denominator;

    if (result > static_cast<unsigned __int128>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Reward amount calculation overflow.");
    }

    return static_cast<std::int64_t>(result);
}

std::string EpochRewardDistributor::buildWorkSummaryHash(
    std::uint64_t epochId,
    const std::string& validatorAddress,
    std::uint64_t acceptedWorkWeight,
    std::uint32_t trustFactorBasisPoints,
    std::uint64_t effectiveRewardWeight,
    const std::string& policyVersion
) {
    std::ostringstream payload;

    payload << "EpochRewardWorkSummary{"
            << "epochId=" << epochId
            << ";validator=" << validatorAddress
            << ";acceptedWorkWeight=" << acceptedWorkWeight
            << ";trustFactorBasisPoints=" << trustFactorBasisPoints
            << ";effectiveRewardWeight=" << effectiveRewardWeight
            << ";policyVersion=" << policyVersion
            << "}";

    char output[NODO_HASH_BUFFER_SIZE] = {0};

    nodo_hash_string(
        payload.str().c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

} // namespace nodo::economics
