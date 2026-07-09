#include "node/StakingRegistry.hpp"

#include "crypto/hash.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string &value) {
  if (value.empty())
    return false;
  for (const char c : value) {
    if (c == ';' || c == '{' || c == '}' || c == '[' || c == ']' || c == ',' ||
        c == '\n' || c == '\r' || c == '\t') {
      return false;
    }
  }
  return true;
}

std::string hashString(const std::string &value) {
  char output[NODO_HASH_BUFFER_SIZE] = {0};
  nodo_hash_string(value.c_str(), output, sizeof(output));
  return std::string(output);
}

utils::Amount safeAvailable(const economics::StakeAccount &account) {
  if (account.bondedAmount() < account.slashedAmount()) {
    throw std::logic_error(
        "Invalid stake account has slash above bonded amount.");
  }
  return account.bondedAmount() - account.slashedAmount();
}

utils::Amount positionAvailable(const StakingRegistry::Position &position) {
  return position.activeAmount + position.pendingActivationAmount +
         position.pendingUnbondingAmount;
}

StakePositionStatus statusFor(const StakingRegistry::Position &position) {
  if (position.status == StakePositionStatus::TOMBSTONED) {
    return StakePositionStatus::TOMBSTONED;
  }
  if ((position.activeAmount + position.pendingActivationAmount +
       position.pendingUnbondingAmount)
          .isZero()) {
    return StakePositionStatus::WITHDRAWN;
  }
  if (position.activeAmount.isPositive() ||
      position.pendingUnbondingAmount.isPositive()) {
    return position.pendingUnbondingAmount.isPositive() &&
                   position.activeAmount.isZero()
               ? StakePositionStatus::UNBONDING
               : StakePositionStatus::ACTIVE;
  }
  return StakePositionStatus::PENDING_ACTIVATION;
}

void requireValidOperationInput(const std::string &ownerAddress,
                                const std::string &validatorAddress,
                                utils::Amount amount,
                                std::uint64_t blockHeight) {
  if (!isSafeScalar(ownerAddress) || !isSafeScalar(validatorAddress) ||
      !amount.isPositive() || blockHeight == 0) {
    throw std::invalid_argument("Invalid staking lifecycle operation.");
  }
}

} // namespace

std::string stakePositionStatusToString(StakePositionStatus status) {
  switch (status) {
  case StakePositionStatus::PENDING_ACTIVATION:
    return "PENDING_ACTIVATION";
  case StakePositionStatus::ACTIVE:
    return "ACTIVE";
  case StakePositionStatus::UNBONDING:
    return "UNBONDING";
  case StakePositionStatus::WITHDRAWN:
    return "WITHDRAWN";
  case StakePositionStatus::TOMBSTONED:
    return "TOMBSTONED";
  default:
    return "PENDING_ACTIVATION";
  }
}

utils::Amount StakePositionView::availableAmount() const {
  return activeAmount + pendingActivationAmount + pendingUnbondingAmount;
}

std::string StakePositionView::serialize() const {
  std::ostringstream oss;
  oss << "StakePosition{"
      << "id=" << positionId << ";owner=" << ownerAddress
      << ";validator=" << validatorAddress
      << ";status=" << stakePositionStatusToString(status)
      << ";active=" << activeAmount.rawUnits()
      << ";pendingActivation=" << pendingActivationAmount.rawUnits()
      << ";pendingUnbonding=" << pendingUnbondingAmount.rawUnits()
      << ";withdrawn=" << withdrawnAmount.rawUnits()
      << ";slashed=" << slashedAmount.rawUnits()
      << ";rewardsPending=" << rewardsPending.rawUnits()
      << ";lockHeight=" << lockHeight
      << ";activationHeight=" << activationHeight
      << ";unbondingStartHeight=" << unbondingStartHeight
      << ";withdrawableHeight=" << withdrawableHeight << "}";
  return oss.str();
}

bool StakeLifecycleRecord::isValid() const {
  return !recordId.empty() && !action.empty() && isSafeScalar(ownerAddress) &&
         isSafeScalar(validatorAddress) && !amount.isNegative() &&
         !activeAfter.isNegative() && !pendingActivationAfter.isNegative() &&
         !pendingUnbondingAfter.isNegative() && !slashedAfter.isNegative() &&
         blockHeight > 0;
}

