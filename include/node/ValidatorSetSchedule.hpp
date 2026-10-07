#ifndef NODO_NODE_VALIDATOR_SET_SCHEDULE_HPP
#define NODO_NODE_VALIDATOR_SET_SCHEDULE_HPP

#include "core/ValidatorRegistry.hpp"
#include "node/ValidatorLifecycle.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace nodo::node {

// Consensus membership is chosen only after the last block of an epoch is
// final. The mutable economic registry may change on every block, but it is
// never the authority for a vote or proposer at the current height.
class ValidatorSetSchedule {
public:
  static constexpr std::uint64_t kMaxChurnBasisPoints = 3333;
  static bool isBoundary(std::uint64_t finalizedHeight) {
    return finalizedHeight != 0 &&
           finalizedHeight % NODO_VALIDATOR_EPOCH_BLOCKS == 0;
  }

  static std::uint64_t activationEpoch(std::uint64_t transactionHeight) {
    if (transactionHeight == 0)
      throw std::invalid_argument("Validator activation requires a positive height.");
    const std::uint64_t epoch =
        (transactionHeight - 1) / NODO_VALIDATOR_EPOCH_BLOCKS + 1;
    if (epoch > std::numeric_limits<std::uint64_t>::max() - 2)
      throw std::overflow_error("Validator activation epoch overflow.");
    return epoch + 2;
  }

  static std::uint64_t activationHeight(std::uint64_t transactionHeight) {
    const std::uint64_t epoch = activationEpoch(transactionHeight);
    if (epoch - 1 > (std::numeric_limits<std::uint64_t>::max() - 1) /
                    NODO_VALIDATOR_EPOCH_BLOCKS)
      throw std::overflow_error("Validator activation height overflow.");
    return (epoch - 1) * NODO_VALIDATOR_EPOCH_BLOCKS + 1;
  }

