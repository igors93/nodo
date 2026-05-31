#include "node/RuntimeBlockPipeline.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/LocalSignatureProvider.hpp"
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
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Transaction;
using nodo::core::TransactionType;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::CryptoPolicy;
using nodo::crypto::KeyPair;
using nodo::crypto::LocalSignatureProvider;
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
    return KeyPair::createDevelopmentKeyPair(
        "runtime-block-pipeline-validator"
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
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator("runtime-block-pipeline-validator")
        },
        "runtime-block-pipeline-genesis"
    );
}

Signer localValidatorSigner() {
    static const LocalSignatureProvider provider;

    return Signer(
        localValidatorKeyPair(),
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
    std::int64_t timestamp
) {
    (void)suffix;

    return nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "runtime-pipeline-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce,
            timestamp
        ),
        localValidatorSigner()
    );
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
