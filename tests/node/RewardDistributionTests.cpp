#include "node/RewardDistribution.hpp"

#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::node::RewardDistribution;
using nodo::node::RewardDistributionCalculator;
using nodo::utils::Amount;

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testSingleValidatorDistributionLocksTenPercent() {
  const std::vector<RewardDistribution> rewards =
      RewardDistributionCalculator::buildForVoters(Amount::fromRawUnits(100),
                                                   {"validator-a"}, 1);

  requireCondition(rewards.size() == 1U,
                   "Single validator reward should produce one distribution.");

  requireCondition(
      rewards[0].validatorAddress() == "validator-a" &&
          rewards[0].blockHeight() == 1U &&
          rewards[0].totalReward().rawUnits() == 100 &&
          rewards[0].liquidReward().rawUnits() == 90 &&
          rewards[0].lockedReward().rawUnits() == 10 &&
          rewards[0].reason() ==
              RewardDistributionCalculator::BLOCK_FINALIZATION_FEE_REASON &&
          rewards[0].isValid(),
      "Single validator reward should split into 90 liquid and 10 locked raw "
      "units.");
}

void testRemainderIsDistributedDeterministically() {
  const std::vector<RewardDistribution> rewards =
      RewardDistributionCalculator::buildForVoters(
          Amount::fromRawUnits(101),
          {"validator-c", "validator-a", "validator-b"}, 7);

  requireCondition(
      rewards.size() == 3U,
      "Three validators should receive three reward distributions.");

  requireCondition(
      rewards[0].validatorAddress() == "validator-a" &&
          rewards[1].validatorAddress() == "validator-b" &&
          rewards[2].validatorAddress() == "validator-c",
      "Reward recipients should be canonicalized by validator address.");

  requireCondition(rewards[0].totalReward().rawUnits() == 34 &&
                       rewards[1].totalReward().rawUnits() == 34 &&
                       rewards[2].totalReward().rawUnits() == 33,
                   "Reward remainder should be assigned deterministically.");

  requireCondition(
      RewardDistributionCalculator::totalReward(rewards).rawUnits() == 101,
      "Reward distributions should sum to the total reward.");
}

void testRejectsInvalidInputs() {
  bool rejectedEmptyRecipient = false;

  try {
    (void)RewardDistributionCalculator::buildForVoters(
        Amount::fromRawUnits(100), {""}, 1);
  } catch (const std::invalid_argument &) {
    rejectedEmptyRecipient = true;
  }

  requireCondition(
      rejectedEmptyRecipient,
      "Reward calculator should reject an empty validator address.");

  bool rejectedGenesisHeight = false;

  try {
    (void)RewardDistributionCalculator::buildForVoters(
        Amount::fromRawUnits(100), {"validator-a"}, 0);
  } catch (const std::invalid_argument &) {
    rejectedGenesisHeight = true;
  }

  requireCondition(
      rejectedGenesisHeight,
      "Reward calculator should reject genesis height reward distribution.");
}

} // namespace

int main() {
  try {
    testSingleValidatorDistributionLocksTenPercent();
    testRemainderIsDistributedDeterministically();
    testRejectsInvalidInputs();

    std::cout << "Nodo reward distribution tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo reward distribution tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
