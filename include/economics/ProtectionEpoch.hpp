#ifndef NODO_ECONOMICS_PROTECTION_EPOCH_HPP
#define NODO_ECONOMICS_PROTECTION_EPOCH_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * ProtectionEpoch groups useful validator work and reward calculation.
 *
 * It does not create unlimited money. It only decides how much of the allowed
 * emission cap is used for the epoch.
 */
class ProtectionEpoch {
public:
    static constexpr std::uint32_t BASIS_POINTS_DENOMINATOR = 10000;

    ProtectionEpoch();

    ProtectionEpoch(
        std::uint64_t epochId,
        std::uint64_t startBlock,
        std::uint64_t endBlock,
        utils::Amount feesCollected,
        utils::Amount emissionCap,
        std::uint32_t workDemandBasisPoints
    );

    ProtectionEpoch(
        std::uint64_t epochId,
        std::uint64_t startBlock,
        std::uint64_t endBlock,
        utils::Amount feesCollected,
        utils::Amount emissionCap,
        std::uint32_t workDemandBasisPoints,
        std::uint64_t targetWorkWeight,
        std::uint64_t acceptedWorkWeight,
        std::string policyVersion,
        std::string evidenceBlockHash
    );

    std::uint64_t epochId() const;
    std::uint64_t startBlock() const;
    std::uint64_t endBlock() const;
    utils::Amount feesCollected() const;
    utils::Amount emissionCap() const;
    std::uint32_t workDemandBasisPoints() const;
    std::uint64_t targetWorkWeight() const;
    std::uint64_t acceptedWorkWeight() const;
    const std::string& policyVersion() const;
    const std::string& evidenceBlockHash() const;
    bool hasCanonicalSettlementMetadata() const;

    bool isValid() const;

    utils::Amount securityEmission() const;
    utils::Amount rewardPool() const;

    std::string serialize() const;

    static ProtectionEpoch deserialize(
        const std::string& serialized
    );

private:
    static std::int64_t multiplyDivideRawUnits(
        std::int64_t rawUnits,
        std::uint64_t numerator,
        std::uint64_t denominator
    );

    std::uint64_t m_epochId;
    std::uint64_t m_startBlock;
    std::uint64_t m_endBlock;
    utils::Amount m_feesCollected;
    utils::Amount m_emissionCap;
    std::uint32_t m_workDemandBasisPoints;
    std::uint64_t m_targetWorkWeight;
    std::uint64_t m_acceptedWorkWeight;
    std::string m_policyVersion;
    std::string m_evidenceBlockHash;
};

} // namespace nodo::economics

#endif
