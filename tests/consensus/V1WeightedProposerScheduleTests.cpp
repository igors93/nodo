#include "../common/TestFramework.hpp"
#include "consensus/V1WeightedProposerSchedule.hpp"
#include "serialization/V1EncodingPrimitives.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

using Schedule = nodo::consensus::V1WeightedProposerSchedule;
using nodo::serialization::V1EncodingPrimitives;
using nodo::test::require;

Schedule::Digest filled(unsigned char byte) {
  Schedule::Digest digest{};
  digest.fill(byte);
  return digest;
}

std::vector<Schedule::Validator>
validators(std::initializer_list<std::uint64_t> weights) {
  std::vector<Schedule::Validator> result;
  unsigned char id = 1;
  for (const std::uint64_t weight : weights) {
    result.push_back({filled(id++), weight});
  }
  return result;
}

void testWeightedCycleAndFallback() {
  auto schedule = Schedule::create(6, filled(0xaa), validators({1, 3, 1, 1}));
  require(schedule.has_value(), "valid genesis proposer state must be created");
  const std::array<unsigned char, 6> primaries{2, 1, 2, 3, 4, 2};
  std::array<std::uint64_t, 5> counts{};
  for (std::uint64_t height = 1; height <= primaries.size(); ++height) {
    const auto primary = schedule->proposer(height, 0);
    require(primary && *primary == filled(primaries[height - 1]),
            "stable weighted primary sequence must match the reference vector");
    ++counts[primaries[height - 1]];
    if (height == 1) {
      std::set<Schedule::Digest> roundOrder;
      for (std::uint64_t round = 0; round < 4; ++round) {
        roundOrder.insert(*schedule->proposer(height, round));
      }
      require(roundOrder.size() == 4 &&
                  schedule->proposer(height, 4) == primary &&
                  schedule->proposer(height,
                                     std::numeric_limits<std::uint64_t>::max()) ==
                      filled(4),
              "fallback rounds enumerate all validators without a long loop");
    }
    require(schedule->finalize(height, 0, *primary),
            "valid finalized primary must advance exactly one height");
  }
  require(counts == std::array<std::uint64_t, 5>{0, 1, 3, 1, 1} &&
              schedule->proposer(7, 0) == filled(2),
          "stable cycle must give exactly the configured integer weights");
}

void testBackupFinalityCannotBuyPriority() {
  auto primary = Schedule::create(10, filled(0xaa), validators({1, 3, 1, 1}));
  auto backup = Schedule::create(10, filled(0xaa), validators({1, 3, 1, 1}));
  require(primary && backup, "both reference schedules must be valid");
  const auto initial = primary->stateValueBytes();
  require(!primary->finalize(1, 0, filled(3)) &&
              primary->stateValueBytes() == initial &&
              !primary->proposer(2, 0),
          "wrong signer cannot mutate state or skip a height");
  require(primary->finalize(1, 0, filled(2)) &&
              backup->finalize(1, 2, filled(3)) &&
              primary->stateValueBytes() == backup->stateValueBytes() &&
              primary->proposer(2, 0) == backup->proposer(2, 0),
          "backup finality cannot change the next primary assignment");

  const auto bytes = primary->stateValueBytes();
  require(bytes.size() == 52 + 4 * 52 && bytes[31] == 0xaa &&
              bytes[39] == 1 && bytes[47] == 6 && bytes[51] == 4 &&
              bytes[55] == 48 && bytes[56] == 1 && bytes[103] == 1 &&
              bytes[108] == 2 && bytes[155] == 0xfd,
          "canonical state value must encode sorted IDs and signed i128 priority");
  require(V1EncodingPrimitives::hex(
              V1EncodingPrimitives::hash("STATE-VALUE", bytes)) ==
              "a178e9d58e90af1ced3dda5bd114d00b1cc836570fa0032792bb36a8b319768a",
          "independent canonical state commitment vector must match");
}

void testEpochRebaseAndInvalidTransitions() {
  auto schedule = Schedule::create(2, filled(0xaa), validators({1, 3, 1, 1}));
  require(schedule.has_value(), "valid genesis proposer state must be created");
  auto replacement = validators({1, 3, 1, 1});
  replacement[3].id = filled(5);
  Schedule::NextSet next{filled(0xbb), replacement};
  const auto before = schedule->stateValueBytes();
  require(!schedule->finalize(1, 0, filled(2), next) &&
              schedule->stateValueBytes() == before,
          "a set change outside an epoch boundary must be atomic and invalid");
  require(schedule->finalize(1, 0, filled(2)),
          "the old set must remain active in the first height");
  const auto second = schedule->proposer(2, 0);
  require(second && schedule->finalize(2, 0, *second, next) &&
              schedule->proposer(3, 0) == filled(2) &&
              schedule->stateValueBytes()[31] == 0xbb,
          "boundary rebase must carry old priorities and penalize new IDs");
  const auto state = schedule->stateValueBytes();
  require(state.size() == 260 && state[39] == 2 &&
              state[103] == 0xff && state[155] == 3 &&
              state[207] == 5 && state[259] == 0xf9 &&
              schedule->totalWeight() == 6,
          "boundary rebase must match the signed [-1,3,5,-7] vector");

  auto duplicate = replacement;
  duplicate[3].id = duplicate[2].id;
  auto overflow = validators({std::numeric_limits<std::uint64_t>::max(),
                              1, 1, 1});
  require(!Schedule::create(2, filled(0xaa), duplicate) &&
              !Schedule::create(2, filled(0xaa), overflow) &&
              !Schedule::create(0, filled(0xaa), replacement),
          "duplicate IDs, weight overflow and zero epoch length fail closed");
}

