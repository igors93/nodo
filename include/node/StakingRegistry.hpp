#ifndef NODO_NODE_STAKING_REGISTRY_HPP
#define NODO_NODE_STAKING_REGISTRY_HPP

#include "economics/StakeAccount.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::node {

enum class StakePositionStatus {
  PENDING_ACTIVATION,
  ACTIVE,
  UNBONDING,
  WITHDRAWN,
  TOMBSTONED
};

std::string stakePositionStatusToString(StakePositionStatus status);

struct StakePositionView {
  std::string positionId;
  std::string ownerAddress;
  std::string validatorAddress;
  utils::Amount activeAmount;
  utils::Amount pendingActivationAmount;
  utils::Amount pendingUnbondingAmount;
  utils::Amount withdrawnAmount;
  utils::Amount slashedAmount;
  utils::Amount rewardsPending;
  std::uint64_t lockHeight = 0;
  std::uint64_t activationHeight = 0;
  std::uint64_t unbondingStartHeight = 0;
  std::uint64_t withdrawableHeight = 0;
  StakePositionStatus status = StakePositionStatus::PENDING_ACTIVATION;

  utils::Amount availableAmount() const;
  std::string serialize() const;
};

struct StakeLifecycleRecord {
  std::string recordId;
  std::string action;
  std::string transactionId;
  std::string ownerAddress;
  std::string validatorAddress;
  utils::Amount amount;
  utils::Amount activeAfter;
  utils::Amount pendingActivationAfter;
  utils::Amount pendingUnbondingAfter;
  utils::Amount slashedAfter;
  std::uint64_t blockHeight = 0;
  std::uint64_t activationHeight = 0;
  std::uint64_t withdrawableHeight = 0;
  std::string reason;

  bool isValid() const;
  std::string serialize() const;
};

/*
 * StakingRegistry is the authoritative in-memory store for validator stake
 * accounts and positions. It tracks bonded, active, pending activation,
 * unbonding, withdrawn, slashed, jail and tombstone state for every validator
 * that has ever received protocol stake.
 *
 * The registry is captured by value into the DeterministicStateDomainTransition
 * closure so that block validation is deterministic and stateless from the
 * caller's perspective.
 */
class StakingRegistry {
public:
  static constexpr std::uint64_t UNBONDING_DELAY_BLOCKS = 21;
  static constexpr std::uint64_t ACTIVATION_DELAY_BLOCKS = 1;

  struct Position {
    std::string positionId;
    utils::Amount activeAmount;
    utils::Amount pendingActivationAmount;
    utils::Amount pendingUnbondingAmount;
    utils::Amount withdrawnAmount;
    utils::Amount slashedAmount;
    utils::Amount rewardsPending;
    std::uint64_t lockHeight = 0;
    std::uint64_t activationHeight = 0;
    std::uint64_t unbondingStartHeight = 0;
    std::uint64_t withdrawableHeight = 0;
    StakePositionStatus status = StakePositionStatus::PENDING_ACTIVATION;
  };

  StakingRegistry() = default;

  static std::string stakePositionId(const std::string &ownerAddress,
                                     const std::string &validatorAddress);

  bool hasAccount(const std::string &validatorAddress) const;

  // Returns nullptr if the validator has no entry.
  const economics::StakeAccount *
  accountFor(const std::string &validatorAddress) const;

  // Returns a default-constructed (zero-bonded) StakeAccount for unknown
  // addresses.
  economics::StakeAccount
  accountOrDefault(const std::string &validatorAddress) const;

  // Insert or replace the stake account for a validator.
  void setAccount(const std::string &validatorAddress,
                  economics::StakeAccount account);

  void deposit(const std::string &ownerAddress,
               const std::string &validatorAddress, utils::Amount amount,
               std::uint64_t blockHeight, bool requireExistingPosition,
               const std::string &transactionId = "");

  void requestUnlock(const std::string &ownerAddress,
                     const std::string &validatorAddress, utils::Amount amount,
                     std::uint64_t blockHeight,
                     const std::string &transactionId = "");

  void withdraw(const std::string &ownerAddress,
                const std::string &validatorAddress, utils::Amount amount,
                std::uint64_t blockHeight,
                const std::string &transactionId = "");

  void requestValidatorExit(const std::string &ownerAddress,
                            const std::string &validatorAddress,
                            std::uint64_t blockHeight,
                            const std::string &transactionId = "");

  bool activatePending(std::uint64_t boundaryHeight);

  void applyPenaltyState(const std::string &validatorAddress,
                         utils::Amount totalSlashedAmount, bool jailed,
                         bool tombstoned, std::uint64_t blockHeight);

  utils::Amount ownedStake(const std::string &ownerAddress,
                           const std::string &validatorAddress) const;
  utils::Amount activeStake(const std::string &ownerAddress,
                            const std::string &validatorAddress) const;
  utils::Amount
  pendingActivationStake(const std::string &ownerAddress,
                         const std::string &validatorAddress) const;
  utils::Amount
  pendingUnbondingStake(const std::string &ownerAddress,
                        const std::string &validatorAddress) const;
  utils::Amount withdrawableStake(const std::string &ownerAddress,
                                  const std::string &validatorAddress,
                                  std::uint64_t blockHeight) const;
  utils::Amount activeStakeFor(const std::string &validatorAddress) const;
  utils::Amount bondedStakeFor(const std::string &validatorAddress) const;
  std::uint64_t unlockHeight(const std::string &ownerAddress,
                             const std::string &validatorAddress) const;
  void unjail(const std::string &validatorAddress);

  void rotateValidatorAddress(const std::string &oldValidatorAddress,
                              const std::string &newValidatorAddress,
                              const std::string &ownerAddress,
                              std::uint64_t blockHeight,
                              const std::string &transactionId = "");

  const std::map<std::string, economics::StakeAccount> &accounts() const;
  std::vector<StakePositionView> positions() const;
  std::vector<StakePositionView>
  positionsForOwner(const std::string &ownerAddress) const;
  const std::vector<StakeLifecycleRecord> &lifecycleRecords() const;

  std::size_t size() const;

  bool isValid() const;

  std::string serialize() const;

private:
  std::map<std::string, economics::StakeAccount> m_accounts;
  std::map<std::string, std::map<std::string, Position>>
      m_positionsByValidatorAndOwner;
  std::vector<StakeLifecycleRecord> m_lifecycleRecords;

  StakePositionView viewFor(const std::string &validatorAddress,
                            const std::string &ownerAddress,
                            const Position &position) const;

  void appendLifecycleRecord(std::string action,
                             const std::string &transactionId,
                             const std::string &ownerAddress,
                             const std::string &validatorAddress,
                             utils::Amount amount, std::uint64_t blockHeight,
                             const Position &position, std::string reason);

  void rewriteAccountFromPositions(const std::string &validatorAddress,
                                   bool preserveJailed,
                                   bool preserveTombstoned);
};

} // namespace nodo::node

#endif
