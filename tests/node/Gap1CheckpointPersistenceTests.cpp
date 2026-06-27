#include "node/PersistentBlockStateSync.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::node::PersistentSyncCheckpoint;
using nodo::node::PersistentSyncCheckpointStore;
using nodo::node::PersistentSyncStatus;

constexpr std::int64_t kNow = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

// Return a writable temp directory unique to this test run.
std::filesystem::path tempDir() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        "nodo_gap1_checkpoint_test";
    std::filesystem::create_directories(dir);
    return dir;
}

void testCheckpointRoundTrip() {
    const std::filesystem::path dir = tempDir();
    PersistentSyncCheckpointStore store(dir);

    const PersistentSyncCheckpoint cp(
        PersistentSyncCheckpoint::SCHEMA_VERSION,
        42,
        "abc123blockHash",
        "xyz789stateRoot",
        PersistentSyncStatus::COMPLETE,
        "peer-node-1",
        kNow
    );

    const auto writeResult = store.save(cp);
    require(writeResult.saved(), "Checkpoint save must succeed.");
    require(store.exists(), "Checkpoint file must exist after save.");

    const auto readResult = store.read();
    require(readResult.loaded(), "Checkpoint must load successfully after save.");

    const auto& loaded = readResult.checkpoint();
    require(loaded.finalizedHeight() == 42,
            "Loaded height must match saved height.");
    require(loaded.finalizedBlockHash() == "abc123blockHash",
            "Loaded block hash must match saved block hash.");
    require(loaded.finalizedStateRoot() == "xyz789stateRoot",
            "Loaded state root must match saved state root.");
    require(loaded.sourcePeerId() == "peer-node-1",
            "Loaded peer ID must match saved peer ID.");
    require(loaded.status() == PersistentSyncStatus::COMPLETE,
            "Loaded status must match saved status.");
}

void testCheckpointAdvancesAcrossMultipleSaves() {
    const std::filesystem::path dir = tempDir();
    PersistentSyncCheckpointStore store(dir);

    for (std::uint64_t h = 1; h <= 5; ++h) {
        const PersistentSyncCheckpoint cp(
            PersistentSyncCheckpoint::SCHEMA_VERSION,
            h,
            "hash-" + std::to_string(h),
            "root-" + std::to_string(h),
            PersistentSyncStatus::COMPLETE,
            "peer",
            kNow
        );
        require(store.save(cp).saved(),
                "Checkpoint save must succeed for height " + std::to_string(h));
    }

    const auto readResult = store.read();
    require(readResult.loaded(), "Checkpoint must be loadable after multiple saves.");
    require(readResult.checkpoint().finalizedHeight() == 5,
            "Final checkpoint height must be 5 (last written).");
    require(readResult.checkpoint().finalizedBlockHash() == "hash-5",
            "Final checkpoint block hash must correspond to height 5.");
}

void testMissingCheckpointReturnsNotLoaded() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        "nodo_gap1_checkpoint_missing_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    PersistentSyncCheckpointStore store(dir);
    require(!store.exists(), "Store must report no file in fresh directory.");

    const auto readResult = store.read();
    require(!readResult.loaded(), "Missing checkpoint must not report as loaded.");
}

} // namespace

int main() {
    try {
        testCheckpointRoundTrip();
        testCheckpointAdvancesAcrossMultipleSaves();
        testMissingCheckpointReturnsNotLoaded();

        std::cout << "Nodo Gap1 checkpoint persistence tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Nodo Gap1 checkpoint persistence tests failed: "
                  << e.what() << "\n";
        return 1;
    }
}