std::string StakeLifecycleRecord::serialize() const {
  std::ostringstream oss;
  oss << "StakeLifecycleRecord{"
      << "id=" << recordId << ";action=" << action
      << ";transactionId=" << transactionId << ";owner=" << ownerAddress
      << ";validator=" << validatorAddress << ";amount=" << amount.rawUnits()
      << ";activeAfter=" << activeAfter.rawUnits()
      << ";pendingActivationAfter=" << pendingActivationAfter.rawUnits()
      << ";pendingUnbondingAfter=" << pendingUnbondingAfter.rawUnits()
      << ";slashedAfter=" << slashedAfter.rawUnits()
      << ";blockHeight=" << blockHeight
      << ";activationHeight=" << activationHeight
      << ";withdrawableHeight=" << withdrawableHeight << ";reason=" << reason
      << "}";
  return oss.str();
}

StakingRegistry StakingRegistry::restore(
    std::map<std::string, economics::StakeAccount> accounts,
    std::vector<StakePositionView> positions,
    std::vector<StakeLifecycleRecord> lifecycleRecords) {
  StakingRegistry registry;
  registry.m_accounts = std::move(accounts);

  for (const StakePositionView &view : positions) {
    Position position;
    position.positionId =
        stakePositionId(view.ownerAddress, view.validatorAddress);
    position.activeAmount = view.activeAmount;
    position.pendingActivationAmount = view.pendingActivationAmount;
    position.pendingUnbondingAmount = view.pendingUnbondingAmount;
    position.withdrawnAmount = view.withdrawnAmount;
    position.slashedAmount = view.slashedAmount;
    position.rewardsPending = view.rewardsPending;
    position.lockHeight = view.lockHeight;
    position.activationHeight = view.activationHeight;
    position.unbondingStartHeight = view.unbondingStartHeight;
    position.withdrawableHeight = view.withdrawableHeight;
    position.status = view.status;
    registry.m_positionsByValidatorAndOwner[view.validatorAddress]
                                            [view.ownerAddress] = position;
  }

  registry.m_lifecycleRecords = std::move(lifecycleRecords);

  if (!registry.isValid()) {
    throw std::invalid_argument(
        "Restored StakingRegistry failed structural validation.");
  }
  return registry;
}

std::string
StakingRegistry::stakePositionId(const std::string &ownerAddress,
                                 const std::string &validatorAddress) {
  if (!isSafeScalar(ownerAddress) || !isSafeScalar(validatorAddress)) {
    return "";
  }
  return hashString("stake-position:" + validatorAddress + ":" + ownerAddress);
}

bool StakingRegistry::hasAccount(const std::string &validatorAddress) const {
  return m_accounts.count(validatorAddress) > 0;
}

const economics::StakeAccount *
StakingRegistry::accountFor(const std::string &validatorAddress) const {
  const auto it = m_accounts.find(validatorAddress);
  if (it == m_accounts.end())
    return nullptr;
  return &it->second;
}

economics::StakeAccount
StakingRegistry::accountOrDefault(const std::string &validatorAddress) const {
  const auto *ptr = accountFor(validatorAddress);
  if (ptr != nullptr)
    return *ptr;
  return economics::StakeAccount(validatorAddress, utils::Amount());
}

