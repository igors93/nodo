#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/Signer.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
using nodo::crypto::Signer;
using nodo::crypto::SignatureBundle;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::node::RuntimeBlockPipelineStatus;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

KeyPair localValidatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "runtime-block-pipeline-validator"
    );
}

KeyPair localUserKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "runtime-block-pipeline-user"
    );
}

BootstrapValidatorConfig validator(
    const std::string& metadata
) {
    return BootstrapValidatorConfig(
        localValidatorKeyPair().publicKey(),
        1,
        1,
        metadata
    );
}

GenesisConfig genesisConfig() {
    const BootstrapValidatorConfig bootstrap =
        validator("runtime-block-pipeline-validator");

    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            bootstrap
        },
        {
            GenesisAccountConfig(
                localUserKeyPair().address().value(),
                Amount::fromRawUnits(1000000000000),
                0
            )
        },
        "runtime-block-pipeline-genesis"
    );
}

Signer localValidatorSigner() {
    static const Bls12381SignatureProvider provider;

    return Signer(
        localValidatorKeyPair(),
        provider
    );
}

Signer localUserSigner() {
    static const Ed25519SignatureProvider provider;

    return Signer(
        localUserKeyPair(),
        provider
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "runtime-pipeline-peer",
        "127.0.0.1:9300",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

NodeRuntime startRuntime() {
    const auto result =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                localPeer(),
                16
            )
        );

    requireCondition(
        result.started(),
        "Runtime should start from genesis."
    );

    return result.runtime();
}

Transaction signedTransfer(
    const std::string& suffix,
    std::uint64_t nonce,
    std::int64_t timestamp,
    std::int64_t amountRawUnits = 1000
) {
    (void)suffix;

    return nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "runtime-pipeline-recipient",
            Amount::fromRawUnits(amountRawUnits),
            Amount::fromRawUnits(100),
            nonce,
            timestamp
        ),
        localUserSigner()
    );
}

void admitTransaction(
    NodeRuntime& runtime,
    std::uint64_t nonce = 1,
    std::int64_t amountRawUnits = 1000
) {
    const Transaction transaction =
        signedTransfer(
            "a",
            nonce,
            kTimestamp + 10,
            amountRawUnits
        );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            transaction,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "Transaction should be admitted to mempool."
    );
}

void testRejectsEconomicallyInvalidTransactionBeforeFinalization() {
    NodeRuntime runtime =
        startRuntime();

    admitTransaction(
        runtime,
        1,
        1000000000001
    );

    const auto result =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 20
            ),
            localValidatorSigner()
        );

    requireCondition(
        result.status() == RuntimeBlockPipelineStatus::STATE_TRANSITION_FAILED,
        "Pipeline should reject economically invalid transactions before voting/finalization."
    );

    requireCondition(
        runtime.blockchain().size() == 1U &&
        !runtime.mempool().empty(),
        "Rejected economic transition should not append a block or remove mempool transactions."
    );
}

void testProducesFinalizesAndCleansMempool() {
    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto result =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 20
            ),
            localValidatorSigner()
        );

    requireCondition(
        result.finalized(),
        "Runtime pipeline should finalize next block."
    );

    requireCondition(
        runtime.blockchain().size() == 2U,
        "Finalized block should append to runtime blockchain."
    );

    requireCondition(
        runtime.finalizationRegistry().isFinalizedBlock(
            result.block().index(),
            result.block().hash()
        ),
        "Finalized block should be registered."
    );

    requireCondition(
        runtime.mempool().empty(),
        "Finalized transactions should be removed from mempool."
    );

    requireCondition(
        result.certificate().round() == 1 &&
        runtime.consensusRoundManager().currentState().height() == 2 &&
        runtime.consensusRoundManager().currentState().round() == 1,
        "Finalized block pipeline should use and advance runtime consensus round state."
    );
}

void testRejectsEmptyMempool() {
    NodeRuntime runtime =
        startRuntime();

    const auto result =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                1,
                kTimestamp + 30
            ),
            localValidatorSigner()
        );

    requireCondition(
        result.status() == RuntimeBlockPipelineStatus::BLOCK_PRODUCTION_FAILED,
        "Pipeline should reject empty mempool."
    );
}

void testRejectsInvalidConfig() {
    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto result =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                0,
                1,
                1,
                kTimestamp + 40
            ),
            localValidatorSigner()
        );

    requireCondition(
        result.status() == RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Pipeline should reject invalid config."
    );
}

void testRejectsConsensusRoundMismatch() {
    NodeRuntime runtime =
        startRuntime();

    admitTransaction(runtime);

    const auto result =
        RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            RuntimeBlockPipelineConfig(
                100,
                1,
                2,
                kTimestamp + 50
            ),
            localValidatorSigner()
        );

    requireCondition(
        result.status() == RuntimeBlockPipelineStatus::VOTE_BUILD_FAILED,
        "Pipeline should reject a block attempt outside the active consensus round."
    );

    requireCondition(
        runtime.blockchain().size() == 1U &&
        !runtime.mempool().empty(),
        "Consensus round mismatch should not append a block or clear mempool."
    );
}

} // namespace

int main() {
    try {
        testProducesFinalizesAndCleansMempool();
        testRejectsEconomicallyInvalidTransactionBeforeFinalization();
        testRejectsEmptyMempool();
        testRejectsInvalidConfig();
        testRejectsConsensusRoundMismatch();

        std::cout << "Nodo runtime block pipeline tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo runtime block pipeline tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
