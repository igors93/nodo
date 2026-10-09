#include "../common/TestFramework.hpp"
#include "consensus/V1LivenessAccountability.hpp"
#include "serialization/V1EncodingPrimitives.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

using nodo::consensus::V1LivenessAccountability;
using nodo::test::require;
using Digest = V1LivenessAccountability::Digest;
using Validator = V1LivenessAccountability::Validator;
using Commit = V1LivenessAccountability::ParentCommit;

Digest id(unsigned char suffix) {
  Digest result{};
  result.back() = suffix;
  return result;
}

std::vector<Validator> validators() {
  return {{id(1), 4}, {id(2), 3}, {id(3), 2}, {id(4), 1}};
}

Commit commit(std::uint64_t height, std::vector<Digest> signers) {
  return {height, id(99), std::move(signers)};
}

void testWindowAndBoundary() {
  auto state = V1LivenessAccountability::create(288, id(99), validators());
  require(state.has_value(), "admissible genesis must construct a window");
  require(state->stateValueBytes().size() == 276,
          "canonical liveness state size must include every entry");
  const auto genesisHash = nodo::serialization::V1EncodingPrimitives::hash(
      "STATE-VALUE", state->stateValueBytes());
  constexpr std::array<unsigned char, 32> expectedHash{
      0x6a, 0x87, 0xda, 0x18, 0x30, 0x6f, 0xbc, 0xb8,
      0x8a, 0x69, 0xa4, 0xc7, 0x05, 0xac, 0xad, 0xc8,
      0x73, 0x46, 0x0e, 0xb4, 0xcb, 0xe5, 0xeb, 0x59,
      0x1a, 0x13, 0x8d, 0xfb, 0xfa, 0x01, 0x96, 0xb0};
  require(genesisHash == expectedHash,
          "domain-separated liveness state vector must match the independent digest");
  require(state->advance(1, std::nullopt).has_value(),
          "height one has no parent QC");
  for (std::uint64_t height = 2; height <= 288; ++height) {
    const auto before = state->stateValueBytes();
    if (height == 2) {
      require(!state->advance(height, commit(1, {id(1), id(1)})),
              "duplicate signer must fail");
      require(!state->advance(height, commit(1, {id(2), id(1)})),
              "unsorted signer list must fail");
      require(!state->advance(height, commit(1, {id(1), id(3)})),
              "subquorum signer list must fail");
      require(!state->advance(height, commit(2, {id(1), id(2)})),
              "wrong parent height must fail");
      auto wrongRoot = commit(1, {id(1), id(2)});
      wrongRoot.setRoot = id(98);
      require(!state->advance(height, wrongRoot),
              "wrong historical set root must fail");
      require(state->stateValueBytes() == before && state->nextHeight() == 2,
              "rejected commits must leave state unchanged");
    }
    const auto result = state->advance(height,
                                       commit(height - 1, {id(1), id(2)}));
    require(result.has_value(), "certified parent QC must advance exactly once");
    if (height < 288) {
      require(!result->boundary && result->assessments.empty(),
              "no early inactivity assessment is allowed");
    } else {
      require(result->boundary && result->assessments.size() == 4,
              "boundary must assess every active validator");
      require(!result->assessments[0].inactivityCandidate &&
                  !result->assessments[1].inactivityCandidate &&
                  result->assessments[2].inactivityCandidate &&
                  result->assessments[3].inactivityCandidate,
              "missing QC signatures create only reversible candidates");
      require(result->assessments[0].signedHeights == 287 &&
                  result->assessments[2].signedHeights == 0 &&
                  result->assessments[2].observedHeights == 287,
              "counts must reflect exactly L-1 finalized parent QCs");
    }
  }
  require(!state->advance(288, commit(287, {id(1), id(2)})),
          "the same height cannot be observed twice");
  require(state->advance(289, commit(288, {id(1), id(2)})).has_value(),
          "the boundary QC is deliberately skipped by the next epoch");
  require(state->advance(290, commit(289, {id(1), id(2)})).has_value(),
          "new epoch observations resume after its first height");
}

