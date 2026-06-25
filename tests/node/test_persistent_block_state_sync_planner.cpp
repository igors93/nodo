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

    auto snapshotPlan = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        farAhead,
        "node-a",
        "node-b",
        512,
        100,
        now + 3
    );

    assert(snapshotPlan.requestSnapshot());
    assert(snapshotPlan.snapshotRequestManifest().has_value());
    assert(snapshotPlan.snapshotRequestManifest()->snapshotHeight() == 10000);

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

    // Planner uses local manifest when remote is far ahead and local manifest
    // covers height beyond the current checkpoint.
    PersistentSnapshotSyncManifest localManifest(
        "local-chain",
        500,
        "epoch-snap-block-hash",
        "epoch-snap-state-root",
        "epoch-snap-digest",
        now + 20
    );

    auto planWithLocalManifest = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        farAhead,
        "node-a",
        "node-b",
        512,
        100,
        now + 30,
        localManifest
    );

    assert(planWithLocalManifest.requestSnapshot());
    assert(planWithLocalManifest.snapshotRequestManifest().has_value());
    // The plan should carry the local manifest data, not remote placeholder values.
    assert(planWithLocalManifest.snapshotRequestManifest()->snapshotHeight() == 500);
    assert(planWithLocalManifest.snapshotRequestManifest()->snapshotDigest() == "epoch-snap-digest");
    assert(planWithLocalManifest.snapshotRequestManifest()->snapshotStateRoot() == "epoch-snap-state-root");

    // Planner falls back to remote info when local manifest height <= checkpoint height.
    PersistentSnapshotSyncManifest staleManifest(
        "local-chain",
        0,  // height 0 is not > localCheckpoint.finalizedHeight() (0), so not used
        "stale-block-hash",
        "stale-state-root",
        "stale-digest",
        now + 5
    );

    // staleManifest has height 0 which is NOT > checkpoint.finalizedHeight() (0),
    // so the planner must fall back to the remote placeholder path.
    // (snapshotHeight=0 also makes isValid() false, ensuring the guard triggers.)
    auto planWithStaleManifest = PersistentBlockStateSyncPlanner::planFromRemoteStatus(
        checkpoint,
        farAhead,
        "node-a",
        "node-b",
        512,
        100,
        now + 40,
        staleManifest
    );

    assert(planWithStaleManifest.requestSnapshot());
    assert(planWithStaleManifest.snapshotRequestManifest().has_value());
    // Falls back to remote data since stale manifest is invalid / height not ahead.
    assert(planWithStaleManifest.snapshotRequestManifest()->snapshotHeight() == 10000);

    std::cout << "persistent block/state sync planner tests passed\n";
    return 0;
}
