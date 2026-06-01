#include "economics/BurnRecord.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

std::string burnTypeToString(BurnType burnType) {
    switch (burnType) {
        case BurnType::FEE_BURN:               return "FEE_BURN";
        case BurnType::SLASH_BURN:             return "SLASH_BURN";
        case BurnType::VOLUNTARY_BURN:         return "VOLUNTARY_BURN";
        case BurnType::GOVERNANCE_DEPOSIT_BURN: return "GOVERNANCE_DEPOSIT_BURN";
        case BurnType::PENALTY_BURN:           return "PENALTY_BURN";
        default:                               return "UNKNOWN";
    }
}

BurnType burnTypeFromString(const std::string& value) {
    if (value == "FEE_BURN")               return BurnType::FEE_BURN;
    if (value == "SLASH_BURN")             return BurnType::SLASH_BURN;
    if (value == "VOLUNTARY_BURN")         return BurnType::VOLUNTARY_BURN;
    if (value == "GOVERNANCE_DEPOSIT_BURN") return BurnType::GOVERNANCE_DEPOSIT_BURN;
    if (value == "PENALTY_BURN")           return BurnType::PENALTY_BURN;
    throw std::invalid_argument("Unknown BurnType: " + value);
}

BurnRecord::BurnRecord()
    : m_burnId(""),
      m_blockHeight(0),
      m_epoch(0),
      m_sourceAddress(""),
      m_amount(utils::Amount::fromRawUnits(0)),
      m_reason(""),
      m_burnType(BurnType::FEE_BURN) {}

BurnRecord::BurnRecord(
    std::string burnId,
    std::uint64_t blockHeight,
    std::uint64_t epoch,
    std::string sourceAddress,
    utils::Amount amount,
    std::string reason,
    BurnType burnType
)
    : m_burnId(std::move(burnId)),
      m_blockHeight(blockHeight),
      m_epoch(epoch),
      m_sourceAddress(std::move(sourceAddress)),
      m_amount(amount),
      m_reason(std::move(reason)),
      m_burnType(burnType) {}

const std::string& BurnRecord::burnId() const { return m_burnId; }
std::uint64_t BurnRecord::blockHeight() const { return m_blockHeight; }
std::uint64_t BurnRecord::epoch() const { return m_epoch; }
const std::string& BurnRecord::sourceAddress() const { return m_sourceAddress; }
utils::Amount BurnRecord::amount() const { return m_amount; }
const std::string& BurnRecord::reason() const { return m_reason; }
BurnType BurnRecord::burnType() const { return m_burnType; }

bool BurnRecord::isValid() const {
    return rejectionReason().empty();
}

std::string BurnRecord::rejectionReason() const {
    if (m_burnId.empty()) {
        return "BurnRecord rejected: burnId is empty.";
    }
    if (m_sourceAddress.empty()) {
        return "BurnRecord rejected: sourceAddress is empty.";
    }
    if (!m_amount.isPositive()) {
        return "BurnRecord rejected: amount must be positive, got " +
               std::to_string(m_amount.rawUnits()) + ".";
    }
    if (m_reason.empty()) {
        return "BurnRecord rejected: reason is empty.";
    }
    return "";
}

std::string BurnRecord::serialize() const {
    std::ostringstream oss;
    oss << "BurnRecord{"
        << "burnId=" << m_burnId
        << ";blockHeight=" << m_blockHeight
        << ";epoch=" << m_epoch
        << ";sourceAddress=" << m_sourceAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";reason=" << m_reason
        << ";burnType=" << burnTypeToString(m_burnType)
        << "}";
    return oss.str();
}

} // namespace nodo::economics
