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

    std::cout << "persistent block/state sync planner tests passed\n";
    return 0;
}
