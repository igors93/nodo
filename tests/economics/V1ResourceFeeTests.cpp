#include "../common/TestFramework.hpp"
#include "economics/V1ResourceFee.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using nodo::economics::V1ResourceFee;
using nodo::test::require;

V1ResourceFee::Parameters parameters() {
  return {262'144, 1'048'576, 524'288, 1'048'576, 1, 1};
}

void testParameterBounds() {
  auto value = parameters();
  require(V1ResourceFee::validParameters(value),
          "the reference parameter set must be admissible");
  value.maxTxUnits = 524'289;
  require(!V1ResourceFee::validParameters(value),
          "a transaction cannot exceed half a block's unit limit");
  value = parameters();
  value.maxBlockBytes = value.maxTxBytes + 15;
  require(!V1ResourceFee::validParameters(value),
          "a maximal transaction must fit in an empty body");
  value = parameters();
  value.feePerUnitFloor = 0;
  require(!V1ResourceFee::validParameters(value),
          "a zero minimum unit price enables free-byte spam");
  value = parameters();
  value.feePerUnitFloor = std::numeric_limits<std::uint64_t>::max();
  require(!V1ResourceFee::validParameters(value),
          "genesis parameters must not overflow a maximal required fee");
}

void testMeterAndFee() {
  const auto policy = parameters();
  const V1ResourceFee::TransactionWork work{1, 249, 92, 1, 1, 0};
  const auto units = V1ResourceFee::transactionUnits(policy, work);
  require(units == 2'005,
          "transaction bytes, receipt, signature, lot work and type cost must agree");
  require(V1ResourceFee::minimumFee(policy, 1, *units) == 2'006,
          "the base fee and per-unit floor must be charged exactly");
  require(V1ResourceFee::evidenceUnits(421) == 2'981,
          "two evidence signatures and validation work must be charged");
  require(V1ResourceFee::systemRecordUnits({100, 156, 2}) == 1'408,
          "system records must account for their receipt and effects");
  require(V1ResourceFee::systemRecordUnits({100, 65'532, 2'045}) ==
              197'536,
          "the largest complete system receipt is an inclusive boundary");
  constexpr std::array<std::uint64_t, 13> surcharges = {
      256, 128, 512, 512, 512, 512, 1'024,
      384, 384, 768, 768, 512, 1'536};
  for (std::uint8_t type = 1;
       type <= static_cast<std::uint8_t>(surcharges.size()); ++type) {
    require(V1ResourceFee::transactionUnits(
                policy, {type, 249, 92, 1, 1, 0}) ==
                1'749 + surcharges[type - 1],
            "every v1 transaction type must have the documented surcharge");
  }
  require(!V1ResourceFee::transactionUnits(policy, {14, 249, 92, 1, 1, 0}) &&
              !V1ResourceFee::transactionUnits(policy, {1, 249, 92, 129, 1, 0}) &&
              !V1ResourceFee::transactionUnits(policy, {1, 249, 92, 1, 1, 1}) &&
              !V1ResourceFee::transactionUnits(policy, {1, 249, 65'537, 1, 1, 0}) &&
              !V1ResourceFee::evidenceUnits(262'145) &&
              !V1ResourceFee::systemRecordUnits({100, 92, 2046}),
          "unknown types and unbounded work must fail before charging");
  require(!V1ResourceFee::minimumFee(
              policy, std::numeric_limits<std::uint64_t>::max(), 2),
          "dynamic base fees must not wrap a u64 transaction fee");
}

void testBlockBudget() {
  const auto policy = parameters();
  const std::array<std::uint64_t, 1> transactions{2'005};
  const std::array<std::uint64_t, 1> evidence{2'981};
  const std::array<std::uint64_t, 1> system{1'408};
  require(V1ResourceFee::blockUnits(policy, 300, transactions, evidence,
                                    system) == 6'394,
          "the header must commit the recomputed sum of every work class");
  require(V1ResourceFee::blockUnits(policy, 20, {}, {}, {}) == 0,
          "an empty body has zero execution units");
  const std::array<std::uint64_t, 2> tooManyUserUnits{524'288, 262'145};
  require(!V1ResourceFee::blockUnits(policy, 300, tooManyUserUnits, {}, {}),
          "user transactions cannot consume the reserved quarter");
  const std::array<std::uint64_t, 2> userCap{524'288, 262'144};
  require(V1ResourceFee::blockUnits(policy, 300, userCap, {}, {}) == 786'432,
          "the user budget boundary is inclusive");
  const std::array<std::uint64_t, 2> overBlock{262'144, 1};
  require(!V1ResourceFee::blockUnits(policy, 300, userCap, {}, overBlock),
          "system work may not overflow the total block cap");
  const std::vector<std::uint64_t> tooMuchEvidence(33, 1);
  require(!V1ResourceFee::blockUnits(policy, 300, {}, tooMuchEvidence, {}),
          "evidence count must be capped independently of unit cost");
}

void testBaseFeeAdjustment() {
  constexpr std::uint64_t maximum = 1'048'576;
  constexpr std::uint64_t target = maximum / 2;
  require(V1ResourceFee::nextBaseFee(100, target, maximum, 1) == 100 &&
              V1ResourceFee::nextBaseFee(100, maximum, maximum, 1) == 112 &&
              V1ResourceFee::nextBaseFee(100, 0, maximum, 1) == 88,
          "base fee moves by at most one eighth at the utilization extremes");
  require(V1ResourceFee::nextBaseFee(1, target + 1, maximum, 1) == 2 &&
              V1ResourceFee::nextBaseFee(100, target - 1, maximum, 1) == 100,
          "upward minimum step and downward truncation must be exact");
  require(V1ResourceFee::nextBaseFee(100, 0, maximum, 200) == 200,
          "an activated child floor applies without rewriting the parent");
  require(V1ResourceFee::nextBaseFee(
              std::numeric_limits<std::uint64_t>::max(), maximum, maximum, 1) ==
              std::numeric_limits<std::uint64_t>::max(),
          "base fee saturation must not wrap");
  require(!V1ResourceFee::nextBaseFee(0, 0, maximum, 1) &&
              !V1ResourceFee::nextBaseFee(1, maximum + 1, maximum, 1) &&
              !V1ResourceFee::nextBaseFee(1, 0, maximum, 0),
          "invalid parent commitment or child floor must fail closed");
}

} // namespace

int main() {
  try {
    testParameterBounds();
    testMeterAndFee();
    testBlockBudget();
    testBaseFeeAdjustment();
    std::cout << "V1 resource and fee reference tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 resource and fee reference tests failed: " << error.what()
              << '\n';
    return 1;
  }
}
