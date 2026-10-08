#include "../common/TestFramework.hpp"
#include "serialization/V1EncodingPrimitives.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::serialization::V1EncodingPrimitives;
using nodo::test::require;

void testVersionedKindPrefixesAndLimits() {
  for (std::uint16_t kind = 1; kind <= 14; ++kind) {
    const auto bytes = V1EncodingPrimitives::topLevel(
        static_cast<V1EncodingPrimitives::Kind>(kind), {});
    require(bytes == std::vector<unsigned char>(
                         {'N', 'O', 'D', 'O', 0, 1, 0,
                          static_cast<unsigned char>(kind)}),
            "all v1 kinds must have one canonical 8-byte prefix");
  }
  bool unknownRejected = false;
  try {
    (void)V1EncodingPrimitives::topLevel(
        static_cast<V1EncodingPrimitives::Kind>(15), {});
  } catch (const std::invalid_argument &) {
    unknownRejected = true;
  }
  require(unknownRejected, "unknown object kinds must fail closed");

  const auto maxTransactionBytes = V1EncodingPrimitives::maxObjectBytes(
      V1EncodingPrimitives::Kind::TRANSACTION);
  require(V1EncodingPrimitives::topLevel(
              V1EncodingPrimitives::Kind::TRANSACTION,
              std::vector<unsigned char>(maxTransactionBytes - 8, 0))
              .size() == maxTransactionBytes,
          "the transaction byte limit is inclusive");
  bool oversizeRejected = false;
  try {
    (void)V1EncodingPrimitives::topLevel(
        V1EncodingPrimitives::Kind::TRANSACTION,
        std::vector<unsigned char>(maxTransactionBytes - 7, 0));
  } catch (const std::length_error &) {
    oversizeRejected = true;
  }
  require(oversizeRejected, "oversized object must fail before encoding");
}

void testHashAndMerkleVectors() {
  require(V1EncodingPrimitives::hex(
              V1EncodingPrimitives::hash("TXID", {})) ==
              "b94f996f5d769d8723b6f8f256e08ff0b0f8249f2d153a0be5953dc6218488f4",
          "domain hash must match the independent v1 vector");
  require(V1EncodingPrimitives::hex(
              V1EncodingPrimitives::merkleRoot("tx", {})) ==
              "cc9ac246c73fab960612cfc09e7d9a25f81890ddc55c5040459e1953ddd72de8",
          "empty Merkle root must be domain-separated");
  require(V1EncodingPrimitives::hex(
              V1EncodingPrimitives::merkleRoot("tx", {{0x00, 0xff}})) ==
              "7cfa61cc1b253b6ae110f12dc45e451386374f8d68c755bcdc9f978dbb278efc",
          "nonempty Merkle root must commit leaf count");
  const auto three = V1EncodingPrimitives::merkleRoot(
      "tx", {{'a'}, {'b'}, {'c'}});
  const auto four = V1EncodingPrimitives::merkleRoot(
      "tx", {{'a'}, {'b'}, {'c'}, {'c'}});
  require(V1EncodingPrimitives::hex(three) ==
              "59e0753959cd18d73dc9bd76ab0536e14772c4e272a9a926fffa4a2e24513de6" &&
              V1EncodingPrimitives::hex(four) ==
                  "7821b4c87bceb6815630fd7eb197eab0073ee8cb67c772bceee50c84514e820a" &&
              three != four,
          "odd leaves must not duplicate into an equal even tree");
  require(V1EncodingPrimitives::merkleRoot(
              "tx", {{'a'}, {'c'}, {'b'}}) != three,
          "Merkle roots must preserve order");
  bool unknownKindRejected = false;
  try {
    (void)V1EncodingPrimitives::merkleRoot("unknown", {});
  } catch (const std::invalid_argument &) {
    unknownKindRejected = true;
  }
  require(unknownKindRejected, "unknown Merkle kinds must fail closed");
}

} // namespace

int main() {
  try {
    testVersionedKindPrefixesAndLimits();
    testHashAndMerkleVectors();
    std::cout << "V1 encoding primitive tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "V1 encoding primitive tests failed: " << error.what()
              << '\n';
    return 1;
  }
}