void StakingRegistry::setAccount(const std::string &validatorAddress,
                                 economics::StakeAccount account) {
  if (!isSafeScalar(validatorAddress) || !account.isValid() ||
      account.validatorAddress() != validatorAddress) {
    throw std::invalid_argument(
        "Staking registry key does not match a valid stake account.");
  }
  m_accounts[validatorAddress] = std::move(account);

  auto &positions = m_positionsByValidatorAndOwner[validatorAddress];
  utils::Amount materializedAvailable;
  utils::Amount materializedSlashed;
  for (const auto &[owner, position] : positions) {
    (void)owner;
    materializedAvailable = materializedAvailable + positionAvailable(position);
    materializedSlashed = materializedSlashed + position.slashedAmount;
  }
  const economics::StakeAccount &stored = m_accounts.at(validatorAddress);
  if (!positions.empty() &&
      (stored.bondedAmount() != materializedAvailable + materializedSlashed ||
       stored.slashedAmount() != materializedSlashed)) {
    positions.clear();
  }

  if (positions.empty()) {
    const utils::Amount available = safeAvailable(stored);
    if (available.isPositive() || stored.slashedAmount().isPositive()) {
      Position position;
      position.positionId = stakePositionId(validatorAddress, validatorAddress);
      position.activeAmount = available;
      position.slashedAmount = stored.slashedAmount();
      position.lockHeight = 1;
      position.activationHeight = 1;
      position.status =
          stored.tombstoned()
              ? StakePositionStatus::TOMBSTONED
              : (available.isPositive() ? StakePositionStatus::ACTIVE
                                        : StakePositionStatus::WITHDRAWN);
      positions[validatorAddress] = position;
    }
  }
}

void StakingRegistry::deposit(const std::string &ownerAddress,
                              const std::string &validatorAddress,
                              utils::Amount amount, std::uint64_t blockHeight,
                              bool requireExistingPosition,
                              const std::string &transactionId) {
  requireValidOperationInput(ownerAddress, validatorAddress, amount,
                             blockHeight);
  if (blockHeight >
      std::numeric_limits<std::uint64_t>::max() - ACTIVATION_DELAY_BLOCKS) {
    throw std::overflow_error("Stake activation height would overflow.");
  }

  const economics::StakeAccount current = accountOrDefault(validatorAddress);
  if (current.tombstoned()) {
    throw std::invalid_argument("Cannot deposit into a tombstoned validator.");
  }

  auto &positions = m_positionsByValidatorAndOwner[validatorAddress];
  auto found = positions.find(ownerAddress);
  if (requireExistingPosition && found == positions.end()) {
    throw std::invalid_argument(
        "Stake top-up requires an existing owner position.");
  }
  if (found != positions.end() &&
      (found->second.status == StakePositionStatus::TOMBSTONED ||
       found->second.pendingUnbondingAmount.isPositive())) {
    throw std::invalid_argument(
        "Stake top-up requires a non-unbonding live position.");
  }

  Position next = found == positions.end() ? Position{} : found->second;
  if (next.positionId.empty()) {
    next.positionId = stakePositionId(ownerAddress, validatorAddress);
    next.lockHeight = blockHeight;
  }
  next.pendingActivationAmount = next.pendingActivationAmount + amount;
  next.activationHeight = blockHeight + ACTIVATION_DELAY_BLOCKS;
  next.status = statusFor(next);
  positions[ownerAddress] = next;

  rewriteAccountFromPositions(validatorAddress, current.jailed(),
                              current.tombstoned());
  appendLifecycleRecord(requireExistingPosition ? "TOP_UP" : "DEPOSIT",
                        transactionId, ownerAddress, validatorAddress, amount,
                        blockHeight, positions.at(ownerAddress),
                        requireExistingPosition
                            ? "stake top-up pending activation"
                            : "stake deposit pending activation");
}

void StakingRegistry::requestUnlock(const std::string &ownerAddress,
                                    const std::string &validatorAddress,
                                    utils::Amount amount,
                                    std::uint64_t blockHeight,
                                    const std::string &transactionId) {
  requireValidOperationInput(ownerAddress, validatorAddress, amount,
                             blockHeight);
  if (blockHeight >
      std::numeric_limits<std::uint64_t>::max() - UNBONDING_DELAY_BLOCKS) {
    throw std::overflow_error("Stake withdrawable height would overflow.");
  }

  const economics::StakeAccount current = accountOrDefault(validatorAddress);
  if (current.jailed() || current.tombstoned()) {
    throw std::invalid_argument(
        "Jailed or tombstoned validator stake cannot be unlocked.");
  }

  auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end()) {
    throw std::invalid_argument("Validator has no staking positions.");
  }
  auto owner = validator->second.find(ownerAddress);
  if (owner == validator->second.end() || owner->second.activeAmount < amount) {
    throw std::invalid_argument("Stake unlock exceeds active owner stake.");
  }

  owner->second.activeAmount = owner->second.activeAmount - amount;
  owner->second.pendingUnbondingAmount =
      owner->second.pendingUnbondingAmount + amount;
  owner->second.unbondingStartHeight = blockHeight;
  owner->second.withdrawableHeight = blockHeight + UNBONDING_DELAY_BLOCKS;
  owner->second.status = statusFor(owner->second);
  rewriteAccountFromPositions(validatorAddress, current.jailed(),
                              current.tombstoned());
  appendLifecycleRecord("UNLOCK", transactionId, ownerAddress, validatorAddress,
                        amount, blockHeight, owner->second,
                        "stake moved to unbonding");
}

