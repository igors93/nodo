#include "core/CoinLot.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

std::string coinLotStatusToString(CoinLotStatus status) {
    switch (status) {
        case CoinLotStatus::AVAILABLE:
            return "AVAILABLE";

        case CoinLotStatus::LOCKED_FOR_SECURITY:
            return "LOCKED_FOR_SECURITY";

        case CoinLotStatus::SPENT:
            return "SPENT";

        case CoinLotStatus::SLASHED:
            return "SLASHED";

        default:
            return "UNKNOWN";
    }
}

CoinLot::CoinLot(
    std::string id,
    std::string originMintRecordId,
    std::string ownerAddress,
    utils::Amount amount,
    CoinLotStatus status,
    std::uint64_t createdAtBlock,
    std::uint64_t lockedUntilBlock,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_originMintRecordId(std::move(originMintRecordId)),
      m_ownerAddress(std::move(ownerAddress)),
      m_amount(amount),
      m_status(status),
      m_createdAtBlock(createdAtBlock),
      m_lockedUntilBlock(lockedUntilBlock),
      m_timestamp(timestamp) {}

const std::string& CoinLot::id() const {
    return m_id;
}

const std::string& CoinLot::originMintRecordId() const {
    return m_originMintRecordId;
}

const std::string& CoinLot::ownerAddress() const {
    return m_ownerAddress;
}

utils::Amount CoinLot::amount() const {
    return m_amount;
}

CoinLotStatus CoinLot::status() const {
    return m_status;
}

std::uint64_t CoinLot::createdAtBlock() const {
    return m_createdAtBlock;
}

std::uint64_t CoinLot::lockedUntilBlock() const {
    return m_lockedUntilBlock;
}

std::int64_t CoinLot::timestamp() const {
    return m_timestamp;
}

bool CoinLot::isAvailable() const {
    return m_status == CoinLotStatus::AVAILABLE;
}

bool CoinLot::isLockedForSecurity() const {
    return m_status == CoinLotStatus::LOCKED_FOR_SECURITY;
}

bool CoinLot::isSpent() const {
    return m_status == CoinLotStatus::SPENT;
}

bool CoinLot::isSlashed() const {
    return m_status == CoinLotStatus::SLASHED;
}

bool CoinLot::isSpendable() const {
    return m_status == CoinLotStatus::AVAILABLE;
}

bool CoinLot::isValid() const {
    if (m_id.empty()) {
        return false;
    }

    if (m_originMintRecordId.empty()) {
        return false;
    }

    if (m_ownerAddress.empty()) {
        return false;
    }

    if (!m_amount.isPositive()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_status == CoinLotStatus::LOCKED_FOR_SECURITY &&
        m_lockedUntilBlock <= m_createdAtBlock) {
        return false;
    }

    return true;
}

void CoinLot::lockForSecurity(std::uint64_t lockedUntilBlock) {
    if (!isAvailable()) {
        throw std::logic_error("Only available coin lots can be locked.");
    }

    if (lockedUntilBlock <= m_createdAtBlock) {
        throw std::invalid_argument("Lock expiration block must be greater than creation block.");
    }

    m_status = CoinLotStatus::LOCKED_FOR_SECURITY;
    m_lockedUntilBlock = lockedUntilBlock;
}

void CoinLot::unlockIfMature(std::uint64_t currentBlock) {
    if (m_status != CoinLotStatus::LOCKED_FOR_SECURITY) {
        return;
    }

    if (currentBlock >= m_lockedUntilBlock) {
        m_status = CoinLotStatus::AVAILABLE;
        m_lockedUntilBlock = 0;
    }
}

void CoinLot::markSpent() {
    if (!isSpendable()) {
        throw std::logic_error("Only spendable coin lots can be marked as spent.");
    }

    m_status = CoinLotStatus::SPENT;
    m_lockedUntilBlock = 0;
}

void CoinLot::markSlashed() {
    if (isSpent()) {
        throw std::logic_error("Spent coin lots cannot be slashed.");
    }

    m_status = CoinLotStatus::SLASHED;
}

std::string CoinLot::serialize() const {
    std::ostringstream oss;

    oss << "CoinLot{"
        << "id=" << m_id
        << ";originMintRecordId=" << m_originMintRecordId
        << ";owner=" << m_ownerAddress
        << ";amountRaw=" << m_amount.rawUnits()
        << ";status=" << coinLotStatusToString(m_status)
        << ";createdAtBlock=" << m_createdAtBlock
        << ";lockedUntilBlock=" << m_lockedUntilBlock
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

} // namespace nodo::core