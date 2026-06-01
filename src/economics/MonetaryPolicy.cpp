#include "economics/MonetaryPolicy.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

MonetaryPolicy::MonetaryPolicy()
    : m_policyVersion(""),
      m_chainId(""),
      m_unitName(""),
      m_baseUnitName(""),
      m_baseUnitsPerUnit(0),
      m_initialSupply(utils::Amount::fromRawUnits(0)),
      m_maxAnnualInflationBasisPoints(0) {}

MonetaryPolicy::MonetaryPolicy(
    std::string policyVersion,
    std::string chainId,
    std::string unitName,
    std::string baseUnitName,
    std::uint64_t baseUnitsPerUnit,
    utils::Amount initialSupply,
    std::uint32_t maxAnnualInflationBasisPoints
)
    : m_policyVersion(std::move(policyVersion)),
      m_chainId(std::move(chainId)),
      m_unitName(std::move(unitName)),
      m_baseUnitName(std::move(baseUnitName)),
      m_baseUnitsPerUnit(baseUnitsPerUnit),
      m_initialSupply(initialSupply),
      m_maxAnnualInflationBasisPoints(maxAnnualInflationBasisPoints) {}

MonetaryPolicy MonetaryPolicy::localnetDefault(
    const std::string& chainId,
    utils::Amount initialSupply
) {
    return MonetaryPolicy(
        "NODO_MONETARY_POLICY_V1",
        chainId,
        "NODO",
        "raw",
        static_cast<std::uint64_t>(utils::Amount::UNITS_PER_NODO),
        initialSupply,
        400
    );
}

const std::string& MonetaryPolicy::policyVersion() const { return m_policyVersion; }
const std::string& MonetaryPolicy::chainId() const { return m_chainId; }
const std::string& MonetaryPolicy::unitName() const { return m_unitName; }
const std::string& MonetaryPolicy::baseUnitName() const { return m_baseUnitName; }
std::uint64_t MonetaryPolicy::baseUnitsPerUnit() const { return m_baseUnitsPerUnit; }
utils::Amount MonetaryPolicy::initialSupply() const { return m_initialSupply; }
std::uint32_t MonetaryPolicy::maxAnnualInflationBasisPoints() const {
    return m_maxAnnualInflationBasisPoints;
}

bool MonetaryPolicy::isValid() const {
    return rejectionReason().empty();
}

std::string MonetaryPolicy::rejectionReason() const {
    if (m_policyVersion.empty()) {
        return "MonetaryPolicy rejected: policyVersion is empty.";
    }
    if (m_chainId.empty()) {
        return "MonetaryPolicy rejected: chainId is empty.";
    }
    if (m_unitName != "NODO") {
        return "MonetaryPolicy rejected: unitName must be 'NODO', got '" +
               m_unitName + "'.";
    }
    if (m_baseUnitName.empty()) {
        return "MonetaryPolicy rejected: baseUnitName is empty.";
    }
    if (m_baseUnitsPerUnit == 0) {
        return "MonetaryPolicy rejected: baseUnitsPerUnit must be greater than zero.";
    }
    if (m_initialSupply.isNegative()) {
        return "MonetaryPolicy rejected: initialSupply is negative.";
    }
    if (m_maxAnnualInflationBasisPoints > MAX_ANNUAL_INFLATION_BASIS_POINTS) {
        return "MonetaryPolicy rejected: maxAnnualInflationBasisPoints " +
               std::to_string(m_maxAnnualInflationBasisPoints) +
               " exceeds protocol limit of " +
               std::to_string(MAX_ANNUAL_INFLATION_BASIS_POINTS) + ".";
    }
    return "";
}

std::string MonetaryPolicy::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryPolicy{"
        << "policyVersion=" << m_policyVersion
        << ";chainId=" << m_chainId
        << ";unitName=" << m_unitName
        << ";baseUnitName=" << m_baseUnitName
        << ";baseUnitsPerUnit=" << m_baseUnitsPerUnit
        << ";initialSupplyRaw=" << m_initialSupply.rawUnits()
        << ";maxAnnualInflationBasisPoints=" << m_maxAnnualInflationBasisPoints
        << "}";
    return oss.str();
}

} // namespace nodo::economics