void StakingRegistry::withdraw(const std::string &ownerAddress,
                               const std::string &validatorAddress,
                               utils::Amount amount, std::uint64_t blockHeight,
                               const std::string &transactionId) {
  requireValidOperationInput(ownerAddress, validatorAddress, amount,
                             blockHeight);

  const economics::StakeAccount current = accountOrDefault(validatorAddress);
  if (current.jailed() || current.tombstoned()) {
    throw std::invalid_argument(
        "Jailed or tombstoned validator stake cannot be withdrawn.");
  }

  auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end()) {
    throw std::invalid_argument("Validator has no staking positions.");
  }
  auto owner = validator->second.find(ownerAddress);
  if (owner == validator->second.end() ||
      owner->second.pendingUnbondingAmount < amount) {
    throw std::invalid_argument(
        "Stake withdrawal exceeds the owner's unbonding position.");
  }
  if (owner->second.withdrawableHeight == 0 ||
      blockHeight < owner->second.withdrawableHeight) {
    throw std::invalid_argument("Stake unbonding cooldown has not elapsed.");
  }

  owner->second.pendingUnbondingAmount =
      owner->second.pendingUnbondingAmount - amount;
  owner->second.withdrawnAmount = owner->second.withdrawnAmount + amount;
  owner->second.status = statusFor(owner->second);
  rewriteAccountFromPositions(validatorAddress, current.jailed(),
                              current.tombstoned());
  appendLifecycleRecord("WITHDRAW", transactionId, ownerAddress,
                        validatorAddress, amount, blockHeight, owner->second,
                        "withdrawable stake credited to liquid account");
}

void StakingRegistry::requestValidatorExit(const std::string &ownerAddress,
                                           const std::string &validatorAddress,
                                           std::uint64_t blockHeight,
                                           const std::string &transactionId) {
  if (!isSafeScalar(ownerAddress) || !isSafeScalar(validatorAddress) ||
      blockHeight == 0) {
    throw std::invalid_argument("Invalid validator exit staking operation.");
  }
  if (blockHeight >
      std::numeric_limits<std::uint64_t>::max() - UNBONDING_DELAY_BLOCKS) {
    throw std::overflow_error("Stake withdrawable height would overflow.");
  }
  auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end()) {
    return;
  }
  auto owner = validator->second.find(ownerAddress);
  if (owner == validator->second.end() || owner->second.activeAmount.isZero()) {
    return;
  }
  const economics::StakeAccount current = accountOrDefault(validatorAddress);
  const utils::Amount amount = owner->second.activeAmount;
  owner->second.activeAmount = utils::Amount();
  owner->second.pendingUnbondingAmount =
      owner->second.pendingUnbondingAmount + amount;
  owner->second.unbondingStartHeight = blockHeight;
  owner->second.withdrawableHeight = blockHeight + UNBONDING_DELAY_BLOCKS;
  owner->second.status = statusFor(owner->second);
  rewriteAccountFromPositions(validatorAddress, current.jailed(),
                              current.tombstoned());
  appendLifecycleRecord("VALIDATOR_EXIT_UNBOND", transactionId, ownerAddress,
                        validatorAddress, amount, blockHeight, owner->second,
                        "validator exit moved active stake to unbonding");
}

