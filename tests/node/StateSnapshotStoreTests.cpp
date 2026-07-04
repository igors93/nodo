#include "core/State.hpp"
#include "economics/GenesisRewardRecord.hpp"
#include "node/StateSnapshotStore.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

nodo::core::State sampleState() {
  nodo::core::State state;
  state.advanceBlock();

  state.applyGenesisRewardRecord(nodo::economics::GenesisRewardRecord(
      1, "snapshot-validator", nodo::utils::Amount::fromRawUnits(5000),
      nodo::economics::GenesisRewardReason::NETWORK_PROTECTION,
      "snapshot-work-summary", "NODO_EPOCH_EMISSION_POLICY_V1",
      "snapshot-accepted-block", 1900000000));

  return state;
}

std::filesystem::path tempPath() {
  return std::filesystem::temp_directory_path() /
         "nodo-state-snapshot-store-test.snapshot";
}

} // namespace

int main() {
  const std::filesystem::path path = tempPath();
  std::error_code cleanupError;
  std::filesystem::remove(path, cleanupError);
  std::filesystem::remove(path.string() + ".tmp", cleanupError);

  const nodo::core::State state = sampleState();
  nodo::node::StateSnapshotStore store(path);

  assert(store.save(state, state.currentBlockIndex()));
  assert(store.exists());
  assert(store.snapshotHeight() == state.currentBlockIndex());

  const auto loaded = store.load(state.currentBlockIndex());
  assert(loaded.has_value());
  assert(loaded->serialize() == state.serialize());

  std::string corrupted;
  {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream contents;
    contents << in.rdbuf();
    corrupted = contents.str();
  }
  const std::size_t rootLineStart =
      corrupted.find('\n', corrupted.find('\n') + 1) + 1;
  corrupted.replace(rootLineStart, 1, "x");

  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << corrupted;
  }

  assert(!store.load(state.currentBlockIndex()).has_value());

  store.remove();
  assert(!store.exists());

  return 0;
}
