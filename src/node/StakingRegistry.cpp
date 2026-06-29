#include "node/StakingRegistry.hpp"

#include <sstream>
#include <stdexcept>
#include <limits>

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
    if (validatorAddress.empty() || !account.isValid() ||
        account.validatorAddress() != validatorAddress) {
        throw std::invalid_argument("Staking registry key does not match a valid stake account.");
    }
    m_accounts[validatorAddress] = std::move(account);
}

void StakingRegistry::deposit(
    const std::string& ownerAddress,
    const std::string& validatorAddress,
    utils::Amount amount,
    std::uint64_t blockHeight,
    bool requireExistingPosition
) {
    if (ownerAddress.empty() || validatorAddress.empty() || !amount.isPositive() || blockHeight == 0) {
        throw std::invalid_argument("Invalid staking deposit.");
    }
    if (blockHeight > std::numeric_limits<std::uint64_t>::max() - UNBONDING_DELAY_BLOCKS) {
        throw std::overflow_error("Stake unlock height would overflow.");
    }
    auto& positions = m_positionsByValidatorAndOwner[validatorAddress];
    auto found = positions.find(ownerAddress);
    if (requireExistingPosition && found == positions.end()) {
        throw std::invalid_argument("Stake top-up requires an existing owner position.");
    }
    Position next = found == positions.end() ? Position{} : found->second;
    next.amount = next.amount + amount;
    next.unlockHeight = blockHeight + UNBONDING_DELAY_BLOCKS;
    positions[ownerAddress] = next;

    const economics::StakeAccount current = accountOrDefault(validatorAddress);
    setAccount(validatorAddress, economics::StakeAccount(
        validatorAddress, current.bondedAmount() + amount, current.slashedAmount(),
        current.jailed(), current.tombstoned()
    ));
}

void StakingRegistry::withdraw(
    const std::string& ownerAddress,
    const std::string& validatorAddress,
    utils::Amount amount,
    std::uint64_t blockHeight
) {
    if (!amount.isPositive()) throw std::invalid_argument("Stake withdrawal amount must be positive.");
    auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
    if (validator == m_positionsByValidatorAndOwner.end()) {
        throw std::invalid_argument("Validator has no staking positions.");
    }
    auto owner = validator->second.find(ownerAddress);
    if (owner == validator->second.end() || owner->second.amount < amount) {
        throw std::invalid_argument("Stake withdrawal exceeds the owner's bonded position.");
    }
    if (blockHeight < owner->second.unlockHeight) {
        throw std::invalid_argument("Stake unbonding cooldown has not elapsed.");
    }
    const economics::StakeAccount current = accountOrDefault(validatorAddress);
    if (current.jailed() || current.tombstoned()) {
        throw std::invalid_argument("Jailed or tombstoned validator stake cannot be withdrawn.");
    }
    const utils::Amount available = current.bondedAmount() - current.slashedAmount();
    if (available < amount) throw std::invalid_argument("Available aggregate stake is insufficient.");

    owner->second.amount = owner->second.amount - amount;
    if (owner->second.amount.isZero()) validator->second.erase(owner);
    if (validator->second.empty()) m_positionsByValidatorAndOwner.erase(validator);
    setAccount(validatorAddress, economics::StakeAccount(
        validatorAddress, current.bondedAmount() - amount, current.slashedAmount(),
        current.jailed(), current.tombstoned()
    ));
}

utils::Amount StakingRegistry::ownedStake(
    const std::string& ownerAddress,
    const std::string& validatorAddress
) const {
    const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
    if (validator == m_positionsByValidatorAndOwner.end()) return utils::Amount();
    const auto owner = validator->second.find(ownerAddress);
    return owner == validator->second.end() ? utils::Amount() : owner->second.amount;
}

std::uint64_t StakingRegistry::unlockHeight(
    const std::string& ownerAddress,
    const std::string& validatorAddress
) const {
    const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
    if (validator == m_positionsByValidatorAndOwner.end()) return 0;
    const auto owner = validator->second.find(ownerAddress);
    return owner == validator->second.end() ? 0 : owner->second.unlockHeight;
}

void StakingRegistry::unjail(const std::string& validatorAddress) {
    economics::StakeAccount account = accountOrDefault(validatorAddress);
    if (!account.isValid() || !account.jailed() || account.tombstoned()) {
        throw std::invalid_argument("Validator staking state cannot be unjailed.");
    }
    account.unjail();
    setAccount(validatorAddress, std::move(account));
}

const std::map<std::string, economics::StakeAccount>& StakingRegistry::accounts() const {
    return m_accounts;
}

std::size_t StakingRegistry::size() const {
    return m_accounts.size();
}

bool StakingRegistry::isValid() const {
    for (const auto& [addr, account] : m_accounts) {
        if (addr.empty() || !account.isValid() || account.validatorAddress() != addr) return false;
    }
    for (const auto& [validator, positions] : m_positionsByValidatorAndOwner) {
        utils::Amount total;
        for (const auto& [owner, position] : positions) {
            if (owner.empty() || !position.amount.isPositive() || position.unlockHeight == 0) return false;
            total = total + position.amount;
        }
        const auto account = m_accounts.find(validator);
        if (account == m_accounts.end() || total > account->second.bondedAmount()) return false;
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
    oss << "];positions=[";
    first = true;
    for (const auto& [validator, positions] : m_positionsByValidatorAndOwner) {
        for (const auto& [owner, position] : positions) {
            if (!first) oss << ",";
            oss << "StakePosition{validator=" << validator
                << ";owner=" << owner
                << ";amount=" << position.amount.rawUnits()
                << ";unlockHeight=" << position.unlockHeight << "}";
            first = false;
        }
    }
    oss << "]}";
    return oss.str();
}

} // namespace nodo::node
