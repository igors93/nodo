#include "node/NodePruningConfig.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::NodePruningConfig;
using nodo::node::NodePruningMode;

void requireCondition(bool condition, const std::string &failureMessage) {
  if (!condition) {
    throw std::runtime_error(failureMessage);
  }
}

void testArchiveModeNeverPrunesAnyHeight() {
  const NodePruningConfig config = NodePruningConfig::archiveMode();

  requireCondition(config.mode() == NodePruningMode::ARCHIVE,
                   "archiveMode() must return ARCHIVE mode.");
  requireCondition(config.isValid(), "ARCHIVE mode must be valid.");

  // ARCHIVE never prunes, regardless of heights
  requireCondition(!config.shouldPruneBlockAtHeight(0, 10000),
                   "ARCHIVE: height 0 should not be pruned.");
  requireCondition(!config.shouldPruneBlockAtHeight(100, 10000),
                   "ARCHIVE: height 100 should not be pruned.");
  requireCondition(!config.shouldPruneBlockAtHeight(9999, 10000),
                   "ARCHIVE: height 9999 should not be pruned.");
  requireCondition(!config.shouldPruneStateAtHeight(0, 10000),
                   "ARCHIVE: state height 0 should not be pruned.");

  requireCondition(config.retainFromHeight(10000) == 0,
                   "ARCHIVE retainFromHeight must return 0.");
}

void testFullModeWith2EpochsAt3000PrunesOldButNotRecent() {
  // 2 epochs × 1000 blocks/epoch = 2000 blocks retained
  // At currentHeight=3000, retain from height 1000
  // height 0 → prune (0 < 1000)
  // height 2500 → keep (2500 >= 1000)
  const NodePruningConfig config = NodePruningConfig::fullMode(2);
  const std::uint64_t currentHeight = 3000;
  const std::uint64_t epochBlocks = 1000;

  requireCondition(config.mode() == NodePruningMode::FULL,
                   "fullMode() must return FULL mode.");
  requireCondition(config.isValid(),
                   "FULL mode with retainEpochs=2 must be valid.");

  requireCondition(
      config.shouldPruneBlockAtHeight(0, currentHeight, epochBlocks),
      "FULL: height 0 at currentHeight=3000, 2 epochs should be pruned.");
  requireCondition(
      !config.shouldPruneBlockAtHeight(2500, currentHeight, epochBlocks),
      "FULL: height 2500 at currentHeight=3000, 2 epochs should not be "
      "pruned.");
  requireCondition(
      !config.shouldPruneBlockAtHeight(1000, currentHeight, epochBlocks),
      "FULL: retainFromHeight=1000 — exactly at boundary should not be "
      "pruned.");
  requireCondition(
      config.shouldPruneBlockAtHeight(999, currentHeight, epochBlocks),
      "FULL: height 999 (< retainFromHeight=1000) should be pruned.");

  requireCondition(config.retainFromHeight(currentHeight, epochBlocks) == 1000,
                   "FULL retainFromHeight must be currentHeight - "
                   "retainEpochs*epochBlocks = 1000.");
}

void testLightModePrunesEverythingExceptCurrentTip() {
  const NodePruningConfig config = NodePruningConfig::lightMode();
  const std::uint64_t currentHeight = 5000;

  requireCondition(config.mode() == NodePruningMode::LIGHT,
                   "lightMode() must return LIGHT mode.");
  requireCondition(config.isValid(), "LIGHT mode must be valid.");

  // Everything below currentHeight should be pruned
  requireCondition(config.shouldPruneBlockAtHeight(0, currentHeight),
                   "LIGHT: height 0 should be pruned.");
  requireCondition(config.shouldPruneBlockAtHeight(4999, currentHeight),
                   "LIGHT: height currentHeight-1 should be pruned.");
  // The current height itself should NOT be pruned (it's the tip)
  requireCondition(
      !config.shouldPruneBlockAtHeight(currentHeight, currentHeight),
      "LIGHT: tip height should not be pruned.");

  requireCondition(config.retainFromHeight(currentHeight) == currentHeight,
                   "LIGHT retainFromHeight must equal currentHeight.");
}

void testFullModeWithZeroEpochsIsInvalid() {
  const NodePruningConfig config = NodePruningConfig::fullMode(0);

  requireCondition(!config.isValid(),
                   "FULL mode with retainEpochs=0 must be invalid.");
}

void testSerializeDeserializeRoundTripArchive() {
  const NodePruningConfig original = NodePruningConfig::archiveMode();
  const std::string serialized = original.serialize();

  requireCondition(!serialized.empty(),
                   "serialize() must produce a non-empty string.");
  requireCondition(serialized.find("ARCHIVE") != std::string::npos,
                   "Serialized ARCHIVE config must contain 'ARCHIVE'.");

  const NodePruningConfig restored = NodePruningConfig::deserialize(serialized);

  requireCondition(restored.mode() == NodePruningMode::ARCHIVE,
                   "Deserialized mode must be ARCHIVE.");
}

void testSerializeDeserializeRoundTripFull() {
  const NodePruningConfig original = NodePruningConfig::fullMode(5);
  const std::string serialized = original.serialize();

  const NodePruningConfig restored = NodePruningConfig::deserialize(serialized);

  requireCondition(restored.mode() == NodePruningMode::FULL,
                   "Deserialized FULL mode must be FULL.");
  requireCondition(restored.retainEpochs() == 5,
                   "Deserialized retainEpochs must be 5.");
  requireCondition(restored.isValid(),
                   "Deserialized FULL config must be valid.");
}

void testSerializeDeserializeRoundTripLight() {
  const NodePruningConfig original = NodePruningConfig::lightMode();
  const std::string serialized = original.serialize();

  const NodePruningConfig restored = NodePruningConfig::deserialize(serialized);

  requireCondition(restored.mode() == NodePruningMode::LIGHT,
                   "Deserialized LIGHT mode must be LIGHT.");
  requireCondition(restored.isValid(),
                   "Deserialized LIGHT config must be valid.");
}

void testDefaultConstructedConfigIsArchive() {
  const NodePruningConfig config;

  requireCondition(config.mode() == NodePruningMode::ARCHIVE,
                   "Default-constructed config must be ARCHIVE mode.");
}

void testFullModeRetainFromHeightAtBoundary() {
  // If currentHeight < retainEpochs * epochBlocks, retainFrom = 0 (nothing to
  // prune)
  const NodePruningConfig config = NodePruningConfig::fullMode(10);

  // currentHeight=500, retain = 10 * 1000 = 10000 > 500
  const std::uint64_t retainFrom = config.retainFromHeight(500, 1000);

  requireCondition(
      retainFrom == 0,
      "When currentHeight < retainBlocks, retainFromHeight must return 0.");

  // Nothing should be pruned in this case
  requireCondition(!config.shouldPruneBlockAtHeight(0, 500, 1000),
                   "FULL: when chain is shorter than retention period, no "
                   "block should be pruned.");
}

} // namespace

int main() {
  try {
    testArchiveModeNeverPrunesAnyHeight();
    testFullModeWith2EpochsAt3000PrunesOldButNotRecent();
    testLightModePrunesEverythingExceptCurrentTip();
    testFullModeWithZeroEpochsIsInvalid();
    testSerializeDeserializeRoundTripArchive();
    testSerializeDeserializeRoundTripFull();
    testSerializeDeserializeRoundTripLight();
    testDefaultConstructedConfigIsArchive();
    testFullModeRetainFromHeightAtBoundary();

    std::cout << "Nodo NodePruningConfig tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo NodePruningConfig tests failed: " << error.what()
              << "\n";
    return 1;
  }
}
