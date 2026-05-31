#ifndef NODO_CORE_ACCOUNT_STATE_VIEW_HPP
#define NODO_CORE_ACCOUNT_STATE_VIEW_HPP

#include "core/AccountState.hpp"

#include <map>
#include <string>
#include <vector>

namespace nodo::core {

class AccountStateView {
public:
    AccountStateView();

    bool hasAccount(
        const std::string& address
    ) const;

    AccountState accountOrDefault(
        const std::string& address
    ) const;

    bool putAccount(
        AccountState account
    );

    std::vector<std::string> knownAddresses() const;
    std::vector<AccountState> accounts() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::map<std::string, AccountState> m_accountsByAddress;
};

} // namespace nodo::core

#endif
