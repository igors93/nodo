#ifndef NODO_ECONOMICS_EPOCH_REWARD_DISTRIBUTOR_HPP
#define NODO_ECONOMICS_EPOCH_REWARD_DISTRIBUTOR_HPP

#include "economics/EpochEmissionPolicy.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "economics/ProtectionEpoch.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "economics/ValidatorScoreRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * ValidatorRewardShare is the audited result for one validator inside an epoch.
 *
 * Security note:
 * The reward amount must be derived from accepted work and bounded score. It is
 * not allowed to be arbitrary user input.
 */
class ValidatorRewardShare {
public:
    ValidatorRewardShare();

    ValidatorRewardShare(
        std::string validatorAddress,
        std::uint64_t acceptedWorkWeight,
        std::uint32_t trustFactorBasisPoints,
        std::uint64_t effectiveRewardWeight,
        utils::Amount rewardAmount,
        GenesisRewardRecord rewardRecord
    );

    const std::string& validatorAddress() const;
    std::uint64_t acceptedWorkWeight() const;
    std::uint32_t trustFactorBasisPoints() const;
    std::uint64_t effectiveRewardWeight() const;
    utils::Amount rewardAmount() const;
    const GenesisRewardRecord& rewardRecord() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_acceptedWorkWeight;
    std::uint32_t m_trustFactorBasisPoints;
    std::uint64_t m_effectiveRewardWeight;
    utils::Amount m_rewardAmount;
    GenesisRewardRecord m_rewardRecord;
};

/*
 * EpochRewardDistribution is the deterministic output of reward calculation.
 *
 * It contains both the epoch summary and the GenesisReward records that should
 * be written into the ledger.
 */
class EpochRewardDistribution {
public:
    EpochRewardDistribution();

    EpochRewardDistribution(
        ProtectionEpoch protectionEpoch,
        std::string policyVersion,
        std::uint64_t targetWorkWeight,
        std::uint64_t totalAcceptedWorkWeight,
        std::uint64_t totalEffectiveRewardWeight,
        utils::Amount distributedSecurityEmission,
        std::vector<ValidatorRewardShare> validatorRewards
    );

    const ProtectionEpoch& protectionEpoch() const;
    const std::string& policyVersion() const;
    std::uint64_t targetWorkWeight() const;
    std::uint64_t totalAcceptedWorkWeight() const;
    std::uint64_t totalEffectiveRewardWeight() const;
    utils::Amount distributedSecurityEmission() const;
    const std::vector<ValidatorRewardShare>& validatorRewards() const;

    std::vector<GenesisRewardRecord> genesisRewardRecords() const;

    bool isValid() const;

    std::string serialize() const;

private:
    ProtectionEpoch m_protectionEpoch;
    std::string m_policyVersion;
    std::uint64_t m_targetWorkWeight;
    std::uint64_t m_totalAcceptedWorkWeight;
    std::uint64_t m_totalEffectiveRewardWeight;
    utils::Amount m_distributedSecurityEmission;
    std::vector<ValidatorRewardShare> m_validatorRewards;
};

/*
 * EpochRewardDistributor converts accepted validation work into GenesisReward
 * records.
 *
 * Important monetary rule:
 * Only securityEmission() is minted as new GenesisReward coins.
 * feesCollected are already existing coins and must not be minted again.
 */
class EpochRewardDistributor {
public:
    static EpochRewardDistribution distribute(
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
    );

private:
    static std::uint32_t calculateWorkDemandBasisPoints(
        std::uint64_t totalAcceptedWorkWeight,
        std::uint64_t targetWorkWeight
    );

    static std::uint64_t safeAdd(
        std::uint64_t left,
        std::uint64_t right,
        const std::string& context
    );

    static std::uint64_t safeMultiply(
        std::uint64_t left,
        std::uint64_t right,
        const std::string& context
    );

    static std::int64_t multiplyDivideRawUnits(
        std::int64_t rawUnits,
        std::uint64_t numerator,
        std::uint64_t denominator
    );

    static std::string buildWorkSummaryHash(
        std::uint64_t epochId,
        const std::string& validatorAddress,
        std::uint64_t acceptedWorkWeight,
        std::uint32_t trustFactorBasisPoints,
        std::uint64_t effectiveRewardWeight,
        const std::string& policyVersion
    );
};

} // namespace nodo::economics

#endif
