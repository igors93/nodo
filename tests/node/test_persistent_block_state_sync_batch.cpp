#include "node/PersistentBlockStateSync.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::node;

int main() {
    const std::int64_t now = 1700000100;

    PersistentSyncCheckpoint checkpoint = PersistentSyncCheckpoint::genesis(
        "genesis-hash",
        "state-root-0",
        now
    );

    PersistentBlockSyncItem item1(
        1,
        "block-1",
        "genesis-hash",
        "Block{index=1}",
        "state-root-1",
        now + 1
    );

    PersistentBlockSyncItem item2(
        2,
        "block-2",
        "block-1",
        "Block{index=2}",
        "state-root-2",
        now + 2
    );

    PersistentBlockSyncBatch batch(
        "node-b",
        1,
        2,
        {item1, item2},
        now + 3
    );

    assert(item1.isValid());
    assert(item2.isValid());
    assert(batch.isValid());
    assert(batch.blockCount() == 2);
    assert(batch.lastItem() != nullptr);
    assert(batch.lastItem()->blockHash() == "block-2");

    auto result = PersistentBlockStateSyncApplier::applyValidatedBatch(
        checkpoint,
        batch,
        now + 4
    );

    assert(result.applied());
    assert(result.checkpoint().has_value());
    assert(result.checkpoint()->finalizedHeight() == 2);
    assert(result.checkpoint()->finalizedBlockHash() == "block-2");
    assert(result.checkpoint()->finalizedStateRoot() == "state-root-2");

    PersistentBlockSyncItem broken(
        2,
        "block-2-alt",
        "wrong-parent",
        "Block{index=2}",
        "state-root-2",
        now + 5
    );

    PersistentBlockSyncBatch brokenBatch(
        "node-b",
        1,
        2,
        {item1, broken},
        now + 6
    );

    assert(!brokenBatch.isValid());

    std::cout << "persistent block/state sync batch tests passed\n";
    return 0;
}
