#include "node/PersistentBlockStateSync.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::node;

int main() {
    const std::int64_t now = 1700000300;

    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "genesis-hash",
        "state-root-0",
        now
    );

    const auto checkpointBytes = PersistentBlockStateSyncCodec::encodeCheckpoint(checkpoint);
    const auto decodedCheckpoint = PersistentBlockStateSyncCodec::decodeCheckpoint(checkpointBytes);
    assert(decodedCheckpoint.finalizedBlockHash() == checkpoint.finalizedBlockHash());
    assert(!PersistentBlockStateSyncCodec::hashCheckpoint(checkpoint).empty());

    PersistentBlockSyncItem item(
        1,
        "block-1",
        "genesis-hash",
        "Block{index=1}",
        "state-root-1",
        now + 1
    );

    PersistentBlockSyncBatch batch(
        "node-b",
        1,
        1,
        {item},
        now + 2
    );

    const auto batchBytes = PersistentBlockStateSyncCodec::encodeBlockSyncBatch(batch);
    const auto decodedBatch = PersistentBlockStateSyncCodec::decodeBlockSyncBatch(batchBytes);
    assert(decodedBatch.isValid());
    assert(decodedBatch.fromHeight() == 1);
    assert(decodedBatch.items().front().blockHash() == "block-1");
    assert(!PersistentBlockStateSyncCodec::hashBlockSyncBatch(batch).empty());

    PersistentSnapshotSyncManifest manifest(
        "node-b",
        4096,
        "block-4096",
        "state-root-4096",
        "snapshot-digest-4096",
        now + 3
    );

    const auto manifestBytes = PersistentBlockStateSyncCodec::encodeSnapshotManifest(manifest);
    const auto decodedManifest = PersistentBlockStateSyncCodec::decodeSnapshotManifest(manifestBytes);
    assert(decodedManifest.isValid());
    assert(decodedManifest.snapshotHeight() == 4096);
    assert(!PersistentBlockStateSyncCodec::hashSnapshotManifest(manifest).empty());

    std::cout << "persistent block/state sync codec tests passed\n";
    return 0;
}
