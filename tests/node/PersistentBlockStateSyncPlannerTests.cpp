#include "node/PersistentBlockStateSync.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::node;

int main() {
    const std::int64_t now = 1700000000;

    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "genesis-hash",
        "genesis-state-root",
        now
    );

    ChainStatusMessage sameHeight(
        "localnet",
        "chain-a",
        "proto-v1",
        0,
        "genesis-hash",
        0,
        "genesis-hash"
    );

    auto noSync = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        sameHeight,
        "node-a",
        "node-b",
        32,
        4096,
        now + 1
    );

    assert(noSync.notRequired());

    ChainStatusMessage ahead(
        "localnet",
        "chain-a",
        "proto-v1",
        10,
        "block-10",
        10,
        "block-10"
    );

    auto blockPlan = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        ahead,
        "node-a",
        "node-b",
        4,
        4096,
        now + 2
    );

    assert(blockPlan.requestBlocks());
    assert(blockPlan.blockRequest().has_value());
    assert(blockPlan.blockRequest()->requesterNodeId() == "node-a");
    assert(blockPlan.blockRequest()->locator().fromHeight() == 1);
    assert(blockPlan.blockRequest()->locator().maxBlocks() == 4);
    assert(blockPlan.blockRequest()->locator().knownAncestorHashes().front() == "genesis-hash");

    ChainStatusMessage farAhead(
        "localnet",
        "chain-a",
        "proto-v1",
        10000,
        "block-10000",
        10000,
        "block-10000"
    );

    auto farAheadPlan = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        farAhead,
        "node-a",
        "node-b",
        512,
        100,
        now + 3
    );

    assert(farAheadPlan.requestBlocks());
    assert(farAheadPlan.blockRequest().has_value());
    assert(farAheadPlan.blockRequest()->locator().maxBlocks() == 512);

    // Manifest deserialize round-trip.
    const PersistentSnapshotSyncManifest original(
        "node-origin",
        43200,
        "epoch1-block-hash",
        "epoch1-state-root",
        "epoch1-snapshot-digest",
        now + 10
    );
    const std::string serialized = original.serialize();
    const PersistentSnapshotSyncManifest roundTrip =
        PersistentSnapshotSyncManifest::deserialize(serialized);

    assert(roundTrip.sourcePeerId()      == original.sourcePeerId());
    assert(roundTrip.snapshotHeight()    == original.snapshotHeight());
    assert(roundTrip.snapshotBlockHash() == original.snapshotBlockHash());
    assert(roundTrip.snapshotStateRoot() == original.snapshotStateRoot());
    assert(roundTrip.snapshotDigest()    == original.snapshotDigest());
    assert(roundTrip.createdAt()         == original.createdAt());
    assert(roundTrip.isValid());

    std::cout << "persistent block/state sync planner tests passed\n";
    return 0;
}
