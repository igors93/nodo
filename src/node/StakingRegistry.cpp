#include "node/StakingRegistry.hpp"

#include <sstream>

namespace nodo::node {

bool StakingRegistry::hasAccount(const std::string& validatorAddress) const {
    return m_accounts.count(validatorAddress) > 0;
}

const economics::StakeAccount* StakingRegistry::accountFor(
    const std::string& validatorAddress
) const {
    const auto it = m_accounts.find(validatorAddress);
    if (it == m_accounts.end()) return nullptr;
    return &it->second;
}

economics::StakeAccount StakingRegistry::accountOrDefault(
    const std::string& validatorAddress
) const {
    const auto* ptr = accountFor(validatorAddress);
    if (ptr != nullptr) return *ptr;
    return economics::StakeAccount(validatorAddress, utils::Amount());
}

void StakingRegistry::setAccount(
    const std::string& validatorAddress,
    economics::StakeAccount account
) {
    m_accounts[validatorAddress] = std::move(account);
}

const std::map<std::string, economics::StakeAccount>& StakingRegistry::accounts() const {
    return m_accounts;
}

std::size_t StakingRegistry::size() const {
    return m_accounts.size();
}

bool StakingRegistry::isValid() const {
    for (const auto& [addr, account] : m_accounts) {
        if (addr.empty() || !account.isValid()) return false;
    }
    return true;
}

std::string StakingRegistry::serialize() const {
    std::ostringstream oss;
    oss << "StakingRegistry{count=" << m_accounts.size() << ";accounts=[";
    bool first = true;
    for (const auto& [addr, account] : m_accounts) {
        if (!first) oss << ",";
        oss << account.serialize();
        first = false;
    }
    oss << "]}";
    return oss.str();
}

} // namespace nodo::node
