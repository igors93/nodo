#include "storage/AccountStateSnapshotStore.hpp"

#include "config/NetworkParameters.hpp"
#include "core/AccountState.hpp"
#include "core/AccountStateView.hpp"
#include "core/Blockchain.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;
using storage::AccountStateSnapshot;
using storage::AccountStateSnapshotStore;

constexpr std::int64_t kTs = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

std::filesystem::path tempDir() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        "nodo_gap6_snapshot_test";
    std::filesystem::create_directories(dir);
    return dir;
}

core::AccountStateView buildView() {
    core::AccountStateView view;
    view.putAccount(core::AccountState("addr-1", utils::Amount(1000), 5));
    view.putAccount(core::AccountState("addr-2", utils::Amount(2000), 3));
    return view;
}

void testSnapshotRoundTrip() {
    const std::filesystem::path dir = tempDir();
    AccountStateSnapshotStore store(dir);

    const core::AccountStateView view = buildView();
    const AccountStateSnapshot snap("genesis-id-abc", 100, "block-hash-100", view);

    require(snap.isValid(), "Snapshot must be valid before saving.");
    store.save(snap);

    const auto loaded = store.load();
    require(loaded.has_value(), "Snapshot must load after saving.");
    require(loaded->genesisConfigId() == "genesis-id-abc",
            "Loaded genesis config ID must match saved value.");
    require(loaded->height() == 100,
            "Loaded height must match saved height.");
    require(loaded->blockHash() == "block-hash-100",
            "Loaded block hash must match saved block hash.");

    // Verify accounts round-tripped.
    const auto& loadedView = loaded->view();
    require(loadedView.hasAccount("addr-1"),
            "addr-1 must be present after round-trip.");
    require(loadedView.hasAccount("addr-2"),
            "addr-2 must be present after round-trip.");
    require(loadedView.accountOrDefault("addr-1").balance().rawUnits() == 1000,
            "addr-1 balance must round-trip correctly.");
    require(loadedView.accountOrDefault("addr-1").nonce() == 5,
            "addr-1 nonce must round-trip correctly.");
    require(loadedView.accountOrDefault("addr-2").balance().rawUnits() == 2000,
            "addr-2 balance must round-trip correctly.");
}

void testMissingSnapshotReturnsNullopt() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        "nodo_gap6_snapshot_missing_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    AccountStateSnapshotStore store(dir);
    require(!store.load().has_value(),
            "Load from empty directory must return nullopt.");
}

void testSnapshotOverwritesPreviousSnapshot() {
    const std::filesystem::path dir = tempDir();
    AccountStateSnapshotStore store(dir);

    core::AccountStateView v1;
    v1.putAccount(core::AccountState("addr-1", utils::Amount(1000), 1));
    store.save(AccountStateSnapshot("genesis", 10, "hash-10", v1));

    core::AccountStateView v2;
    v2.putAccount(core::AccountState("addr-2", utils::Amount(9999), 7));
    store.save(AccountStateSnapshot("genesis", 20, "hash-20", v2));

    const auto loaded = store.load();
    require(loaded.has_value(), "Must load after second save.");
    require(loaded->height() == 20, "Second save must overwrite first.");
    require(loaded->view().hasAccount("addr-2"),
            "Overwritten snapshot must contain addr-2 from second save.");
    require(!loaded->view().hasAccount("addr-1"),
            "Overwritten snapshot must NOT contain addr-1 from first save.");
}

void testPartialReplayMatchesFullReplay() {
    // Build a genesis config and empty blockchain (only genesis block at height 0).
    const config::GenesisConfig genesis(
        config::NetworkParameters::developmentLocal(),
        kTs,
        {},
        { config::GenesisAccountConfig("gap6-account", utils::Amount::fromRawUnits(5000), 0) },
        "gap6-partial-replay-test"
    );

    // An empty blockchain still contains the genesis block (height 0).
    // Since there are no blocks beyond genesis, the full replay and the
    // partial replay starting at height 0 (same as genesis) must agree.
    const config::GenesisBuildResult genesisResult =
        config::GenesisBuilder::build(genesis);
    require(genesisResult.built(), "Genesis must build for partial-replay test.");

    const core::Blockchain& blockchain = genesisResult.blockchain();

    const core::AccountStateView fullReplay =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(genesis, blockchain, 0);

    // Snapshot at genesis height 0 — same as the initial state.
    const core::AccountStateView genesisSnapshot =
        node::RuntimeAccountStateBuilder::initialAccountStateView(genesis);

    const core::AccountStateView partialReplay =
        node::RuntimeAccountStateBuilder::accountStateViewFromSnapshot(
            genesisSnapshot, blockchain, 0 /* snapshotHeight */, 0
        );

    // Both paths must produce the same account states.
    const auto fullAddresses = fullReplay.knownAddresses();
    const auto partialAddresses = partialReplay.knownAddresses();

    require(fullAddresses.size() == partialAddresses.size(),
            "Full and partial replay must produce the same number of accounts.");

    for (const auto& addr : fullAddresses) {
        require(partialReplay.hasAccount(addr),
                "Partial replay must contain all accounts from full replay.");
        const auto fullAcc = fullReplay.accountOrDefault(addr);
        const auto partAcc = partialReplay.accountOrDefault(addr);
        require(fullAcc.balance().rawUnits() == partAcc.balance().rawUnits(),
                "Balance must match between full and partial replay for " + addr);
        require(fullAcc.nonce() == partAcc.nonce(),
                "Nonce must match between full and partial replay for " + addr);
    }
}

} // namespace

int main() {
    try {
        testSnapshotRoundTrip();
        testMissingSnapshotReturnsNullopt();
        testSnapshotOverwritesPreviousSnapshot();
        testPartialReplayMatchesFullReplay();

        std::cout << "Nodo Gap6 account-state-snapshot tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo Gap6 account-state-snapshot tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
