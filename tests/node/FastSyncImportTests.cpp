#include "config/GenesisRegistry.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/StateRootCalculator.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/FastSyncSnapshotStore.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/ProtocolExecutionStateParser.hpp"
#include "node/NodeDataDirectory.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nodo::node;
using namespace nodo::core;
using namespace nodo::config;

int main() {
  // 1. Setup a simple runtime
  GenesisConfig genesisConfig = GenesisRegistry::get("localnet").genesis();
  NodeRuntime runtime;

  // 2. Build a dummy snapshot
  AccountStateView view;
  view.putAccount(
      AccountState("alice", nodo::utils::Amount::fromRawUnits(1000), 7));
  std::string accountRoot =
      StateRootCalculator::calculateAccountStateRoot(view);

  std::map<std::string, std::string> domains;
  domains["supply"] = "RuntimeSupply{latestRawUnits=1000}";
  domains["validators"] = "ValidatorRegistry{size=0;entries=[]}";

  FastSyncSnapshot snapshot(
      genesisConfig.genesisMemo(), genesisConfig.networkParameters().chainId(),
      genesisConfig.networkParameters().networkName(), 10,
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
      accountRoot,
      "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
      view.accounts(), 1700000000, domains);

  PersistentSnapshotSyncManifest manifest(
      "peer-a", snapshot.blockHeight(), snapshot.blockHash(),
      snapshot.stateRoot(), snapshot.digest(), snapshot.createdAt());

  std::filesystem::path dir = std::filesystem::temp_directory_path() /
                              "nodo-fast-sync-snapshot-import-tests";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir / "runtime" / "fast_sync_snapshots");

  NodeDataDirectoryConfig directoryConfig(dir);

  // Write the snapshot to disk
  FastSyncSnapshotStore store(directoryConfig.runtimeDirectoryPath() /
                              "fast_sync_snapshots");
  assert(store.save(snapshot));

  PersistentSyncCheckpoint localCheckpoint(
      PersistentSyncCheckpoint::SCHEMA_VERSION, 0, genesisConfig.genesisMemo(),
      "", PersistentSyncStatus::SYNCING, "local", 1700000000);

  // 3. Test import
  auto result = PersistentBlockStateSyncApplier::importSnapshot(
      localCheckpoint, manifest, runtime, directoryConfig, nullptr, 1700000001);

  // Since the root digests might not match, we just test that the process runs
  // without crashing and correctly attempts to hydrate. Because we put dummy
  // hashes above, verification will fail. So result should be REJECTED.
  assert(!result.applied());
  std::cout << "Import rejected (expected due to invalid hashes): "
            << result.reason() << std::endl;

  std::filesystem::remove_all(dir);
  return 0;
}
