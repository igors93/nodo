#include "core/AccountState.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

AccountState::AccountState()
    : m_address(""),
      m_balance(),
      m_nonce(0) {}

AccountState::AccountState(
    std::string address,
    utils::Amount balance,
    std::uint64_t nonce
)
    : m_address(std::move(address)),
      m_balance(balance),
      m_nonce(nonce) {}

const std::string& AccountState::address() const {
    return m_address;
}

utils::Amount AccountState::balance() const {
    return m_balance;
}

std::uint64_t AccountState::nonce() const {
    return m_nonce;
}

bool AccountState::isValid() const {
    return !m_address.empty() &&
           !m_balance.isNegative();
}

std::string AccountState::serialize() const {
    std::ostringstream oss;

    oss << "AccountState{"
        << "address=" << m_address
        << ";balanceRaw=" << m_balance.rawUnits()
        << ";nonce=" << m_nonce
        << "}";

    return oss.str();
}

} // namespace nodo::core
