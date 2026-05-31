#ifndef NODO_CORE_ACCOUNT_STATE_HPP
#define NODO_CORE_ACCOUNT_STATE_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {

class AccountState {
public:
    AccountState();

    AccountState(
        std::string address,
        utils::Amount balance,
        std::uint64_t nonce
    );

    const std::string& address() const;
    utils::Amount balance() const;
    std::uint64_t nonce() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_address;
    utils::Amount m_balance;
    std::uint64_t m_nonce;
};

} // namespace nodo::core

#endif
