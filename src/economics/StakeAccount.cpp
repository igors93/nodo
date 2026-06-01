#include "economics/StakeAccount.hpp"

#include <sstream>
#include <stdexcept>

namespace nodo::economics {

StakeAccount::StakeAccount()
    : m_validatorAddress(""),
      m_bondedAmount(utils::Amount::fromRawUnits(0)),
      m_slashedAmount(utils::Amount::fromRawUnits(0)),
      m_jailed(false),
      m_tombstoned(false) {}

StakeAccount::StakeAccount(
    std::string validatorAddress,
    utils::Amount bondedAmount
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_bondedAmount(bondedAmount),
      m_slashedAmount(utils::Amount::fromRawUnits(0)),
      m_jailed(false),
      m_tombstoned(false) {}

const std::string& StakeAccount::validatorAddress() const { return m_validatorAddress; }
utils::Amount StakeAccount::bondedAmount() const { return m_bondedAmount; }
utils::Amount StakeAccount::slashedAmount() const { return m_slashedAmount; }
bool StakeAccount::jailed() const { return m_jailed; }
bool StakeAccount::tombstoned() const { return m_tombstoned; }

bool StakeAccount::isEligible() const {
    return !m_jailed && !m_tombstoned && m_bondedAmount > utils::Amount::fromRawUnits(0);
}

bool StakeAccount::canSlash(utils::Amount amount) const {
    if (amount.isNegative() || amount.isZero()) {
        return false;
    }
    const utils::Amount remaining = m_bondedAmount - m_slashedAmount;
    return amount <= remaining;
}

void StakeAccount::applySlash(utils::Amount amount) {
    if (amount.isNegative() || amount.isZero()) {
        throw std::invalid_argument("Slash amount must be positive.");
    }
    const utils::Amount remaining = m_bondedAmount - m_slashedAmount;
    if (amount > remaining) {
        throw std::invalid_argument(
            "Slash amount exceeds remaining stake for validator " + m_validatorAddress
        );
    }
    m_slashedAmount = m_slashedAmount + amount;
    // Auto-tombstone when fully slashed.
    if (m_slashedAmount >= m_bondedAmount) {
        m_tombstoned = true;
    }
}

void StakeAccount::jail() {
    m_jailed = true;
}

void StakeAccount::tombstone() {
    m_tombstoned = true;
    m_jailed = true;
}

void StakeAccount::unjail() {
    if (!m_tombstoned) {
        m_jailed = false;
    }
}

bool StakeAccount::isValid() const {
    return !m_validatorAddress.empty() &&
           !m_bondedAmount.isNegative() &&
           !m_slashedAmount.isNegative() &&
           m_slashedAmount <= m_bondedAmount;
}

std::string StakeAccount::serialize() const {
    std::ostringstream oss;
    oss << "StakeAccount{"
        << "address=" << m_validatorAddress
        << ";bonded=" << m_bondedAmount.rawUnits()
        << ";slashed=" << m_slashedAmount.rawUnits()
        << ";jailed=" << (m_jailed ? "1" : "0")
        << ";tombstoned=" << (m_tombstoned ? "1" : "0")
        << "}";
    return oss.str();
}

} // namespace nodo::economics
