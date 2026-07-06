#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/StateRootCalculator.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/FastSyncSnapshotStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodePruningManifest.hpp"
#include "node/NodePruningService.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

const std::string HASH_A =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const std::string HASH_B =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const std::string HASH_C =
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
const std::string HASH_D =
    "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

std::filesystem::path tempDir(const std::string &name) {
  return std::filesystem::temp_directory_path() /
         ("nodo-pruning-tests-" + name);
}

nodo::core::AccountStateView accounts() {
  nodo::core::AccountStateView view;
  assert(view.putAccount(nodo::core::AccountState(
      "alice", nodo::utils::Amount::fromRawUnits(1000), 1)));
  return view;
}

nodo::node::FastSyncSnapshot snapshotAt(std::uint64_t height) {
  const auto view = accounts();
  const std::string accountRoot =
      nodo::core::StateRootCalculator::calculateAccountStateRoot(view);
  return nodo::node::FastSyncSnapshot(
      HASH_A, "chain-a", "localnet", height, HASH_B, HASH_C, accountRoot,
      HASH_D, view.accounts(), 1900000000 + static_cast<std::int64_t>(height));
}

nodo::node::NodeRuntimeManifest runtimeManifest(std::uint64_t height) {
  return nodo::node::NodeRuntimeManifest(
      "chain-a", "localnet", "nodo/0.1", HASH_A, height, HASH_B, HASH_C, 1, 0,
      1900000000, 1900000000 + static_cast<std::int64_t>(height));
}

void touch(const std::filesystem::path &path) {
  std::filesystem::create_directories(path.parent_path());
  nodo::storage::AtomicFile::writeTextFile(path, "test\n");
}

void clean(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

void testManifestRoundTrip() {
  const nodo::node::NodeRuntimeManifest runtime = runtimeManifest(9);
  const nodo::node::NodePruningManifest manifest(
      nodo::node::NodePruningConfig::lightMode(), runtime.chainId(),
      runtime.genesisConfigId(), runtime.latestBlockHeight(),
      runtime.latestBlockHash(), runtime.latestStateRoot(), 9, 9, HASH_D, 2, 3,
      1900000100);
  assert(manifest.isValid());
  const std::string serialized = manifest.toFileContents();
  const auto decoded =
      nodo::node::NodePruningManifest::fromFileContents(serialized);
  assert(decoded.toFileContents() == serialized);
  assert(decoded.config().mode() == nodo::node::NodePruningMode::LIGHT);
}

void testFullModePrunesOnlyOldSnapshots() {
  const std::filesystem::path dir = tempDir("full");
  clean(dir);
  const nodo::node::NodeDataDirectoryConfig config(dir);
  std::filesystem::create_directories(config.blocksDirectoryPath());
  std::filesystem::create_directories(config.fastSyncSnapshotsDirectoryPath());

  nodo::node::FastSyncSnapshotStore store(
      config.fastSyncSnapshotsDirectoryPath());
  assert(store.save(snapshotAt(100)));
  assert(store.save(snapshotAt(1005)));

  touch(config.blocksDirectoryPath() / ("block_1_" + HASH_B + ".nodo"));

  const auto result = nodo::node::NodePruningService::apply(
      config, runtimeManifest(2005), nodo::node::NodePruningConfig::fullMode(1),
      1900000300);

  assert(result.success());
  assert(!std::filesystem::exists(store.snapshotPath(100)));
  assert(std::filesystem::exists(store.snapshotPath(1005)));
  assert(std::filesystem::exists(config.blocksDirectoryPath() /
                                 ("block_1_" + HASH_B + ".nodo")));
  clean(dir);
}

void testLightModeRequiresBoundarySnapshotAndPrunesOldArtifacts() {
  const std::filesystem::path dir = tempDir("light");
  clean(dir);
  const nodo::node::NodeDataDirectoryConfig config(dir);
  std::filesystem::create_directories(config.blocksDirectoryPath());
  std::filesystem::create_directories(config.fastSyncSnapshotsDirectoryPath());

  nodo::node::FastSyncSnapshotStore store(
      config.fastSyncSnapshotsDirectoryPath());
  assert(store.save(snapshotAt(12)));

  const std::filesystem::path oldBlock =
      config.blocksDirectoryPath() / ("block_1_" + HASH_B + ".nodo");
  touch(oldBlock);

  const auto result = nodo::node::NodePruningService::apply(
      config, runtimeManifest(12), nodo::node::NodePruningConfig::lightMode(),
      1900000400);

  assert(result.success());
  assert(!std::filesystem::exists(oldBlock));
  assert(
      std::filesystem::exists(config.prunedBlocksDirectoryPath() / "1.pruned"));
  clean(dir);
}

} // namespace

int main() {
  testManifestRoundTrip();
  testFullModePrunesOnlyOldSnapshots();
  testLightModeRequiresBoundarySnapshotAndPrunesOldArtifacts();
  return 0;
}
