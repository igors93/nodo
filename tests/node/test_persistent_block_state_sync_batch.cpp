#include "node/PersistentBlockStateSync.hpp"

#include "core/ValidatorRegistry.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::node;
using namespace nodo::core;
using namespace nodo::crypto;

int main() {
    const std::int64_t now = 1700000100;

    const ValidatorRegistry registry;
    const CryptoPolicy policy = CryptoPolicy::developmentPolicy();
    const Bls12381SignatureProvider provider;

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

    assert(item1.serializedFinalizedRecord().empty());
    assert(item2.serializedFinalizedRecord().empty());

    PersistentBlockSyncItem itemWithRecord(
        3,
        "block-3",
        "block-2",
        "Block{index=3}",
        "state-root-3",
        now + 3,
        "FINALIZED_RECORD_PAYLOAD"
    );

    assert(!itemWithRecord.serializedFinalizedRecord().empty());
    assert(itemWithRecord.serializedFinalizedRecord() == "FINALIZED_RECORD_PAYLOAD");

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
        registry,
        policy,
        provider,
        now + 4
    );

    // Fast-path now requires a FinalizedBlockRecord (QC proof) for every item.
    // item1 and item2 both have empty serializedFinalizedRecord so the batch
    // must be rejected — a peer cannot bypass quorum verification by omitting
    // the finalized record.
    assert(!result.applied());
    assert(!result.checkpoint().has_value());

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

    PersistentBlockSyncItem itemBadRecord(
        1,
        "block-1",
        "genesis-hash",
        "Block{index=1}",
        "state-root-1",
        now + 1,
        "NOT_VALID_SERIALIZED_RECORD_DATA_XXXX"
    );

    PersistentBlockSyncBatch batchBadRecord(
        "node-b",
        1,
        1,
        {itemBadRecord},
        now + 2
    );

    auto badRecordResult = PersistentBlockStateSyncApplier::applyValidatedBatch(
        checkpoint,
        batchBadRecord,
        registry,
        policy,
        provider,
        now + 3
    );

    assert(!badRecordResult.applied());

    std::cout << "persistent block/state sync batch tests passed\n";
    return 0;
}
