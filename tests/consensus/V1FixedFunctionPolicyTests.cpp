#include "../common/TestFramework.hpp"
#include "consensus/V1FixedFunctionPolicy.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using nodo::consensus::V1FixedFunctionPolicy;
using Violation = V1FixedFunctionPolicy::Violation;
using nodo::test::require;

std::vector<unsigned char> proposal(std::uint8_t kind,
                                    const std::vector<unsigned char> &action) {
  std::vector<unsigned char> result(1 + 32 + 4 + action.size() + 8);
  result[0] = kind;
  const std::size_t size = action.size();
  result[33] = static_cast<unsigned char>(size >> 24);
  result[34] = static_cast<unsigned char>(size >> 16);
  result[35] = static_cast<unsigned char>(size >> 8);
  result[36] = static_cast<unsigned char>(size);
  std::copy(action.begin(), action.end(), result.begin() + 37);
  return result;
}

void testAllClosedTransactionShapes() {
  const std::array<std::size_t, 14> sizes{
      0, 32, 0, 32, 32, 32, 32, 64, 32, 32, 72, 0, 65, 32};
  for (std::uint8_t type = 1; type <= 13; ++type) {
    std::vector<unsigned char> payload(sizes[type], 0x11);
    if (type == 11) {
      payload = proposal(3, {});
    }
    if (type == 12) {
      payload[32] = 1;
    }
    const std::uint64_t amount = type <= 6 || type == 11 ? 1 : 0;
    require(V1FixedFunctionPolicy::validateTransaction(type, amount, payload) ==
                Violation::NONE,
            "all thirteen fixed transaction shapes must be accepted");
    payload.push_back(0);
    require(V1FixedFunctionPolicy::validateTransaction(type, amount, payload) !=
                Violation::NONE,
            "trailing or extension bytes must be rejected");
    payload.pop_back();
    if (!payload.empty()) {
      payload.pop_back();
      require(V1FixedFunctionPolicy::validateTransaction(type, amount,
                                                         payload) !=
                  Violation::NONE,
              "a truncated typed payload must be rejected");
    }
  }
}

void testNoCodeOrUnknownAction() {
  const std::vector<unsigned char> wasm{0x00, 0x61, 0x73, 0x6d};
  require(V1FixedFunctionPolicy::validateTransaction(14, 0, wasm) ==
              Violation::UNKNOWN_TYPE &&
              V1FixedFunctionPolicy::validateTransaction(0, 0, {}) ==
                  Violation::UNKNOWN_TYPE &&
              V1FixedFunctionPolicy::validateTransaction(1, 1, wasm) ==
                  Violation::INVALID_SHAPE,
          "unknown or code-bearing transaction shapes must be rejected");
  require(V1FixedFunctionPolicy::validateTransaction(
              11, 1, proposal(3, wasm)) == Violation::INVALID_ACTION &&
              V1FixedFunctionPolicy::validateTransaction(
                  11, 1, proposal(6, {})) == Violation::INVALID_ACTION,
          "TEXT cannot execute data and unknown governance cannot add a VM");
  for (unsigned type = 14; type <= 255; ++type) {
    require(V1FixedFunctionPolicy::validateTransaction(
                static_cast<std::uint8_t>(type), 0, {}) ==
                Violation::UNKNOWN_TYPE,
            "every unassigned v1 transaction tag must stay closed");
  }
  for (unsigned kind = 6; kind <= 255; ++kind) {
    require(V1FixedFunctionPolicy::validateGovernanceAction(
                static_cast<std::uint8_t>(kind), {}) ==
                Violation::INVALID_ACTION,
            "every unassigned v1 governance action must stay closed");
  }
}

void testGovernanceAndAmountBoundaries() {
  std::vector<unsigned char> parameter(17);
  parameter[0] = 7;
  std::vector<unsigned char> spend(40);
  spend[39] = 1;
  const std::array<std::vector<unsigned char>, 5> actions{
      parameter, spend, {},
      std::vector<unsigned char>(106), std::vector<unsigned char>(40)};
  for (std::uint8_t kind = 1; kind <= 5; ++kind) {
    require(V1FixedFunctionPolicy::validateTransaction(
                11, 1, proposal(kind, actions[kind - 1])) == Violation::NONE,
            "all five closed governance action shapes must be accepted");
  }
  parameter[0] = 8;
  require(V1FixedFunctionPolicy::validateGovernanceAction(1, parameter) ==
              Violation::INVALID_ACTION,
          "a parameter vote cannot authorize an unlisted field");
  require(V1FixedFunctionPolicy::validateGovernanceAction(
              2, std::vector<unsigned char>(40)) ==
              Violation::INVALID_ACTION,
          "a zero treasury spend cannot create a no-op execution");
  require(V1FixedFunctionPolicy::validateTransaction(1, 0,
                                                    std::vector<unsigned char>(32)) ==
              Violation::INVALID_AMOUNT &&
              V1FixedFunctionPolicy::validateTransaction(7, 1,
                                                    std::vector<unsigned char>(64)) ==
                  Violation::INVALID_AMOUNT,
          "fixed transaction amount classes must be enforced");
  std::vector<unsigned char> vote(65);
  vote[32] = 4;
  require(V1FixedFunctionPolicy::validateTransaction(12, 0, vote) ==
              Violation::INVALID_CHOICE,
          "an unknown governance choice must be rejected");
}

void testFramingAndBounds() {
  auto tx = proposal(4, std::vector<unsigned char>(106));
  require(V1FixedFunctionPolicy::validateTransaction(11, 1, tx) ==
              Violation::NONE,
          "a correctly framed upgrade action must be accepted");
  tx[36] = 105;
  require(V1FixedFunctionPolicy::validateTransaction(11, 1, tx) ==
              Violation::INVALID_SHAPE,
          "a mismatched action length must be rejected");
  tx = proposal(4, std::vector<unsigned char>(106));
  tx[33] = 0xff;
  tx[34] = 0xff;
  tx[35] = 0xff;
  tx[36] = 0xff;
  require(V1FixedFunctionPolicy::validateTransaction(11, 1, tx) ==
              Violation::INVALID_SHAPE,
          "an oversized declared action cannot overflow framing arithmetic");
  require(V1FixedFunctionPolicy::validateTransaction(
              11, 1, std::vector<unsigned char>(44)) ==
                  Violation::INVALID_SHAPE &&
              V1FixedFunctionPolicy::validateTransaction(
                  11, 1,
                  std::vector<unsigned char>(
                      V1FixedFunctionPolicy::kMaxTransactionBytes + 1)) ==
                  Violation::OVERSIZE,
          "truncation and oversized payloads must fail before typed decoding");
}

} // namespace

int main() {
  try {
    testAllClosedTransactionShapes();
    testNoCodeOrUnknownAction();
    testGovernanceAndAmountBoundaries();
    testFramingAndBounds();
    std::cout << "V1 fixed-function policy tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 fixed-function policy tests failed: " << error.what()
              << '\n';
    return 1;
  }
}