bool StakingRegistry::activatePending(std::uint64_t boundaryHeight) {
  if (boundaryHeight == 0)
    return false;

  bool changed = false;
  for (auto &[validator, positions] : m_positionsByValidatorAndOwner) {
    const economics::StakeAccount current = accountOrDefault(validator);
    if (current.tombstoned())
      continue;
    for (auto &[owner, position] : positions) {
      (void)owner;
      if (position.pendingActivationAmount.isPositive() &&
          position.activationHeight <= boundaryHeight) {
        const utils::Amount activated = position.pendingActivationAmount;
        position.activeAmount = position.activeAmount + activated;
        position.pendingActivationAmount = utils::Amount();
        position.status = statusFor(position);
        appendLifecycleRecord(
            "ACTIVATE", "", owner, validator, activated, boundaryHeight,
            position, "pending stake activated at deterministic height");
        changed = true;
      }
    }
    if (changed) {
      rewriteAccountFromPositions(validator, current.jailed(),
                                  current.tombstoned());
    }
  }
  return changed;
}

void StakingRegistry::applyPenaltyState(const std::string &validatorAddress,
                                        utils::Amount totalSlashedAmount,
                                        bool jailed, bool tombstoned,
                                        std::uint64_t blockHeight) {
  if (!isSafeScalar(validatorAddress) || totalSlashedAmount.isNegative() ||
      blockHeight == 0) {
    throw std::invalid_argument("Invalid staking penalty state.");
  }

  const economics::StakeAccount current = accountOrDefault(validatorAddress);
  if (totalSlashedAmount < current.slashedAmount()) {
    throw std::invalid_argument("Canonical slashing total cannot decrease.");
  }

  utils::Amount remainingSlash = totalSlashedAmount - current.slashedAmount();
  auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator != m_positionsByValidatorAndOwner.end()) {
    for (auto &[owner, position] : validator->second) {
      if (remainingSlash.isZero())
        break;

      utils::Amount fromActive;
      if (position.activeAmount < remainingSlash) {
        fromActive = position.activeAmount;
      } else {
        fromActive = remainingSlash;
      }
      if (fromActive.isPositive()) {
        position.activeAmount = position.activeAmount - fromActive;
        position.slashedAmount = position.slashedAmount + fromActive;
        remainingSlash = remainingSlash - fromActive;
      }

      utils::Amount fromPendingActivation;
      if (remainingSlash.isPositive()) {
        fromPendingActivation =
            position.pendingActivationAmount < remainingSlash
                ? position.pendingActivationAmount
                : remainingSlash;
        if (fromPendingActivation.isPositive()) {
          position.pendingActivationAmount =
              position.pendingActivationAmount - fromPendingActivation;
          position.slashedAmount =
              position.slashedAmount + fromPendingActivation;
          remainingSlash = remainingSlash - fromPendingActivation;
        }
      }

      utils::Amount fromUnbonding;
      if (remainingSlash.isPositive()) {
        fromUnbonding = position.pendingUnbondingAmount < remainingSlash
                            ? position.pendingUnbondingAmount
                            : remainingSlash;
        if (fromUnbonding.isPositive()) {
          position.pendingUnbondingAmount =
              position.pendingUnbondingAmount - fromUnbonding;
          position.slashedAmount = position.slashedAmount + fromUnbonding;
          remainingSlash = remainingSlash - fromUnbonding;
        }
      }

      if (tombstoned) {
        position.status = StakePositionStatus::TOMBSTONED;
      } else {
        position.status = statusFor(position);
      }
      const utils::Amount applied =
          fromActive + fromPendingActivation + fromUnbonding;
      if (applied.isPositive()) {
        appendLifecycleRecord("SLASH", "", owner, validatorAddress, applied,
                              blockHeight, position,
                              tombstoned
                                  ? "stake slashed and validator tombstoned"
                                  : "stake slashed");
      }
    }
  }

  if (remainingSlash.isPositive()) {
    throw std::invalid_argument(
        "Canonical slashing total exceeds remaining stake positions.");
  }

  rewriteAccountFromPositions(validatorAddress, jailed || tombstoned,
                              tombstoned);
  const economics::StakeAccount rewritten = accountOrDefault(validatorAddress);
  if (rewritten.slashedAmount() != totalSlashedAmount) {
    setAccount(validatorAddress,
               economics::StakeAccount(
                   validatorAddress, rewritten.bondedAmount(),
                   totalSlashedAmount, jailed || tombstoned, tombstoned));
  }
}

