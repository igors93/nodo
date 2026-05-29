#include "economics/EpochEmissionPolicy.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

EpochEmissionPolicy::EpochEmissionPolicy()
    : m_policyVersion(""),
      m_targetYearlyInflationBasisPoints(0),
      m_epochsPerYear(0),
      m_zeroSupplyBootstrapCap(utils::Amount::fromRawUnits(0)) {}

EpochEmissionPolicy::EpochEmissionPolicy(
    std::string policyVersion,
    std::uint32_t targetYearlyInflationBasisPoints,
    std::uint64_t epochsPerYear,
    utils::Amount zeroSupplyBootstrapCap
)
    : m_policyVersion(std::move(policyVersion)),
      m_targetYearlyInflationBasisPoints(targetYearlyInflationBasisPoints),
      m_epochsPerYear(epochsPerYear),
      m_zeroSupplyBootstrapCap(zeroSupplyBootstrapCap) {}

EpochEmissionPolicy EpochEmissionPolicy::developmentDefaultPolicy() {
    return EpochEmissionPolicy(
        "NODO_EPOCH_EMISSION_POLICY_V1",
        400,
        365,
        utils::Amount::fromNodo(100)
    );
}

const std::string& EpochEmissionPolicy::policyVersion() const {
    return m_policyVersion;
}

std::uint32_t EpochEmissionPolicy::targetYearlyInflationBasisPoints() const {
    return m_targetYearlyInflationBasisPoints;
}

std::uint64_t EpochEmissionPolicy::epochsPerYear() const {
    return m_epochsPerYear;
}

utils::Amount EpochEmissionPolicy::zeroSupplyBootstrapCap() const {
    return m_zeroSupplyBootstrapCap;
}

bool EpochEmissionPolicy::isValid() const {
    if (m_policyVersion.empty()) {
        return false;
    }

    if (m_epochsPerYear == 0) {
        return false;
    }

    if (m_targetYearlyInflationBasisPoints > BASIS_POINTS_DENOMINATOR) {
        return false;
    }

    if (m_zeroSupplyBootstrapCap.isNegative()) {
        return false;
    }

    return true;
}

utils::Amount EpochEmissionPolicy::calculateNewEmissionCap(
    utils::Amount currentCirculatingSupply
) const {
    if (!isValid()) {
        throw std::logic_error("Invalid epoch emission policy.");
    }

    if (currentCirculatingSupply.isNegative()) {
        throw std::invalid_argument("Circulating supply cannot be negative.");
    }

    if (currentCirculatingSupply.isZero()) {
        return m_zeroSupplyBootstrapCap;
    }

    const std::uint64_t denominator =
        static_cast<std::uint64_t>(BASIS_POINTS_DENOMINATOR) *
        m_epochsPerYear;

    return utils::Amount::fromRawUnits(
        multiplyDivideRawUnits(
            currentCirculatingSupply.rawUnits(),
            m_targetYearlyInflationBasisPoints,
            denominator
        )
    );
}

std::string EpochEmissionPolicy::serialize() const {
    std::ostringstream oss;

    oss << "EpochEmissionPolicy{"
        << "policyVersion=" << m_policyVersion
        << ";targetYearlyInflationBasisPoints=" << m_targetYearlyInflationBasisPoints
        << ";epochsPerYear=" << m_epochsPerYear
        << ";zeroSupplyBootstrapCapRaw=" << m_zeroSupplyBootstrapCap.rawUnits()
        << "}";

    return oss.str();
}

std::int64_t EpochEmissionPolicy::multiplyDivideRawUnits(
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

    const std::uint64_t raw =
        static_cast<std::uint64_t>(rawUnits);

    const std::uint64_t quotient =
        raw / denominator;

    const std::uint64_t remainder =
        raw % denominator;

    if (numerator != 0 &&
        quotient > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / numerator) {
        throw std::overflow_error("Emission cap calculation overflow.");
    }

    const std::uint64_t result =
        quotient * numerator +
        (remainder * numerator) / denominator;

    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Emission cap calculation overflow.");
    }

    return static_cast<std::int64_t>(result);
}

} // namespace nodo::economics
