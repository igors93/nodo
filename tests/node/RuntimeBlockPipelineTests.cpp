#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::PrivateKey;
using nodo::crypto::PublicKey;
using nodo::crypto::SecurityContext;
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

PublicKey publicKey(
    const std::string& suffix
) {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "runtime-block-pipeline-public-key-" + suffix
    );
}

PrivateKey privateKey(
    const std::string& suffix
) {
    return PrivateKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "runtime-block-pipeline-private-key-" + suffix
    );
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        publicKey("validator-" + suffix),
        1,
        1,
        "runtime-block-pipeline-validator-" + suffix
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator("a"),
            validator("b")
        },
        "runtime-block-pipeline-genesis"
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
    std::int64_t timestamp
) {
    Transaction transaction(
        TransactionType::TRANSFER,
        "runtime-pipeline-sender",
        "runtime-pipeline-recipient",
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(100),
        nonce,
        timestamp
    );

    transaction.attachSignatureBundle(
        SignatureBundle::createDevelopmentSignature(
            transaction.signingPayload(),
            publicKey("tx-" + suffix),
            privateKey("tx-" + suffix),
            timestamp
        )
    );

    return transaction;
}

void admitTransaction(
    NodeRuntime& runtime
) {
    const Transaction transaction =
        signedTransfer(
            "a",
            1,
            kTimestamp + 10
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
            )
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
            )
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
            )
        );

    requireCondition(
        result.status() == RuntimeBlockPipelineStatus::INVALID_CONFIG,
        "Pipeline should reject invalid config."
    );
}

} // namespace

int main() {
    try {
        testProducesFinalizesAndCleansMempool();
        testRejectsEmptyMempool();
        testRejectsInvalidConfig();

        std::cout << "Nodo runtime block pipeline tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo runtime block pipeline tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
