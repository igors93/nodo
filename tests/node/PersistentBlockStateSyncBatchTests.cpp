#include "node/PersistentBlockStateSync.hpp"

#include "core/ValidatorRegistry.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace nodo::node;
using namespace nodo::core;
using namespace nodo::crypto;

int main() {
    const std::int64_t now = 1700000100;
    const std::string genesisHash(64, '0');
    const std::string block1Hash(64, '1');
    const std::string block2Hash(64, '2');
    const std::string block3Hash(64, '3');
    const std::string stateRoot1(64, 'a');
    const std::string stateRoot2(64, 'b');
    const std::string stateRoot3(64, 'c');

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
        block1Hash,
        genesisHash,
        "Block{index=1}",
        stateRoot1,
        now + 1,
        "FINALIZED_RECORD_1"
    );

    PersistentBlockSyncItem item2(
        2,
        block2Hash,
        block1Hash,
        "Block{index=2}",
        stateRoot2,
        now + 2,
        "FINALIZED_RECORD_2"
    );

    PersistentBlockSyncItem missingFinalityRecord(
        1,
        block1Hash,
        genesisHash,
        "Block{index=1}",
        stateRoot1,
        now + 1
    );
    assert(missingFinalityRecord.isValid());
    assert(missingFinalityRecord.serializedFinalizedRecord().empty());

    PersistentBlockSyncItem itemWithRecord(
        3,
        block3Hash,
        block2Hash,
        "Block{index=3}",
        stateRoot3,
        now + 3,
        "FINALIZED_RECORD_PAYLOAD"
    );

    assert(!itemWithRecord.serializedFinalizedRecord().empty());
    assert(itemWithRecord.serializedFinalizedRecord() == "FINALIZED_RECORD_PAYLOAD");

    PersistentBlockSyncBatch batch(
        "node-b",
        1,
        1,
        {item1},
        now + 3
    );

    assert(item1.isValid());
    assert(item2.isValid());
    assert(batch.isValid());
    assert(batch.blockCount() == 1);
    assert(batch.lastItem() != nullptr);
    assert(batch.lastItem()->blockHash() == block1Hash);

    PersistentBlockSyncBatch oversizedBatch(
        "node-b",
        1,
        2,
        {item1, item2},
        now + 4
    );
    assert(!oversizedBatch.isValid());

    PersistentBlockSyncItem broken(
        2,
        std::string(64, '4'),
        std::string(64, '9'),
        "Block{index=2}",
        stateRoot2,
        now + 5,
        "FINALIZED_RECORD_BROKEN"
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
        block1Hash,
        genesisHash,
        "Block{index=1}",
        stateRoot1,
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

    assert(batchBadRecord.isValid());

    PersistentBlockSyncBatch impossibleChronology(
        "node-b",
        1,
        1,
        {item1},
        now
    );
    assert(!impossibleChronology.isValid());

    std::cout << "persistent block/state sync batch tests passed\n";
    return 0;
}
