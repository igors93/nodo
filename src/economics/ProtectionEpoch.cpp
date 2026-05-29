#include "economics/ProtectionEpoch.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>

namespace nodo::economics {

ProtectionEpoch::ProtectionEpoch()
    : m_epochId(0),
      m_startBlock(0),
      m_endBlock(0),
      m_feesCollected(utils::Amount::fromRawUnits(0)),
      m_emissionCap(utils::Amount::fromRawUnits(0)),
      m_workDemandBasisPoints(0) {}

ProtectionEpoch::ProtectionEpoch(
    std::uint64_t epochId,
    std::uint64_t startBlock,
    std::uint64_t endBlock,
    utils::Amount feesCollected,
    utils::Amount emissionCap,
    std::uint32_t workDemandBasisPoints
)
    : m_epochId(epochId),
      m_startBlock(startBlock),
      m_endBlock(endBlock),
      m_feesCollected(feesCollected),
      m_emissionCap(emissionCap),
      m_workDemandBasisPoints(workDemandBasisPoints) {}

std::uint64_t ProtectionEpoch::epochId() const {
    return m_epochId;
}

std::uint64_t ProtectionEpoch::startBlock() const {
    return m_startBlock;
}

std::uint64_t ProtectionEpoch::endBlock() const {
    return m_endBlock;
}

utils::Amount ProtectionEpoch::feesCollected() const {
    return m_feesCollected;
}

utils::Amount ProtectionEpoch::emissionCap() const {
    return m_emissionCap;
}

std::uint32_t ProtectionEpoch::workDemandBasisPoints() const {
    return m_workDemandBasisPoints;
}

bool ProtectionEpoch::isValid() const {
    if (m_epochId == 0) {
        return false;
    }

    if (m_endBlock < m_startBlock) {
        return false;
    }

    if (m_feesCollected.isNegative()) {
        return false;
    }

    if (m_emissionCap.isNegative()) {
        return false;
    }

    if (m_workDemandBasisPoints > BASIS_POINTS_DENOMINATOR) {
        return false;
    }

    return true;
}

utils::Amount ProtectionEpoch::securityEmission() const {
    if (!isValid()) {
        throw std::logic_error("Invalid protection epoch.");
    }

    return utils::Amount::fromRawUnits(
        multiplyDivideRawUnits(
            m_emissionCap.rawUnits(),
            m_workDemandBasisPoints,
            BASIS_POINTS_DENOMINATOR
        )
    );
}

utils::Amount ProtectionEpoch::rewardPool() const {
    return m_feesCollected + securityEmission();
}

std::string ProtectionEpoch::serialize() const {
    std::ostringstream oss;

    oss << "ProtectionEpoch{"
        << "epochId=" << m_epochId
        << ";startBlock=" << m_startBlock
        << ";endBlock=" << m_endBlock
        << ";feesCollectedRaw=" << m_feesCollected.rawUnits()
        << ";emissionCapRaw=" << m_emissionCap.rawUnits()
        << ";workDemandBasisPoints=" << m_workDemandBasisPoints
        << ";securityEmissionRaw=" << (isValid() ? securityEmission().rawUnits() : 0)
        << ";rewardPoolRaw=" << (isValid() ? rewardPool().rawUnits() : 0)
        << "}";

    return oss.str();
}

std::int64_t ProtectionEpoch::multiplyDivideRawUnits(
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
        throw std::overflow_error("Protection epoch emission calculation overflow.");
    }

    const std::uint64_t result =
        quotient * numerator +
        (remainder * numerator) / denominator;

    if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Protection epoch emission calculation overflow.");
    }

    return static_cast<std::int64_t>(result);
}

} // namespace nodo::economics
