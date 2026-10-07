#include "consensus/ConsensusWeight.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testLinearWeight() {
  requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(0) == 0,
                   "zero stake has zero weight");
  requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(1) == 1,
                   "one raw unit has one unit of weight");
  requireCondition(
      nodo::consensus::ConsensusWeight::weightFromStake(1'000'000ULL) ==
          1'000'000ULL,
      "weight must equal the exact stake in raw units");
  requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(
                       1'000'000'000ULL) == 1'000'000'000ULL,
                   "large stakes must not be truncated");
  requireCondition(nodo::consensus::ConsensusWeight::weightFromStake(
                       18'446'744'073'709'551'615ULL) ==
                       18'446'744'073'709'551'615ULL,
                   "the weight function must not silently cap large input");
}

void testSplittingNeverCreatesVotingPower() {
  constexpr std::uint64_t stake = 1'000'000'003ULL;
  for (std::uint64_t validatorCount : {2ULL, 3ULL, 7ULL, 101ULL}) {
    const std::uint64_t base = stake / validatorCount;
    const std::uint64_t remainder = stake % validatorCount;
    std::uint64_t splitWeight = 0;
    for (std::uint64_t index = 0; index < validatorCount; ++index) {
      splitWeight += nodo::consensus::ConsensusWeight::weightFromStake(
          base + (index < remainder ? 1 : 0));
    }
    requireCondition(
        splitWeight == nodo::consensus::ConsensusWeight::weightFromStake(stake),
        "splitting stake across validators must preserve total voting power");
  }
}

} // namespace

int main() {
  try {
    testLinearWeight();
    testSplittingNeverCreatesVotingPower();
    std::cout << "ConsensusWeight linear-stake tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ConsensusWeight tests failed: " << error.what() << "\n";
    return 1;
  }
}