void testChurnAndInvalidTransitions() {
  auto state = V1LivenessAccountability::create(288, id(99), validators());
  require(state.has_value(), "fixture must construct");
  require(!state->advance(1, commit(0, {id(1), id(2)})),
          "genesis cannot carry a parent QC");
  require(!state->advance(1, std::nullopt,
                          V1LivenessAccountability::NextSet{id(100), validators()}),
          "validator sets cannot change outside boundaries");
  require(state->advance(1, std::nullopt).has_value(),
          "genesis transition must remain possible after rejection");
  for (std::uint64_t height = 2; height < 288; ++height) {
    require(state->advance(height, commit(height - 1,
                                           {id(1), id(2), id(3), id(4)})).has_value(),
            "fully signed parent must advance");
  }
  const auto before = state->stateValueBytes();
  auto duplicate = validators();
  duplicate.back().id = id(3);
  require(!state->advance(288,
                          commit(287, {id(1), id(2), id(3), id(4)}),
                          V1LivenessAccountability::NextSet{id(100), duplicate}),
          "duplicate next-set identities must fail atomically");
  require(state->stateValueBytes() == before,
          "invalid boundary cannot alter counters or root");
  const auto transition = state->advance(
      288, commit(287, {id(1), id(2), id(3), id(4)}),
      V1LivenessAccountability::NextSet{id(100),
                                        {{id(1), 4}, {id(2), 3},
                                         {id(3), 2}, {id(5), 1}}});
  require(transition && transition->boundary &&
              !transition->assessments[3].inactivityCandidate,
          "fully signed epoch has no inactivity candidate");
  auto wrongBoundaryRoot = commit(288, {id(1), id(2), id(3), id(4)});
  wrongBoundaryRoot.setRoot = id(100);
  require(!state->advance(289, wrongBoundaryRoot),
          "first child must identify the old boundary QC root");
  require(!state->advance(289, commit(288, {id(1), id(3)})),
          "skipped boundary QC still needs a valid old-set quorum");
  require(state->advance(289, commit(288, {id(1), id(2), id(3), id(4)})).has_value(),
          "old boundary QC root must be accepted for skip-only processing");
  require(state->advance(290, Commit{289, id(100), {id(1), id(2)}}).has_value(),
          "new set must authorize following parent QCs");
}

void testMalformedSets() {
  auto zero = validators();
  zero.front().weight = 0;
  require(!V1LivenessAccountability::create(288, id(99), zero),
          "zero weight must fail");
  auto overflow = validators();
  overflow.front().weight = std::numeric_limits<std::uint64_t>::max();
  require(!V1LivenessAccountability::create(288, id(99), overflow),
          "total weight overflow must fail");
  require(!V1LivenessAccountability::create(287, id(99), validators()),
          "epoch length below the cadence minimum must fail");
  std::vector<Validator> tooMany;
  tooMany.reserve(9'620);
  for (std::uint32_t value = 0; value < 9'620; ++value) {
    Digest validatorId{};
    validatorId[30] = static_cast<unsigned char>(value >> 8);
    validatorId[31] = static_cast<unsigned char>(value);
    tooMany.push_back({validatorId, 1});
  }
  require(!V1LivenessAccountability::create(288, id(99), tooMany),
          "the canonical validator-set byte cap also bounds liveness state");
  tooMany.pop_back();
  const auto maximum = V1LivenessAccountability::create(288, id(99), tooMany);
  require(maximum && maximum->stateValueBytes().size() == 500'256,
          "maximum active set has the documented bounded state value");
}

void testExactParticipationThreshold() {
  for (const std::uint64_t signedCount : {215ULL, 216ULL}) {
    auto state = V1LivenessAccountability::create(289, id(99), validators());
    require(state && state->advance(1, std::nullopt),
            "threshold fixture must begin at height one");
    for (std::uint64_t height = 2; height <= 289; ++height) {
      std::vector<Digest> signers{id(1), id(2)};
      if (height - 2 < signedCount) {
        signers.push_back(id(3));
      }
      const auto result = state->advance(height,
                                         commit(height - 1, std::move(signers)));
      require(result.has_value(), "all threshold QCs must certify");
      if (height == 289) {
        require(result->assessments[2].signedHeights == signedCount &&
                    result->assessments[2].observedHeights == 288 &&
                    result->assessments[2].inactivityCandidate ==
                        (signedCount == 215),
                "exactly 75 percent passes and one fewer signature fails");
      }
    }
  }
}

} // namespace

int main() {
  try {
    testWindowAndBoundary();
    testChurnAndInvalidTransitions();
    testMalformedSets();
    testExactParticipationThreshold();
    std::cout << "V1 liveness accountability tests passed.\n";
  } catch (const std::exception &error) {
    std::cerr << "V1 liveness accountability tests failed: " << error.what()
              << '\n';
    return 1;
  }
  return 0;
}
