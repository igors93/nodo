#include "node/PersistentBlockStateSync.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace nodo::node;

int main() {
    const auto tempDir = std::filesystem::temp_directory_path() /
        "nodo_persistent_sync_store_test";

    std::filesystem::remove_all(tempDir);

    PersistentSyncCheckpointStore store(tempDir);
    assert(!store.exists());
    assert(!store.load().has_value());

    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "genesis-hash",
        "genesis-state-root",
        1700000200
    );

    store.save(checkpoint);
    assert(store.exists());

    auto loaded = store.load();
    assert(loaded.has_value());
    assert(loaded->isValid());
    assert(loaded->finalizedHeight() == 0);
    assert(loaded->finalizedBlockHash() == "genesis-hash");
    assert(loaded->finalizedStateRoot() == "genesis-state-root");

    const std::string serialized = checkpoint.serialize();
    auto deserialized = PersistentSyncCheckpoint::deserialize(serialized);
    assert(deserialized.isValid());
    assert(deserialized.finalizedBlockHash() == checkpoint.finalizedBlockHash());

    std::filesystem::remove_all(tempDir);

    std::cout << "persistent block/state sync store tests passed\n";
    return 0;
}
