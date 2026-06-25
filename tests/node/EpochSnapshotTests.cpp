#include "config/NetworkParameters.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "core/TransactionBuilder.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900000000;

crypto::KeyPair validatorKey() {
    return crypto::KeyPair::createDeterministicBls12381KeyPair("epoch-snap-v");
}

crypto::KeyPair userKey() {
    return crypto::KeyPair::createDeterministicEd25519KeyPair("epoch-snap-u");
}

config::GenesisConfig buildGenesis(
    const crypto::KeyPair& vk,
    const crypto::KeyPair& uk
) {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(vk.publicKey(), 1, 1, "epoch-snap-validator") },
        { config::GenesisAccountConfig(uk.address().value(), utils::Amount::fromRawUnits(5000000000000LL), 0) },
        "epoch-snapshot-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("epoch-snap-node", "127.0.0.1:19999", "nodo/test", 0, kTs);
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 10)
    );
    assert(result.started());
    return result.runtime();
}

node::RuntimeBlockPipelineResult produceBlock(
    node::NodeRuntime& rt,
    const crypto::KeyPair& vk,
    const crypto::KeyPair& uk,
    std::int64_t ts,
    std::uint64_t nonce
) {
    const crypto::Ed25519SignatureProvider userProv;
    const crypto::Bls12381SignatureProvider valProv;

    const auto tx = core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "epoch-snap-recipient",
            utils::Amount::fromRawUnits(1),
            utils::Amount::fromRawUnits(1),
            nonce,
            ts - 10
        ),
        crypto::Signer(uk, userProv)
    );

    rt.mutableMempool().admitTransaction(
        tx,
        crypto::CryptoPolicy::developmentPolicy(),
        crypto::SecurityContext::USER_TRANSACTION,
        ts - 9
    );

    return node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        rt,
        node::RuntimeBlockPipelineConfig(100, 1, 1, ts),
        crypto::Signer(vk, valProv)
    );
}

// Non-epoch blocks have an empty snapshotDigest.
void testNonEpochBlockHasNoSnapshotDigest() {
    const auto vk = validatorKey();
    const auto uk = userKey();
    const auto genesis = buildGenesis(vk, uk);
    auto rt = startRuntime(genesis);

    const auto result = produceBlock(rt, vk, uk, kTs + 1, 1);
    assert(result.finalized());
    // Block 1 is far from an epoch boundary (43,200 blocks away).
    assert(result.snapshotDigest().empty());
}

// StatePruner records the finalized state root for the block.
void testStatePrunerRecordsStateRootAfterBlock() {
    const auto vk = validatorKey();
    const auto uk = userKey();
    const auto genesis = buildGenesis(vk, uk);
    auto rt = startRuntime(genesis);

    assert(!rt.statePruner().hasStateRoot(1));

    const auto result = produceBlock(rt, vk, uk, kTs + 1, 1);
    assert(result.finalized());

    // After the block is finalized the pruner holds the state root.
    assert(rt.statePruner().hasStateRoot(1));
    assert(rt.statePruner().getStateRoot(1) == result.postStateRoot());
}

// Pruner trims entries beyond the keep window.
void testStatePrunerPrunesOldEntries() {
    const auto vk = validatorKey();
    const auto uk = userKey();
    const auto genesis = buildGenesis(vk, uk);
    auto rt = startRuntime(genesis);

    // Manually record many state roots at low heights and then call prune.
    auto& pruner = rt.mutableStatePruner();
    for (std::uint64_t h = 1; h <= 50; ++h) {
        pruner.recordStateRoot(h, "root-" + std::to_string(h));
    }
    // Default window is 100, so nothing pruned yet.
    pruner.pruneHistory(50);
    assert(pruner.prunedCount() == 0);

    // Shrink the pruner window via a fresh instance to verify pruning logic.
    nodo::core::StatePruner smallPruner(5);
    for (std::uint64_t h = 1; h <= 20; ++h) {
        smallPruner.recordStateRoot(h, "root-" + std::to_string(h));
    }
    smallPruner.pruneHistory(20);   // threshold = 20 - 5 = 15 → prune 1..14
    assert(!smallPruner.hasStateRoot(1));
    assert(!smallPruner.hasStateRoot(14));
    assert(smallPruner.hasStateRoot(15));
    assert(smallPruner.hasStateRoot(20));
    assert(smallPruner.prunedCount() == 14);
}

} // namespace

int main() {
    try {
        testNonEpochBlockHasNoSnapshotDigest();
        testStatePrunerRecordsStateRootAfterBlock();
        testStatePrunerPrunesOldEntries();
        std::cout << "Epoch snapshot and state pruner tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
