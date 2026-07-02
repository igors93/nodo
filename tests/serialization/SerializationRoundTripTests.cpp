#include "economics/MintRecord.hpp"
#include "serialization/FieldCodec.hpp"
#include "serialization/MintRecordCodec.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::economics::MintReason;
using nodo::economics::MintRecord;
using nodo::serialization::FieldCodec;
using nodo::serialization::MintRecordCodec;
using nodo::utils::Amount;

constexpr std::int64_t kTestTimestamp = 1700000000;

void requireCondition(bool condition, const std::string &failureMessage) {
  if (!condition) {
    throw std::runtime_error(failureMessage);
  }
}

std::string replaceFirst(const std::string &input, const std::string &target,
                         const std::string &replacement) {
  const std::size_t position = input.find(target);

  if (position == std::string::npos) {
    throw std::runtime_error("Could not find text to replace: " + target);
  }

  std::string output = input;
  output.replace(position, target.size(), replacement);

  return output;
}

void requireRejected(const std::string &failureMessage,
                     const std::function<void()> &action) {
  bool rejected = false;

  try {
    action();
  } catch (const std::exception &) {
    rejected = true;
  }

  requireCondition(rejected, failureMessage);
}

void testFieldCodecBasicExtraction() {
  const std::string serialized =
      "Example{id=abc123;type=TEST;payload=[Object{id=inner}]};";

  requireCondition(FieldCodec::extractField(serialized, "id") == "abc123",
                   "FieldCodec failed to extract id.");

  requireCondition(FieldCodec::extractField(serialized, "type") == "TEST",
                   "FieldCodec failed to extract type.");
}

void testFieldCodecTopLevelObjectSplit() {
  const std::string serializedList = "Object{id=a;type=FIRST},"
                                     "Object{id=b;type=SECOND}";

  const std::vector<std::string> objects =
      FieldCodec::splitTopLevelObjects(serializedList, "Object{");

  requireCondition(objects.size() == 2,
                   "FieldCodec failed to split two top-level objects.");

  requireCondition(objects[0].find("id=a") != std::string::npos,
                   "First split object is incorrect.");

  requireCondition(objects[1].find("id=b") != std::string::npos,
                   "Second split object is incorrect.");
}

void testMintRecordCodecRoundTrip() {
  MintRecord original("mint_test_codec_001", "auth_test_001", "igor",
                      Amount::fromNodo(1000), MintReason::GENESIS_ALLOCATION, 0,
                      0, "GENESIS", kTestTimestamp);

  const std::string serialized = original.serialize();

  MintRecord rebuilt = MintRecordCodec::deserialize(serialized);

  requireCondition(rebuilt.isValid(),
                   "MintRecordCodec round-trip produced an invalid object.");

  requireCondition(rebuilt.serialize() == serialized,
                   "MintRecordCodec round-trip changed serialization.");
}

void testMintRecordLegacyDeserializeDelegatesToCodec() {
  MintRecord original("mint_test_legacy_delegate_001", "auth_test_legacy_001",
                      "ana", Amount::fromNodo(25),
                      MintReason::NETWORK_DEFENSE_REWARD, 7, 3, "abc123",
                      kTestTimestamp + 1);

  const std::string serialized = original.serialize();

  MintRecord rebuilt = MintRecord::deserialize(serialized);

  requireCondition(rebuilt.isValid(),
                   "MintRecord::deserialize produced an invalid object.");

  requireCondition(rebuilt.serialize() == serialized,
                   "MintRecord::deserialize changed serialization.");
}

void testMintRecordCodecRejectsTamperedAmount() {
  const std::string tampered =
      "MintRecord{id=mint_bad_001;recipient=igor;amountRaw=-1;"
      "reason=GENESIS_ALLOCATION;epoch=0;sourceBlockIndex=0;"
      "sourceBlockHash=GENESIS;timestamp=1700000000}";

  bool rejected = false;

  try {
    (void)MintRecordCodec::deserialize(tampered);
  } catch (const std::exception &) {
    rejected = true;
  }

  requireCondition(rejected,
                   "MintRecordCodec accepted a tampered negative mint amount.");
}

void testCanonicalSerializationRejectsMintRecordFieldReordering() {
  MintRecord original("mint_test_canonical_order_001",
                      "auth_canonical_order_001", "igor",
                      Amount::fromNodo(1000), MintReason::GENESIS_ALLOCATION, 0,
                      0, "GENESIS", kTestTimestamp);

  const std::string nonCanonical =
      "MintRecord{recipient=igor;id=mint_test_canonical_order_001;"
      "amountRaw=100000000000;reason=GENESIS_ALLOCATION;epoch=0;"
      "sourceBlockIndex=0;sourceBlockHash=GENESIS;timestamp=1700000000}";

  requireRejected("MintRecordCodec accepted reordered fields.",
                  [&]() { (void)MintRecordCodec::deserialize(nonCanonical); });
}

void testCanonicalSerializationRejectsMintRecordUnknownField() {
  MintRecord original("mint_test_canonical_unknown_001",
                      "auth_canonical_unknown_001", "igor",
                      Amount::fromNodo(1000), MintReason::GENESIS_ALLOCATION, 0,
                      0, "GENESIS", kTestTimestamp);

  const std::string nonCanonical =
      replaceFirst(original.serialize(), ";timestamp=1700000000}",
                   ";timestamp=1700000000;unknownField=forbidden}");

  requireRejected("MintRecordCodec accepted an unknown field.",
                  [&]() { (void)MintRecordCodec::deserialize(nonCanonical); });
}

void testCanonicalSerializationRejectsMintRecordLeadingZeroAmount() {
  MintRecord original("mint_test_canonical_leading_zero_001",
                      "auth_canonical_leading_zero_001", "igor",
                      Amount::fromNodo(1000), MintReason::GENESIS_ALLOCATION, 0,
                      0, "GENESIS", kTestTimestamp);

  const std::string nonCanonical =
      replaceFirst(original.serialize(), "amountRaw=100000000000",
                   "amountRaw=0100000000000");

  requireRejected(
      "MintRecordCodec accepted a non-canonical leading-zero amount.",
      [&]() { (void)MintRecordCodec::deserialize(nonCanonical); });
}

} // namespace

int main() {
  try {
    testFieldCodecBasicExtraction();
    testFieldCodecTopLevelObjectSplit();
    testMintRecordCodecRoundTrip();
    testMintRecordLegacyDeserializeDelegatesToCodec();
    testMintRecordCodecRejectsTamperedAmount();
    testCanonicalSerializationRejectsMintRecordFieldReordering();
    testCanonicalSerializationRejectsMintRecordUnknownField();
    testCanonicalSerializationRejectsMintRecordLeadingZeroAmount();

    std::cout << "Nodo serialization round-trip tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo serialization round-trip tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
