#include "node/RuntimeBlockPipeline.hpp"
#include "node/NodeRuntime.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

// Item 8: regression tests proving cumulative supply accounting works.

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::node::RuntimeBlockPipelineStatus;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair("supply-delta-test-validator");
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("supply-delta-test-user");
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {BootstrapValidatorConfig(validatorKeyPair().publicKey(), 1, 1, "sd-validator")},
        {GenesisAccountConfig(userKeyPair().address().value(), Amount::fromRawUnits(1000000000000LL), 0)},
        "sd-test-genesis"
    );
}

Signer validatorSigner() {
    static const Bls12381SignatureProvider p;
    return Signer(validatorKeyPair(), p);
}

Signer userSigner() {
    static const Ed25519SignatureProvider p;
    return Signer(userKeyPair(), p);
}

NodeRuntime startRuntime() {
    const auto result = NodeRuntimeFactory::startFromGenesis(
        NodeRuntimeConfig(genesisConfig(),
            PeerInfo("sd-peer", "127.0.0.1:9500", "nodo/0.1", 0, kTimestamp), 16)
    );
    assert(result.started());
    return result.runtime();
}

void admitTransfer(NodeRuntime& runtime, std::uint64_t nonce) {
    const auto tx = nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "sd-recipient", Amount::fromRawUnits(1000), Amount::fromRawUnits(100),
            nonce, kTimestamp + 10
        ),
        userSigner()
    );
    assert(runtime.mutableMempool().admitTransaction(
        tx, CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION, kTimestamp + 11
    ).accepted());
}

// 1. Pipeline finalized result exposes a valid SupplyDelta.
void testPipelineFinalizedResultHasValidSupplyDelta() {
    NodeRuntime runtime = startRuntime();
    admitTransfer(runtime, 1);

    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    assert(result.finalized());
    assert(result.supplyDelta().isValid());
}

// 2. Pipeline SupplyDelta.burnedAmount > 0 for a block with fee.
void testPipelineDeltaHasNonZeroBurnForFeeBlock() {
    NodeRuntime runtime = startRuntime();
    admitTransfer(runtime, 1);

    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    assert(result.finalized());
    // Fee=100 raw, burn split=20%, so burnedAmount=20.
    assert(!result.supplyDelta().burnedAmount().isZero());
}

// 3. Two blocks do not reset supplyBefore to genesis on second block.
void testTwoBlocksSupplyChainsCorrectly() {
    NodeRuntime runtime = startRuntime();

    // Block 1.
    admitTransfer(runtime, 1);
    const auto result1 = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    assert(result1.finalized());
    const Amount supplyAfterBlock1 = result1.supplyDelta().supplyAfter();

    // Block 2 — after finalization, the consensus round resets to 1 for the next height.
    admitTransfer(runtime, 2);
    const auto result2 = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 30),
        validatorSigner()
    );
    assert(result2.finalized());

    // supplyBefore for block 2 must equal supplyAfter of block 1.
    assert(result2.supplyDelta().supplyBefore() == supplyAfterBlock1);
}

// 4. NodeRuntime supply state tracks latest supply after finalization.
void testNodeRuntimeSupplyStateIsUpdatedAfterFinalization() {
    NodeRuntime runtime = startRuntime();
    const Amount genesisSupply = runtime.supplyState().latestSupply();
    assert(!genesisSupply.isZero());

    admitTransfer(runtime, 1);
    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    assert(result.finalized());

    // After finalization, latest supply should be lower than genesis (fee was burned).
    assert(runtime.supplyState().latestSupply() < genesisSupply);
    assert(runtime.supplyState().latestSupply() == result.supplyDelta().supplyAfter());
}

// 5. SupplyDelta blockHash matches the finalized block hash.
void testSupplyDeltaBlockHashMatchesFinalizedBlock() {
    NodeRuntime runtime = startRuntime();
    admitTransfer(runtime, 1);
    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    assert(result.finalized());
    assert(result.supplyDelta().blockHash() == result.block().hash());
    assert(result.supplyDelta().blockHeight() == result.block().index());
}

} // namespace

int main() {
    testPipelineFinalizedResultHasValidSupplyDelta();
    testPipelineDeltaHasNonZeroBurnForFeeBlock();
    testTwoBlocksSupplyChainsCorrectly();
    testNodeRuntimeSupplyStateIsUpdatedAfterFinalization();
    testSupplyDeltaBlockHashMatchesFinalizedBlock();
    return 0;
}
