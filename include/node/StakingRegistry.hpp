#ifndef NODO_NODE_STAKING_REGISTRY_HPP
#define NODO_NODE_STAKING_REGISTRY_HPP

#include "economics/StakeAccount.hpp"

#include <map>
#include <string>
#include <cstdint>

namespace nodo::node {

/*
 * StakingRegistry is the authoritative in-memory store for validator stake
 * accounts. It tracks bonded amounts, slashing, jail status, and tombstoning
 * for every validator that has ever submitted a STAKE_DEPOSIT transaction.
 *
 * The registry is captured by value into the DeterministicStateDomainTransition
 * closure so that block validation is deterministic and stateless from the
 * caller's perspective.
 */
class StakingRegistry {
public:
    static constexpr std::uint64_t UNBONDING_DELAY_BLOCKS = 21;
    StakingRegistry() = default;

    bool hasAccount(const std::string& validatorAddress) const;

    // Returns nullptr if the validator has no entry.
    const economics::StakeAccount* accountFor(const std::string& validatorAddress) const;

    // Returns a default-constructed (zero-bonded) StakeAccount for unknown addresses.
    economics::StakeAccount accountOrDefault(const std::string& validatorAddress) const;

    // Insert or replace the stake account for a validator.
    void setAccount(const std::string& validatorAddress, economics::StakeAccount account);

    void deposit(
        const std::string& ownerAddress,
        const std::string& validatorAddress,
        utils::Amount amount,
        std::uint64_t blockHeight,
        bool requireExistingPosition
    );

    void withdraw(
        const std::string& ownerAddress,
        const std::string& validatorAddress,
        utils::Amount amount,
        std::uint64_t blockHeight
    );

    utils::Amount ownedStake(
        const std::string& ownerAddress,
        const std::string& validatorAddress
    ) const;
    std::uint64_t unlockHeight(
        const std::string& ownerAddress,
        const std::string& validatorAddress
    ) const;
    void unjail(const std::string& validatorAddress);

    const std::map<std::string, economics::StakeAccount>& accounts() const;

    std::size_t size() const;

    bool isValid() const;

    std::string serialize() const;

private:
    struct Position {
        utils::Amount amount;
        std::uint64_t unlockHeight = 0;
    };
    std::map<std::string, economics::StakeAccount> m_accounts;
    std::map<std::string, std::map<std::string, Position>> m_positionsByValidatorAndOwner;
};

} // namespace nodo::node

#endif
