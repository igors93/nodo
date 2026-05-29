#ifndef NODO_ECONOMICS_EPOCH_EMISSION_POLICY_HPP
#define NODO_ECONOMICS_EPOCH_EMISSION_POLICY_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * EpochEmissionPolicy controls how many new coins can be created in an epoch.
 *
 * The cap is dynamic, but it is still a cap.
 *
 * Useful work may decide how much of the cap is used, but work must not create
 * unlimited emission.
 */
class EpochEmissionPolicy {
public:
    static constexpr std::uint32_t BASIS_POINTS_DENOMINATOR = 10000;

    EpochEmissionPolicy();

    EpochEmissionPolicy(
        std::string policyVersion,
        std::uint32_t targetYearlyInflationBasisPoints,
        std::uint64_t epochsPerYear,
        utils::Amount zeroSupplyBootstrapCap
    );

    static EpochEmissionPolicy developmentDefaultPolicy();

    const std::string& policyVersion() const;
    std::uint32_t targetYearlyInflationBasisPoints() const;
    std::uint64_t epochsPerYear() const;
    utils::Amount zeroSupplyBootstrapCap() const;

    bool isValid() const;

    utils::Amount calculateNewEmissionCap(
        utils::Amount currentCirculatingSupply
    ) const;

    std::string serialize() const;

private:
    static std::int64_t multiplyDivideRawUnits(
        std::int64_t rawUnits,
        std::uint64_t numerator,
        std::uint64_t denominator
    );

    std::string m_policyVersion;
    std::uint32_t m_targetYearlyInflationBasisPoints;
    std::uint64_t m_epochsPerYear;
    utils::Amount m_zeroSupplyBootstrapCap;
};

} // namespace nodo::economics

#endif