  // The L1 bound counts changed voting weight, including exits, additions,
  // stake changes and key rotations. Splitting one change across keys cannot
  // increase this budget. Changes that exceed it remain staged for later
  // epochs; a single stake change may be projected in bounded increments.
  static core::ValidatorRegistry project(
      const core::ValidatorRegistry &previous,
      const core::ValidatorRegistry &target,
      std::uint64_t effectiveEpoch) {
    if (!previous.isValid() || !target.isValid() ||
        previous.totalConsensusWeight() == 0 || effectiveEpoch == 0)
      throw std::invalid_argument("Invalid validator-set projection input.");

    const std::uint64_t budget = static_cast<std::uint64_t>(
        (static_cast<unsigned __int128>(previous.totalConsensusWeight()) *
         kMaxChurnBasisPoints) /
        10000);
    std::uint64_t remaining = budget;
    std::map<std::string, core::ValidatorRegistryEntry> selected;
    const auto oldAddresses = previous.validatorAddresses();
    const auto targetAddresses = target.validatorAddresses();
    std::set<std::string> all(oldAddresses.begin(), oldAddresses.end());
    all.insert(targetAddresses.begin(), targetAddresses.end());

    // Process removals and reductions first. This prevents a key rotation
    // from temporarily assigning the same locked stake to both keys.
    for (const std::string &address : all) {
      const auto *old = previous.entryForAddress(address);
      const auto *next = target.entryForAddress(address);
      const std::uint64_t oldWeight = previous.consensusWeightFor(address);
      const std::uint64_t nextWeight = target.consensusWeightFor(address);
      if (oldWeight == 0 || nextWeight >= oldWeight)
        continue;
      if (next == nullptr) {
        bool replacementIsImmature = false;
        for (const std::string &newAddress : targetAddresses) {
          const auto *replacement = target.entryForAddress(newAddress);
          if (previous.hasValidator(newAddress) || replacement == nullptr ||
              !replacement->eligibleForConsensus() ||
              replacement->ownerAddress() != old->ownerAddress())
            continue;
          if (replacement->registrationRecord().activationEpoch() >
              effectiveEpoch) {
            replacementIsImmature = true;
            break;
          }
        }
        if (replacementIsImmature) {
          selected.emplace(address, *old);
          continue;
        }
      }
      if (oldWeight - nextWeight <= remaining) {
        remaining -= oldWeight - nextWeight;
        if (nextWeight != 0 && next != nullptr)
          selected.emplace(address, *next);
      } else {
        const std::uint64_t minimum =
            core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS;
        const std::uint64_t projectedWeight =
            std::max(minimum, oldWeight - remaining);
        const std::uint64_t spent = oldWeight - projectedWeight;
        selected.emplace(address, withStake(*old, projectedWeight));
        remaining -= spent;
      }
    }

    for (const std::string &address : all) {
      if (selected.contains(address))
        continue;
      const auto *old = previous.entryForAddress(address);
      const auto *next = target.entryForAddress(address);
      const std::uint64_t oldWeight = previous.consensusWeightFor(address);
      const std::uint64_t nextWeight = target.consensusWeightFor(address);
      if (oldWeight > nextWeight)
        continue; // A completed removal was handled above.
      if (next == nullptr)
        continue;
      // Pending, jailed and exited entries belong in economic state, not in
      // the consensus set or its historical QC snapshots.
      if (nextWeight == 0)
        continue;
      if (old == nullptr && nextWeight != 0 &&
          next->registrationRecord().activationEpoch() > effectiveEpoch)
        continue;

      // If an old key disappeared from the target but remains live in the
      // selected set, delay a same-owner replacement key.
      bool oldKeyStillLive = false;
      if (old == nullptr && nextWeight != 0) {
        for (const auto &[oldAddress, entry] : selected) {
          if (!target.hasValidator(oldAddress) &&
              entry.eligibleForConsensus() &&
              entry.ownerAddress() == next->ownerAddress()) {
            oldKeyStillLive = true;
            break;
          }
        }
      }
      if (oldKeyStillLive)
        continue;

      const std::uint64_t increase = nextWeight - oldWeight;
      if (increase <= remaining) {
        selected.emplace(address, *next);
        remaining -= increase;
      } else if (nextWeight != 0) {
        const std::uint64_t projectedWeight = oldWeight + remaining;
        if (projectedWeight >=
            core::ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS) {
          selected.emplace(address, withStake(*next, projectedWeight));
          remaining = 0;
        } else if (old != nullptr) {
          selected.emplace(address, *old);
        }
      }
    }

    core::ValidatorRegistry projected;
    for (const auto &[address, entry] : selected) {
      (void)address;
      if (!projected.restoreEntry(entry))
        throw std::logic_error("Bounded validator-set projection is invalid.");
    }
    if (projected.totalConsensusWeight() == 0)
      throw std::logic_error("Validator-set projection would remove all voting weight.");
    unsigned __int128 changedWeight = 0;
    for (const std::string &address : all) {
      const std::uint64_t before = previous.consensusWeightFor(address);
      const std::uint64_t after = projected.consensusWeightFor(address);
      changedWeight += before > after ? before - after : after - before;
    }
    if (changedWeight > budget)
      throw std::logic_error("Validator-set projection exceeded the churn budget.");
    return projected;
  }

  static bool recordNext(std::uint64_t finalizedHeight,
                         const core::ValidatorRegistry &target,
                         core::ValidatorSetHistory &history) {
    if (finalizedHeight == 0 ||
        finalizedHeight == std::numeric_limits<std::uint64_t>::max() ||
        !history.hasSet(finalizedHeight))
      return false;
    const core::ValidatorRegistry &previous = history.setAt(finalizedHeight);
    return history.recordSet(finalizedHeight + 1,
                             isBoundary(finalizedHeight)
                                 ? project(previous, target,
                                           finalizedHeight /
                                                   NODO_VALIDATOR_EPOCH_BLOCKS +
                                               1)
                                 : previous);
  }

private:
  static core::ValidatorRegistryEntry withStake(
      const core::ValidatorRegistryEntry &entry, std::uint64_t stake) {
    return core::ValidatorRegistryEntry(
        entry.registrationRecord(), entry.status(), entry.lastUpdatedAt(),
        stake, entry.jailUntilEpoch(), entry.exitRequestHeight(),
        entry.ownerAddress());
  }
};

} // namespace nodo::node

#endif
