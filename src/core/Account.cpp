#include "core/Account.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

Account Account::create(
    std::string address,
    std::uint64_t createdAtBlock
) {
    return Account(
        std::move(address),
        1,
        createdAtBlock,
        createdAtBlock
    );
}

Account::Account(
    std::string address,
    std::uint64_t nextNonce,
    std::uint64_t createdAtBlock,
    std::uint64_t lastUpdatedAtBlock
)
    : m_address(std::move(address)),
      m_nextNonce(nextNonce),
      m_createdAtBlock(createdAtBlock),
      m_lastUpdatedAtBlock(lastUpdatedAtBlock) {}

const std::string& Account::address() const {
    return m_address;
}

std::uint64_t Account::nextNonce() const {
    return m_nextNonce;
}

std::uint64_t Account::createdAtBlock() const {
    return m_createdAtBlock;
}

std::uint64_t Account::lastUpdatedAtBlock() const {
    return m_lastUpdatedAtBlock;
}

bool Account::isValid() const {
    if (m_address.empty()) {
        return false;
    }

    if (m_nextNonce == 0) {
        return false;
    }

    if (m_lastUpdatedAtBlock < m_createdAtBlock) {
        return false;
    }

    return true;
}

bool Account::canApplyNonce(std::uint64_t nonce) const {
    if (nonce == 0) {
        return false;
    }

    return nonce == m_nextNonce;
}

void Account::markTransactionApplied(
    std::uint64_t nonce,
    std::uint64_t blockIndex
) {
    if (!canApplyNonce(nonce)) {
        throw std::logic_error("Invalid account nonce rejected.");
    }

    if (m_nextNonce == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Account nonce overflow.");
    }

    if (blockIndex < m_lastUpdatedAtBlock) {
        throw std::logic_error("Account update cannot move backwards in block history.");
    }

    ++m_nextNonce;
    m_lastUpdatedAtBlock = blockIndex;
}

std::string Account::serialize() const {
    std::ostringstream oss;

    oss << "Account{"
        << "address=" << m_address
        << ";nextNonce=" << m_nextNonce
        << ";createdAtBlock=" << m_createdAtBlock
        << ";lastUpdatedAtBlock=" << m_lastUpdatedAtBlock
        << "}";

    return oss.str();
}

} // namespace nodo::core