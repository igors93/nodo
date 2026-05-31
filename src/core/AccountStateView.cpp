#include "core/AccountStateView.hpp"

#include <sstream>
#include <utility>

namespace nodo::core {

AccountStateView::AccountStateView()
    : m_accountsByAddress() {}

bool AccountStateView::hasAccount(
    const std::string& address
) const {
    if (address.empty()) {
        return false;
    }

    return m_accountsByAddress.find(address) != m_accountsByAddress.end();
}

AccountState AccountStateView::accountOrDefault(
    const std::string& address
) const {
    const auto found =
        m_accountsByAddress.find(address);

    if (found == m_accountsByAddress.end()) {
        return AccountState(
            address,
            utils::Amount(),
            0
        );
    }

    return found->second;
}

bool AccountStateView::putAccount(
    AccountState account
) {
    if (!account.isValid()) {
        return false;
    }

    m_accountsByAddress[account.address()] = std::move(account);
    return true;
}

std::vector<std::string> AccountStateView::knownAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, _] : m_accountsByAddress) {
        addresses.push_back(address);
    }

    return addresses;
}

std::vector<AccountState> AccountStateView::accounts() const {
    std::vector<AccountState> result;

    for (const auto& [_, account] : m_accountsByAddress) {
        result.push_back(account);
    }

    return result;
}

bool AccountStateView::isValid() const {
    for (const auto& [address, account] : m_accountsByAddress) {
        if (address.empty() ||
            address != account.address() ||
            !account.isValid()) {
            return false;
        }
    }

    return true;
}

std::string AccountStateView::serialize() const {
    std::ostringstream oss;

    oss << "AccountStateView{"
        << "accountCount=" << m_accountsByAddress.size()
        << ";accounts=[";

    bool first = true;

    for (const auto& [_, account] : m_accountsByAddress) {
        if (!first) {
            oss << ",";
        }

        oss << account.serialize();
        first = false;
    }

    oss << "]}";

    return oss.str();
}

} // namespace nodo::core
