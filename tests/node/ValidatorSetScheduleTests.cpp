#include "node/ValidatorSetSchedule.hpp"

#include "crypto/KeyPair.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>

namespace {

using namespace nodo;

std::string add(core::ValidatorRegistry &registry, const std::string &name,
                std::uint64_t weight, std::uint64_t activationEpoch = 1,
                const std::string &owner = "") {
  const auto key = crypto::KeyPair::createDeterministicBls12381KeyPair(
      "validator-set-schedule-" + name);
  const std::string address = key.address().value();
  const core::ValidatorRegistrationRecord record(
      address, key.publicKey(), activationEpoch, "schedule-" + name,
      1900000000);
  assert(registry.registerValidator(record, weight,
                                    owner.empty() ? address : owner).accepted());
  return address;
}

void testFrozenUntilFinalizedBoundary() {
  core::ValidatorRegistry old;
  const std::string a = add(old, "frozen-a", 4'000'000);
  add(old, "frozen-b", 4'000'000);
  add(old, "frozen-c", 4'000'000);
  core::ValidatorRegistry target = old;
  assert(target.updateStake(a, 7'000'000, 1900000001).success());

  core::ValidatorSetHistory history;
  assert(history.recordSet(1, old));
  assert(node::ValidatorSetSchedule::recordNext(1, target, history));
  assert(history.setAt(2).validatorSetRoot() == old.validatorSetRoot());
  assert(history.serialize().find("snapshotCount=1") != std::string::npos);
  assert(history.changesOnlyAtBoundaries(node::NODO_VALIDATOR_EPOCH_BLOCKS));

  core::ValidatorSetHistory boundary;
  assert(boundary.recordSet(node::NODO_VALIDATOR_EPOCH_BLOCKS, old));
  assert(node::ValidatorSetSchedule::recordNext(
      node::NODO_VALIDATOR_EPOCH_BLOCKS, target, boundary));
  assert(boundary.setAt(node::NODO_VALIDATOR_EPOCH_BLOCKS).validatorSetRoot() ==
         old.validatorSetRoot());
  assert(boundary.setAt(node::NODO_VALIDATOR_EPOCH_BLOCKS + 1)
             .consensusWeightFor(a) == 7'000'000);
  assert(boundary.serialize().find("snapshotCount=2") != std::string::npos);
  assert(boundary.changesOnlyAtBoundaries(node::NODO_VALIDATOR_EPOCH_BLOCKS));
  assert(!boundary.recordSet(node::NODO_VALIDATOR_EPOCH_BLOCKS + 1, old));

  core::ValidatorSetHistory invalidTiming;
  assert(invalidTiming.recordSet(1, old));
  assert(invalidTiming.recordSet(2, target));
  assert(!invalidTiming.changesOnlyAtBoundaries(
      node::NODO_VALIDATOR_EPOCH_BLOCKS));
}

void testChurnBudgetStagesLargeChangesAndIsSybilNeutral() {
  core::ValidatorRegistry old;
  add(old, "churn-a", 4'000'000);
  add(old, "churn-b", 4'000'000);
  add(old, "churn-c", 4'000'000);

  core::ValidatorRegistry concentrated = old;
  const std::string added = add(concentrated, "large-add", 6'000'000);
  const auto first = node::ValidatorSetSchedule::project(old, concentrated, 2);
  assert(first.consensusWeightFor(added) == 3'999'600);
  assert(first.totalConsensusWeight() == 15'999'600);
  const auto second = node::ValidatorSetSchedule::project(first, concentrated, 3);
  assert(second.consensusWeightFor(added) == 6'000'000);

  core::ValidatorRegistry split = old;
  const std::string splitA = add(split, "split-a", 3'000'000);
  const std::string splitB = add(split, "split-b", 3'000'000);
  const auto splitFirst = node::ValidatorSetSchedule::project(old, split, 2);
  assert(splitFirst.consensusWeightFor(splitA) +
             splitFirst.consensusWeightFor(splitB) <= 3'999'600);
  assert(splitFirst.totalConsensusWeight() <= first.totalConsensusWeight());
}

void testPendingIdentityDoesNotEnterConsensusSnapshot() {
  core::ValidatorRegistry old;
  add(old, "pending-a", 4'000'000);
  add(old, "pending-b", 4'000'000);
  add(old, "pending-c", 4'000'000);
  core::ValidatorRegistry target = old;
  const auto key = crypto::KeyPair::createDeterministicBls12381KeyPair(
      "validator-set-schedule-pending-new");
  const std::string address = key.address().value();
  const core::ValidatorRegistrationRecord record(
      address, key.publicKey(), 3, "pending-new", 1900000001);
  assert(target.registerPendingValidator(record, 1'000'000, address).accepted());
  const auto selected = node::ValidatorSetSchedule::project(old, target, 3);
  assert(!selected.hasValidator(address));
  assert(selected.validatorSetRoot() == old.validatorSetRoot());
}

void testActivationDelayAndOverflow() {
  const std::uint64_t length = node::NODO_VALIDATOR_EPOCH_BLOCKS;
  assert(node::ValidatorSetSchedule::activationEpoch(1) == 3);
  assert(node::ValidatorSetSchedule::activationHeight(1) == 2 * length + 1);
  assert(node::ValidatorSetSchedule::activationHeight(length) ==
         2 * length + 1);
  assert(node::ValidatorSetSchedule::activationHeight(length + 1) ==
         3 * length + 1);

  core::ValidatorRegistry old;
  add(old, "delay-a", 4'000'000);
  add(old, "delay-b", 4'000'000);
  add(old, "delay-c", 4'000'000);
  core::ValidatorRegistry target = old;
  const std::string delayed = add(target, "delayed", 1'000'000, 3);
  const auto premature = node::ValidatorSetSchedule::project(old, target, 2);
  assert(premature.consensusWeightFor(delayed) == 0);
  const auto mature = node::ValidatorSetSchedule::project(premature, target, 3);
  assert(mature.consensusWeightFor(delayed) == 1'000'000);

  bool overflowed = false;
  try {
    (void)node::ValidatorSetSchedule::activationHeight(
        std::numeric_limits<std::uint64_t>::max());
  } catch (const std::overflow_error &) {
    overflowed = true;
  }
  assert(overflowed);
}

void testRotationNeverAssignsOneOwnersStakeToBothKeys() {
  core::ValidatorRegistry old;
  const std::string owner = "rotation-owner";
  const std::string oldAddress = add(old, "rotation-old", 4'000'000, 1, owner);
  add(old, "rotation-other-a", 4'000'000);
  add(old, "rotation-other-b", 4'000'000);
  core::ValidatorRegistry target = old;
  const auto newKey = crypto::KeyPair::createDeterministicBls12381KeyPair(
      "validator-set-schedule-rotation-new");
  const std::string newAddress = newKey.address().value();
  const core::ValidatorRegistrationRecord newRecord(
      newAddress, newKey.publicKey(), 3, "rotation-new", 1900000001);
  assert(target.rotateValidatorKey(oldAddress, newRecord, 1900000001).success());

  const auto premature = node::ValidatorSetSchedule::project(old, target, 2);
  assert(premature.consensusWeightFor(oldAddress) == 4'000'000);
  assert(premature.consensusWeightFor(newAddress) == 0);
  const auto staging = node::ValidatorSetSchedule::project(premature, target, 3);
  assert(staging.consensusWeightFor(oldAddress) >= 1'000'000);
  assert(staging.consensusWeightFor(newAddress) == 0);
  const auto activated = node::ValidatorSetSchedule::project(staging, target, 4);
  assert(activated.consensusWeightFor(oldAddress) == 0);
  assert(activated.consensusWeightFor(newAddress) >= 1'000'000);
}

} // namespace

int main() {
  testFrozenUntilFinalizedBoundary();
  testChurnBudgetStagesLargeChangesAndIsSybilNeutral();
  testPendingIdentityDoesNotEnterConsensusSnapshot();
  testActivationDelayAndOverflow();
  testRotationNeverAssignsOneOwnersStakeToBothKeys();
  return 0;
}
