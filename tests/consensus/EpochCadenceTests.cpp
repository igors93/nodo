#include "../common/TestFramework.hpp"
#include "consensus/EpochCadence.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

using nodo::consensus::EpochCadence;
using nodo::test::require;

void testGenesisParameterBounds() {
  require(!EpochCadence::create(0, 30), "zero epoch length must fail");
  require(!EpochCadence::create(100, 0), "zero target time must fail");
  require(!EpochCadence::create(1, 301), "excessive target time must fail");
  require(!EpochCadence::create(287, 300),
          "an epoch shorter than one nominal day must fail");
  require(!EpochCadence::create(2017, 300),
          "an epoch longer than seven nominal days must fail");
  require(!EpochCadence::create(std::numeric_limits<std::uint64_t>::max(), 300),
          "multiplication overflow must fail before calculation");
  const auto week = EpochCadence::create(2016, 300);
  require(week && week->nominalEpochSeconds() == 604800,
          "the seven-day nominal bound is inclusive");
  const auto day = EpochCadence::create(288, 300);
  require(day && day->nominalEpochSeconds() == 86400,
          "the one-day nominal bound is inclusive");
}

void testParameterDrivenBoundaries() {
  const auto fast = EpochCadence::create(288, 300);
  const auto slow = EpochCadence::create(1440, 60);
  require(fast && slow, "fixture parameters must be valid");
  require(!fast->epochForHeight(0) && !fast->isFinalBlockOfEpoch(0),
          "genesis must not be a consensus epoch or boundary");
  require(fast->epochForHeight(1) == 0 &&
              fast->epochForHeight(288) == 0 &&
              fast->epochForHeight(289) == 1 &&
              fast->epochForHeight(576) == 1,
          "epoch membership must be zero-based and height-defined");
  require(fast->firstHeight(0) == 1 && fast->lastHeight(0) == 288 &&
              fast->firstHeight(1) == 289 && fast->lastHeight(1) == 576 &&
              fast->isFinalBlockOfEpoch(288) &&
              !fast->isFinalBlockOfEpoch(289),
          "epoch edges and finalization boundary must agree");
  require(slow->epochForHeight(1441) == 1 &&
              slow->isFinalBlockOfEpoch(1440) &&
              !slow->isFinalBlockOfEpoch(1441) &&
              slow->nominalEpochSeconds() == 86400,
          "genesis parameters must change cadence without recompilation");
}

void testCheckedHeightAndTime() {
  const auto cadence = EpochCadence::create(288, 300);
  require(cadence.has_value(), "fixture cadence must be valid");
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  require(!cadence->firstHeight(maximum) &&
              !cadence->lastHeight(maximum),
          "epoch height overflow must fail closed");
  require(cadence->earliestHeaderTime(100, 0) == 100 &&
              cadence->earliestHeaderTime(100, 1) == 400 &&
              cadence->earliestHeaderTime(100, 288) == 86500,
          "BFT time floor must advance by target seconds per height");
  require(!cadence->earliestHeaderTime(0, 1) &&
              !cadence->earliestHeaderTime(-1, 1) &&
              !cadence->earliestHeaderTime(
                  std::numeric_limits<std::int64_t>::max() - 299, 1) &&
              !cadence->earliestHeaderTime(100, maximum),
          "invalid genesis time or signed-time overflow must fail closed");
}

} // namespace

int main() {
  try {
    testGenesisParameterBounds();
    testParameterDrivenBoundaries();
    testCheckedHeightAndTime();
    std::cout << "Epoch cadence tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Epoch cadence tests failed: " << error.what() << '\n';
    return 1;
  }
}