void testSplittingDoesNotIncreaseAggregatePrimaryWeight() {
  auto unsplit = Schedule::create(20, filled(0xaa), validators({4, 1, 1, 1}));
  auto split = Schedule::create(20, filled(0xaa), validators({2, 2, 1, 1, 1}));
  require(unsplit && split, "both linear-stake schedules must be valid");
  unsigned unsplitCount = 0;
  unsigned splitCount = 0;
  for (std::uint64_t height = 1; height <= 7; ++height) {
    const auto a = unsplit->proposer(height, 0);
    const auto b = split->proposer(height, 0);
    require(a && b, "every stable height must have a primary");
    unsplitCount += *a == filled(1);
    splitCount += *b == filled(1) || *b == filled(2);
    require(unsplit->finalize(height, 0, *a) &&
                split->finalize(height, 0, *b),
            "both schedules must advance deterministically");
  }
  require(unsplitCount == 4 && splitCount == 4,
          "splitting four stake units must not create extra primary slots");
}

void testScalingAndUnchangedBoundary() {
  auto scaled = Schedule::create(3, filled(0xaa),
                                 validators({1, 1, 1, 100}));
  auto unchanged = Schedule::create(3, filled(0xaa),
                                    validators({1, 1, 1, 100}));
  require(scaled && unchanged, "both priority states must initialize");
  for (std::uint64_t height = 1; height <= 3; ++height) {
    const auto a = scaled->proposer(height, 0);
    const auto b = unchanged->proposer(height, 0);
    require(a && b && a == b, "both histories must select the same primary");
    if (height == 3) {
      require(scaled->finalize(
                  height, 0, *a,
                  Schedule::NextSet{filled(0xbb), validators({1, 1, 1, 1})}) &&
                  unchanged->finalize(
                      height, 0, *b,
                      Schedule::NextSet{filled(0xaa),
                                        validators({1, 1, 1, 100})}),
              "changed weights rebase; an unchanged boundary preserves credits");
    } else {
      require(scaled->finalize(height, 0, *a) &&
                  unchanged->finalize(height, 0, *b),
              "pre-boundary priorities must advance identically");
    }
  }
  const auto scaledBytes = scaled->stateValueBytes();
  const auto unchangedBytes = unchanged->stateValueBytes();
  require(scaledBytes.back() == 0xfc &&
              scaled->totalWeight() == 4 &&
              unchanged->totalWeight() == 103 &&
              scaledBytes != unchangedBytes,
          "large old priorities must scale and center under the new weight");
}

void testSingleSurvivorStillChangesState() {
  auto schedule = Schedule::create(1, filled(0xaa), validators({1, 1, 1, 1}));
  require(schedule.has_value(), "four validators are required at genesis");
  const auto first = schedule->proposer(1, 0);
  require(first && schedule->finalize(
                       1, 0, *first,
                       Schedule::NextSet{filled(0xbb), validators({1})}),
          "a boundary can deterministically leave one active survivor");
  const auto before = schedule->stateValueBytes();
  require(schedule->finalize(2, 0, filled(1)) &&
              schedule->stateValueBytes() != before,
          "height commitment makes even a one-validator transition nonempty");
}

void testStableWeightQuotasAcrossManySets() {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    const std::size_t count = 4 + seed % 5;
    std::vector<Schedule::Validator> set;
    std::vector<std::uint64_t> assigned(count);
    std::uint64_t totalWeight = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint64_t weight = 1 + ((seed * 17 + i * 23) % 19);
      set.push_back({filled(static_cast<unsigned char>(i + 1)), weight});
      totalWeight += weight;
    }
    auto schedule = Schedule::create(totalWeight + 1, filled(0xaa), set);
    require(schedule.has_value(), "generated sorted validator set must be valid");
    for (std::uint64_t height = 1; height <= totalWeight; ++height) {
      const auto primary = schedule->proposer(height, 0);
      require(primary.has_value(), "every stable height has a primary");
      ++assigned[(*primary)[0] - 1];
      require(schedule->finalize(height, 0, *primary),
              "every generated primary advances exactly once");
    }
    for (std::size_t i = 0; i < count; ++i) {
      require(assigned[i] == set[i].weight,
              "a stable full-weight cycle must assign exact primary quotas");
    }
  }
}

} // namespace

int main() {
  try {
    testWeightedCycleAndFallback();
    testBackupFinalityCannotBuyPriority();
    testEpochRebaseAndInvalidTransitions();
    testSplittingDoesNotIncreaseAggregatePrimaryWeight();
    testScalingAndUnchangedBoundary();
    testSingleSurvivorStillChangesState();
    testStableWeightQuotasAcrossManySets();
    std::cout << "V1 weighted proposer schedule tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 weighted proposer schedule tests failed: "
              << error.what() << '\n';
    return 1;
  }
}
