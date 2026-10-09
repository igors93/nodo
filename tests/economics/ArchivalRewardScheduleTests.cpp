#include "../common/TestFramework.hpp"

#include "config/HistoryParameters.hpp"
#include "economics/ArchivalRewardSchedule.hpp"
#include "economics/EpochEmissionPolicy.hpp"
#include "economics/ProtectionWorkType.hpp"

#include <iostream>
#include <stdexcept>

using nodo::test::require;
using namespace nodo;
using namespace nodo::economics;

namespace {

const config::HistoryParameters &parameters() {
  static const config::HistoryParameters params =
      config::HistoryParameters::developmentLocal();
  return params;
}

utils::Amount cap() {
  return EpochEmissionPolicy::developmentDefaultPolicy().calculateNewEmissionCap(
      utils::Amount::fromNodo(1000000000));
}

ArchivalRewardSlot slot(const std::string &provider, std::uint64_t segment,
                        std::uint32_t issued, std::uint32_t passed,
                        bool fraud = false) {
  return ArchivalRewardSlot{provider, "operator-" + provider, segment, issued,
                            passed, fraud};
}

utils::Amount rewardOf(const ArchivalRewardSettlement &settlement,
                       const std::string &provider) {
  for (const auto &reward : settlement.rewards) {
    if (reward.providerId == provider) {
      return reward.amount;
    }
  }
  return utils::Amount();
}

ArchivalRewardInputs baseInputs() {
  ArchivalRewardInputs inputs;
  inputs.epoch = 7;
  inputs.epochEmissionCap = cap();
  inputs.segments = {{0, 1'000'000}, {1, 1'000'000}, {2, 1'000'000}};
  inputs.reliabilityBasisPoints = {{"a", 10000}, {"b", 10000}, {"c", 10000}};
  return inputs;
}

void testBudgetIsBoundedByMonetaryPolicy() {
  ArchivalRewardInputs inputs = baseInputs();
  inputs.slots = {slot("a", 0, 4, 4), slot("b", 1, 4, 4), slot("c", 2, 4, 4)};
  const ArchivalRewardSettlement settlement =
      ArchivalRewardSchedule::settle(parameters(), inputs);
  require(settlement.budget ==
              ProtectionBudgetSplit::withArchival(
                  parameters().archiveRewardShareBasisPoints())
                  .slice(cap(), ProtectionWorkType::HISTORICAL_ARCHIVAL),
          "the archival budget is a slice of the emission cap");
  require(settlement.budget <= cap() && settlement.distributed <= settlement.budget &&
              settlement.distributed + settlement.unminted == settlement.budget,
          "rewards never exceed the budget and the rest is never minted");
  require(settlement.isValid(), "settlement invariants hold");
  std::string reason;
  require(ArchivalRewardSchedule::audit(parameters(), inputs, settlement, reason),
          "an honest settlement passes the audit: " + reason);

  ArchivalRewardSettlement inflated = settlement;
  inflated.rewards.front().amount =
      inflated.rewards.front().amount + utils::Amount::fromRawUnits(1);
  inflated.distributed = inflated.distributed + utils::Amount::fromRawUnits(1);
  inflated.unminted = inflated.unminted - utils::Amount::fromRawUnits(1);
  require(!ArchivalRewardSchedule::audit(parameters(), inputs, inflated, reason),
          "an inflated reward fails the supply audit");

  const ProtectionBudgetSplit split = ProtectionBudgetSplit::withArchival(1000);
  require(split.consensusBasisPoints() == 9000 && split.isValid() &&
              !ProtectionBudgetSplit::withArchival(5000).isValid(),
          "consensus keeps the majority of the protection budget");
  require(ProtectionBudgetSplit::consensusOnly().slice(
              cap(), ProtectionWorkType::HISTORICAL_ARCHIVAL)
              .isZero(),
          "today's protocol budgets nothing for archival");
}

void testNoRewardWithoutProof() {
  ArchivalRewardInputs inputs = baseInputs();
  inputs.slots = {slot("a", 0, 4, 0), slot("b", 1, 0, 0), slot("c", 2, 4, 4, true)};
  const ArchivalRewardSettlement settlement =
      ArchivalRewardSchedule::settle(parameters(), inputs);
  require(settlement.rewards.empty() && settlement.distributed.isZero() &&
              settlement.unminted == settlement.budget,
          "missed, unchallenged and fraudulent slots earn nothing");

  inputs.slots = {slot("a", 0, 10, 7)}; // 70% < 80% availability floor
  require(ArchivalRewardSchedule::settle(parameters(), inputs).rewards.empty(),
          "availability below the floor earns nothing");
}

void testConstantRateBelowTarget() {
  ArchivalRewardInputs alone = baseInputs();
  alone.slots = {slot("a", 0, 4, 4)};
  ArchivalRewardInputs together = baseInputs();
  together.slots = {slot("a", 0, 4, 4), slot("b", 1, 4, 4)};
  require(rewardOf(ArchivalRewardSchedule::settle(parameters(), alone), "a") ==
              rewardOf(ArchivalRewardSchedule::settle(parameters(), together), "a"),
          "below the replication target a provider's reward does not shrink "
          "when others join");
  ArchivalRewardInputs partial = baseInputs();
  partial.slots = {slot("a", 0, 10, 9)};
  require(rewardOf(ArchivalRewardSchedule::settle(parameters(), partial), "a") <
              rewardOf(ArchivalRewardSchedule::settle(parameters(), alone), "a"),
          "availability scales the reward");
  ArchivalRewardInputs newcomer = alone;
  newcomer.reliabilityBasisPoints["a"] =
      parameters().archiveReliabilityFloorBasisPoints();
  require(rewardOf(ArchivalRewardSchedule::settle(parameters(), newcomer), "a") <
              rewardOf(ArchivalRewardSchedule::settle(parameters(), alone), "a"),
          "consistent providers earn more than newcomers");
  ArchivalRewardInputs bigger = alone;
  bigger.segments[0].totalBytes = 4'000'000;
  require(rewardOf(ArchivalRewardSchedule::settle(parameters(), bigger), "a") >
              rewardOf(ArchivalRewardSchedule::settle(parameters(), alone), "a"),
          "preserving more bytes earns more");
}

void testScarcityPaysMore() {
  ArchivalRewardInputs inputs = baseInputs();
  inputs.slots = {slot("a", 0, 4, 4), slot("b", 0, 4, 4),
                  slot("d", 0, 4, 4), slot("c", 2, 4, 4)};
  const ArchivalRewardSettlement settlement =
      ArchivalRewardSchedule::settle(parameters(), inputs);
  const auto common = rewardOf(settlement, "a").rawUnits();
  const auto scarce = rewardOf(settlement, "c").rawUnits();
  // Same bytes; segment 2 has one proven replica (2.5x) vs three (1.0x).
  require(scarce > common, "a scarce segment pays more");
  require(scarce * 10000 / common >= 24990 && scarce * 10000 / common <= 25010,
          "the scarcity tier sets the multiplier");
}

void testUnassignedAndSybilSlotsRejected() {
  ArchivalRewardInputs inputs;
  inputs.epoch = 1;
  inputs.epochEmissionCap = cap();
  inputs.segments = {{0, 1000}};
  for (int index = 0; index < 8; ++index) {
    const std::string id = "p" + std::to_string(index);
    inputs.slots.push_back(slot(id, 0, 4, 4));
    inputs.reliabilityBasisPoints[id] = 10000;
  }
  bool rejected = false;
  try {
    (void)ArchivalRewardSchedule::settle(parameters(), inputs);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "extra identities cannot claim unassigned replica slots");

  inputs.slots = {slot("a", 0, 4, 4),
                  ArchivalRewardSlot{"b", "operator-a", 0, 4, 4, false}};
  rejected = false;
  try {
    (void)ArchivalRewardSchedule::settle(parameters(), inputs);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "one economic operator cannot claim two segment replicas");
}

void testWorkTaxonomy() {
  require(protectionWorkTypeFor(ValidationWorkType::CONSENSUS_VOTE) ==
                  ProtectionWorkType::FINALITY_PARTICIPATION &&
              protectionWorkTypeFor(ValidationWorkType::VALIDATE_BLOCK) ==
                  ProtectionWorkType::CONSENSUS_PROTECTION &&
              protectionWorkTypeFor(ValidationWorkType::RESPOND_INTEGRITY_CHALLENGE) ==
                  ProtectionWorkType::DATA_AVAILABILITY &&
              !protectionWorkTypeFor(ValidationWorkType::UNKNOWN).has_value(),
          "existing work records map onto Proof-of-Protection pillars");
  require(protectionWorkTypeToString(ProtectionWorkType::HISTORICAL_ARCHIVAL) ==
              "HISTORICAL_ARCHIVAL",
          "historical archival is a protection pillar");
}

} // namespace

int main() {
  try {
    testBudgetIsBoundedByMonetaryPolicy();
    testNoRewardWithoutProof();
    testConstantRateBelowTarget();
    testScarcityPaysMore();
    testUnassignedAndSybilSlotsRejected();
    testWorkTaxonomy();
    std::cout << "Archival reward schedule tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Archival reward schedule tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
