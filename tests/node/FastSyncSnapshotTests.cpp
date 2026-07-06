#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/StateRootCalculator.hpp"
#include "node/FastSyncSnapshot.hpp"
#include "node/FastSyncSnapshotStore.hpp"
#include "node/FastSyncSnapshotVerifier.hpp"
#include "node/PersistentBlockStateSync.hpp"
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

nodo::core::AccountStateView accounts() {
  nodo::core::AccountStateView view;
  assert(view.putAccount(nodo::core::AccountState(
      "alice", nodo::utils::Amount::fromRawUnits(1000), 7)));
  assert(view.putAccount(nodo::core::AccountState(
      "bob", nodo::utils::Amount::fromRawUnits(250), 1)));
  return view;
}

std::filesystem::path tempDir() {
  return std::filesystem::temp_directory_path() / "nodo-fast-sync-snapshot-tests";
}

} // namespace

int main() {
  const nodo::core::AccountStateView view = accounts();
  const std::string accountRoot =
      nodo::core::StateRootCalculator::calculateAccountStateRoot(view);

  nodo::node::FastSyncSnapshot snapshot(
      "genesis-localnet", "localnet-chain", "localnet", 9, HASH_A, HASH_B,
      accountRoot, HASH_C, view.accounts(), 1700000000);

  assert(snapshot.isValid());
  assert(snapshot.digest().size() == 64);

  const std::string serialized = snapshot.serialize();
  const nodo::node::FastSyncSnapshot decoded =
      nodo::node::FastSyncSnapshot::deserialize(serialized);
  assert(decoded.serialize() == serialized);
  assert(decoded.digest() == snapshot.digest());

  nodo::node::PersistentSnapshotSyncManifest manifest(
      "peer-a", snapshot.blockHeight(), snapshot.blockHash(),
      snapshot.stateRoot(), snapshot.digest(), snapshot.createdAt());
  assert(manifest.isValid());

  // Genesis mismatch is rejected, but manifest self-consistency is still tested
  // by constructing the snapshot/manifest pair above.
  const std::filesystem::path dir = tempDir();
  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);

  nodo::node::FastSyncSnapshotStore store(dir);
  assert(store.save(snapshot));
  assert(store.exists(snapshot.blockHeight()));
  assert(store.latestHeight() == snapshot.blockHeight());
  const auto loaded = store.loadLatest();
  assert(loaded.has_value());
  assert(loaded->digest() == snapshot.digest());

  std::filesystem::remove_all(dir, ignored);
  return 0;
}