utils::Amount
StakingRegistry::ownedStake(const std::string &ownerAddress,
                            const std::string &validatorAddress) const {
  return activeStake(ownerAddress, validatorAddress) +
         pendingActivationStake(ownerAddress, validatorAddress) +
         pendingUnbondingStake(ownerAddress, validatorAddress);
}

utils::Amount
StakingRegistry::activeStake(const std::string &ownerAddress,
                             const std::string &validatorAddress) const {
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end())
    return utils::Amount();
  const auto owner = validator->second.find(ownerAddress);
  return owner == validator->second.end() ? utils::Amount()
                                          : owner->second.activeAmount;
}

utils::Amount StakingRegistry::pendingActivationStake(
    const std::string &ownerAddress,
    const std::string &validatorAddress) const {
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end())
    return utils::Amount();
  const auto owner = validator->second.find(ownerAddress);
  return owner == validator->second.end()
             ? utils::Amount()
             : owner->second.pendingActivationAmount;
}

utils::Amount StakingRegistry::pendingUnbondingStake(
    const std::string &ownerAddress,
    const std::string &validatorAddress) const {
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end())
    return utils::Amount();
  const auto owner = validator->second.find(ownerAddress);
  return owner == validator->second.end()
             ? utils::Amount()
             : owner->second.pendingUnbondingAmount;
}

utils::Amount
StakingRegistry::withdrawableStake(const std::string &ownerAddress,
                                   const std::string &validatorAddress,
                                   std::uint64_t blockHeight) const {
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end())
    return utils::Amount();
  const auto owner = validator->second.find(ownerAddress);
  if (owner == validator->second.end() ||
      owner->second.withdrawableHeight == 0 ||
      blockHeight < owner->second.withdrawableHeight) {
    return utils::Amount();
  }
  return owner->second.pendingUnbondingAmount;
}

utils::Amount
StakingRegistry::activeStakeFor(const std::string &validatorAddress) const {
  const auto account = m_accounts.find(validatorAddress);
  if (account != m_accounts.end() &&
      (account->second.jailed() || account->second.tombstoned())) {
    return utils::Amount();
  }
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end() ||
      validator->second.empty()) {
    return account == m_accounts.end() ? utils::Amount()
                                       : safeAvailable(account->second);
  }
  utils::Amount total;
  for (const auto &[owner, position] : validator->second) {
    (void)owner;
    if (position.status != StakePositionStatus::TOMBSTONED) {
      total = total + position.activeAmount;
    }
  }
  return total;
}

utils::Amount
StakingRegistry::bondedStakeFor(const std::string &validatorAddress) const {
  return accountOrDefault(validatorAddress).bondedAmount();
}

std::uint64_t
StakingRegistry::unlockHeight(const std::string &ownerAddress,
                              const std::string &validatorAddress) const {
  const auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  if (validator == m_positionsByValidatorAndOwner.end())
    return 0;
  const auto owner = validator->second.find(ownerAddress);
  return owner == validator->second.end() ? 0
                                          : owner->second.withdrawableHeight;
}

void StakingRegistry::unjail(const std::string &validatorAddress) {
  economics::StakeAccount account = accountOrDefault(validatorAddress);
  if (!account.isValid() || !account.jailed() || account.tombstoned()) {
    throw std::invalid_argument("Validator staking state cannot be unjailed.");
  }
  account.unjail();
  setAccount(validatorAddress, std::move(account));
}

