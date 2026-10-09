#include "economics/ProtectionWorkType.hpp"

#include <stdexcept>

namespace nodo::economics {

std::string protectionWorkTypeToString(ProtectionWorkType type) {
  switch (type) {
  case ProtectionWorkType::CONSENSUS_PROTECTION:
    return "CONSENSUS_PROTECTION";
  case ProtectionWorkType::FINALITY_PARTICIPATION:
    return "FINALITY_PARTICIPATION";
  case ProtectionWorkType::NETWORK_AVAILABILITY:
    return "NETWORK_AVAILABILITY";
  case ProtectionWorkType::DATA_AVAILABILITY:
    return "DATA_AVAILABILITY";
  case ProtectionWorkType::HISTORICAL_ARCHIVAL:
    return "HISTORICAL_ARCHIVAL";
  case ProtectionWorkType::USEFUL_COMPUTE:
    return "USEFUL_COMPUTE";
  }
  return "USEFUL_COMPUTE";
}

std::optional<ProtectionWorkType>
protectionWorkTypeFor(ValidationWorkType workType) {
  switch (workType) {
  case ValidationWorkType::VALIDATE_BLOCK:
  case ValidationWorkType::VALIDATE_TRANSACTION:
  case ValidationWorkType::VERIFY_COIN_EXISTENCE:
  case ValidationWorkType::VERIFY_SIGNATURE:
    return ProtectionWorkType::CONSENSUS_PROTECTION;
  case ValidationWorkType::CONSENSUS_VOTE:
    return ProtectionWorkType::FINALITY_PARTICIPATION;
  case ValidationWorkType::RESPOND_INTEGRITY_CHALLENGE:
    return ProtectionWorkType::DATA_AVAILABILITY;
  case ValidationWorkType::SERVE_HISTORICAL_BLOCK:
    // Serving is not yet provable (ADR 0014); preservation is measured by
    // archival proofs instead.
    return ProtectionWorkType::NETWORK_AVAILABILITY;
  case ValidationWorkType::UNKNOWN:
    return std::nullopt;
  }
  return std::nullopt;
}

ProtectionBudgetSplit::ProtectionBudgetSplit()
    : m_availabilityBasisPoints(0), m_dataProtectionBasisPoints(0),
      m_archivalBasisPoints(0) {}

ProtectionBudgetSplit::ProtectionBudgetSplit(
    std::uint32_t availabilityBasisPoints,
    std::uint32_t dataProtectionBasisPoints, std::uint32_t archivalBasisPoints)
    : m_availabilityBasisPoints(availabilityBasisPoints),
      m_dataProtectionBasisPoints(dataProtectionBasisPoints),
      m_archivalBasisPoints(archivalBasisPoints) {}

ProtectionBudgetSplit ProtectionBudgetSplit::consensusOnly() {
  return ProtectionBudgetSplit();
}

ProtectionBudgetSplit
ProtectionBudgetSplit::withArchival(std::uint32_t archivalBasisPoints) {
  return ProtectionBudgetSplit(0, 0, archivalBasisPoints);
}

std::uint32_t ProtectionBudgetSplit::consensusBasisPoints() const {
  const std::uint64_t others = static_cast<std::uint64_t>(m_availabilityBasisPoints) +
                               m_dataProtectionBasisPoints + m_archivalBasisPoints;
  return others >= kBasisPoints ? 0
                                : kBasisPoints - static_cast<std::uint32_t>(others);
}

std::uint32_t ProtectionBudgetSplit::availabilityBasisPoints() const {
  return m_availabilityBasisPoints;
}

std::uint32_t ProtectionBudgetSplit::dataProtectionBasisPoints() const {
  return m_dataProtectionBasisPoints;
}

std::uint32_t ProtectionBudgetSplit::archivalBasisPoints() const {
  return m_archivalBasisPoints;
}

bool ProtectionBudgetSplit::isValid() const {
  const std::uint64_t others = static_cast<std::uint64_t>(m_availabilityBasisPoints) +
                               m_dataProtectionBasisPoints + m_archivalBasisPoints;
  return others <= kBasisPoints &&
         consensusBasisPoints() >= kMinConsensusBasisPoints;
}

utils::Amount ProtectionBudgetSplit::slice(utils::Amount epochEmissionCap,
                                           ProtectionWorkType pillar) const {
  if (!isValid() || epochEmissionCap.isNegative()) {
    throw std::invalid_argument("Invalid protection budget split or cap.");
  }
  std::uint32_t basisPoints = 0;
  switch (pillar) {
  case ProtectionWorkType::CONSENSUS_PROTECTION:
  case ProtectionWorkType::FINALITY_PARTICIPATION:
    basisPoints = consensusBasisPoints();
    break;
  case ProtectionWorkType::NETWORK_AVAILABILITY:
    basisPoints = m_availabilityBasisPoints;
    break;
  case ProtectionWorkType::DATA_AVAILABILITY:
    basisPoints = m_dataProtectionBasisPoints;
    break;
  case ProtectionWorkType::HISTORICAL_ARCHIVAL:
    basisPoints = m_archivalBasisPoints;
    break;
  case ProtectionWorkType::USEFUL_COMPUTE:
    basisPoints = 0;
    break;
  }
  const unsigned __int128 scaled =
      static_cast<unsigned __int128>(epochEmissionCap.rawUnits()) * basisPoints /
      kBasisPoints;
  return utils::Amount::fromRawUnits(static_cast<std::int64_t>(scaled));
}

} // namespace nodo::economics