void StakingRegistry::rotateValidatorAddress(
    const std::string &oldValidatorAddress,
    const std::string &newValidatorAddress, const std::string &ownerAddress,
    std::uint64_t blockHeight, const std::string &transactionId) {
  if (!isSafeScalar(oldValidatorAddress) ||
      !isSafeScalar(newValidatorAddress) || !isSafeScalar(ownerAddress) ||
      oldValidatorAddress == newValidatorAddress || blockHeight == 0) {
    throw std::invalid_argument(
        "Invalid validator key rotation staking input.");
  }
  if (m_accounts.find(newValidatorAddress) != m_accounts.end() ||
      m_positionsByValidatorAndOwner.find(newValidatorAddress) !=
          m_positionsByValidatorAndOwner.end()) {
    throw std::invalid_argument(
        "New validator address already has staking state.");
  }

  auto accountIt = m_accounts.find(oldValidatorAddress);
  auto positionsIt = m_positionsByValidatorAndOwner.find(oldValidatorAddress);
  if (accountIt == m_accounts.end() ||
      positionsIt == m_positionsByValidatorAndOwner.end()) {
    throw std::invalid_argument(
        "Old validator has no staking state to rotate.");
  }

  const economics::StakeAccount oldAccount = accountIt->second;
  auto movedPositions = positionsIt->second;
  if (movedPositions.find(ownerAddress) == movedPositions.end()) {
    throw std::invalid_argument(
        "Validator key rotation owner has no stake position.");
  }

  m_accounts.erase(accountIt);
  m_positionsByValidatorAndOwner.erase(positionsIt);

  for (auto &[owner, position] : movedPositions) {
    position.positionId = stakePositionId(owner, newValidatorAddress);
    appendLifecycleRecord("VALIDATOR_KEY_ROTATE", transactionId, owner,
                          newValidatorAddress, positionAvailable(position),
                          blockHeight, position,
                          "validator key rotation from " + oldValidatorAddress);
  }

  m_positionsByValidatorAndOwner.emplace(newValidatorAddress,
                                         std::move(movedPositions));
  m_accounts.emplace(
      newValidatorAddress,
      economics::StakeAccount(newValidatorAddress, oldAccount.bondedAmount(),
                              oldAccount.slashedAmount(), oldAccount.jailed(),
                              oldAccount.tombstoned()));

  if (!isValid()) {
    throw std::logic_error("Staking registry failed post key-rotation audit.");
  }
}

const std::map<std::string, economics::StakeAccount> &
StakingRegistry::accounts() const {
  return m_accounts;
}

std::vector<StakePositionView> StakingRegistry::positions() const {
  std::vector<StakePositionView> result;
  for (const auto &[validator, ownerPositions] :
       m_positionsByValidatorAndOwner) {
    for (const auto &[owner, position] : ownerPositions) {
      result.push_back(viewFor(validator, owner, position));
    }
  }
  return result;
}

std::vector<StakePositionView>
StakingRegistry::positionsForOwner(const std::string &ownerAddress) const {
  std::vector<StakePositionView> result;
  for (const auto &[validator, ownerPositions] :
       m_positionsByValidatorAndOwner) {
    const auto found = ownerPositions.find(ownerAddress);
    if (found != ownerPositions.end()) {
      result.push_back(viewFor(validator, ownerAddress, found->second));
    }
  }
  return result;
}

const std::vector<StakeLifecycleRecord> &
StakingRegistry::lifecycleRecords() const {
  return m_lifecycleRecords;
}

std::size_t StakingRegistry::size() const { return m_accounts.size(); }

bool StakingRegistry::isValid() const {
  for (const auto &[addr, account] : m_accounts) {
    if (!isSafeScalar(addr) || !account.isValid() ||
        account.validatorAddress() != addr) {
      return false;
    }
  }
  for (const auto &[validator, positions] : m_positionsByValidatorAndOwner) {
    if (!isSafeScalar(validator))
      return false;
    utils::Amount availableTotal;
    utils::Amount slashedTotal;
    for (const auto &[owner, position] : positions) {
      if (!isSafeScalar(owner) ||
          position.positionId != stakePositionId(owner, validator) ||
          position.lockHeight == 0 || position.activationHeight == 0 ||
          position.activeAmount.isNegative() ||
          position.pendingActivationAmount.isNegative() ||
          position.pendingUnbondingAmount.isNegative() ||
          position.withdrawnAmount.isNegative() ||
          position.slashedAmount.isNegative() ||
          position.rewardsPending.isNegative()) {
        return false;
      }
      availableTotal = availableTotal + positionAvailable(position);
      slashedTotal = slashedTotal + position.slashedAmount;
    }
    const auto account = m_accounts.find(validator);
    if (account == m_accounts.end())
      return false;
    if (account->second.slashedAmount() != slashedTotal)
      return false;
    if (account->second.bondedAmount() != availableTotal + slashedTotal)
      return false;
  }
  for (const auto &record : m_lifecycleRecords) {
    if (!record.isValid())
      return false;
  }
  return true;
}

std::string StakingRegistry::serialize() const {
  std::ostringstream oss;
  const std::vector<StakePositionView> allPositions = positions();
  oss << "StakingRegistry{count=" << m_accounts.size()
      << ";positionCount=" << allPositions.size()
      << ";lifecycleRecordCount=" << m_lifecycleRecords.size() << ";accounts=[";
  bool first = true;
  for (const auto &[addr, account] : m_accounts) {
    if (!first)
      oss << ",";
    oss << account.serialize();
    first = false;
  }
  oss << "];positions=[";
  first = true;
  for (const auto &position : allPositions) {
    if (!first)
      oss << ",";
    oss << position.serialize();
    first = false;
  }
  oss << "];lifecycle=[";
  first = true;
  for (const auto &record : m_lifecycleRecords) {
    if (!first)
      oss << ",";
    oss << record.serialize();
    first = false;
  }
  oss << "]}";
  return oss.str();
}

StakePositionView StakingRegistry::viewFor(const std::string &validatorAddress,
                                           const std::string &ownerAddress,
                                           const Position &position) const {
  StakePositionView view;
  view.positionId = position.positionId;
  view.ownerAddress = ownerAddress;
  view.validatorAddress = validatorAddress;
  view.activeAmount = position.activeAmount;
  view.pendingActivationAmount = position.pendingActivationAmount;
  view.pendingUnbondingAmount = position.pendingUnbondingAmount;
  view.withdrawnAmount = position.withdrawnAmount;
  view.slashedAmount = position.slashedAmount;
  view.rewardsPending = position.rewardsPending;
  view.lockHeight = position.lockHeight;
  view.activationHeight = position.activationHeight;
  view.unbondingStartHeight = position.unbondingStartHeight;
  view.withdrawableHeight = position.withdrawableHeight;
  view.status = position.status;
  return view;
}

void StakingRegistry::appendLifecycleRecord(
    std::string action, const std::string &transactionId,
    const std::string &ownerAddress, const std::string &validatorAddress,
    utils::Amount amount, std::uint64_t blockHeight, const Position &position,
    std::string reason) {
  StakeLifecycleRecord record;
  record.action = std::move(action);
  record.transactionId = transactionId;
  record.ownerAddress = ownerAddress;
  record.validatorAddress = validatorAddress;
  record.amount = amount;
  record.activeAfter = position.activeAmount;
  record.pendingActivationAfter = position.pendingActivationAmount;
  record.pendingUnbondingAfter = position.pendingUnbondingAmount;
  record.slashedAfter = position.slashedAmount;
  record.blockHeight = blockHeight;
  record.activationHeight = position.activationHeight;
  record.withdrawableHeight = position.withdrawableHeight;
  record.reason = std::move(reason);
  record.recordId = hashString(
      "stake-lifecycle:" + std::to_string(m_lifecycleRecords.size()) + ":" +
      record.action + ":" + validatorAddress + ":" + ownerAddress + ":" +
      std::to_string(blockHeight) + ":" + std::to_string(amount.rawUnits()) +
      ":" + transactionId);
  if (!record.isValid()) {
    throw std::logic_error("Generated invalid stake lifecycle record.");
  }
  m_lifecycleRecords.push_back(std::move(record));
}

void StakingRegistry::rewriteAccountFromPositions(
    const std::string &validatorAddress, bool preserveJailed,
    bool preserveTombstoned) {
  auto validator = m_positionsByValidatorAndOwner.find(validatorAddress);
  utils::Amount availableTotal;
  utils::Amount slashedTotal;
  if (validator != m_positionsByValidatorAndOwner.end()) {
    for (auto &[owner, position] : validator->second) {
      (void)owner;
      if (preserveTombstoned) {
        position.status = StakePositionStatus::TOMBSTONED;
      } else {
        position.status = statusFor(position);
      }
      availableTotal = availableTotal + positionAvailable(position);
      slashedTotal = slashedTotal + position.slashedAmount;
    }
  }
  setAccount(validatorAddress,
             economics::StakeAccount(
                 validatorAddress, availableTotal + slashedTotal, slashedTotal,
                 preserveJailed || preserveTombstoned, preserveTombstoned));
}

} // namespace nodo::node
