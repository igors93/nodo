#include "config/NetworkParameters.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/PeerMessage.hpp"
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
using nodo::core::TransactionBuildRequest;
using nodo::core::TransactionBuilder;
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
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp  = 1900000000;
constexpr std::int64_t kStake      = 1'000'000;
constexpr std::int64_t kFee        = 100;
const std::string kChainId         = "nodo-localnet-1";

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair("staking-writeback-validator");
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("staking-writeback-user");
}

Signer validatorSigner() {
    static const Bls12381SignatureProvider provider;
    return Signer(validatorKeyPair(), provider);
}

Signer userSigner() {
    static const Ed25519SignatureProvider provider;
    return Signer(userKeyPair(), provider);
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            BootstrapValidatorConfig(
                validatorKeyPair().publicKey(),
                1, 1,
                "staking-writeback-validator"
            )
        },
        {
            GenesisAccountConfig(
                userKeyPair().address().value(),
                Amount::fromRawUnits(1'000'000'000'000LL),
                0
            )
        },
        "staking-writeback-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "staking-writeback-peer",
        "127.0.0.1:9901",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

NodeRuntime startRuntime() {
    const auto result = NodeRuntimeFactory::startFromGenesis(
        NodeRuntimeConfig(genesisConfig(), localPeer(), 16)
    );
    requireCondition(result.started(), "Runtime should start from genesis.");
    return result.runtime();
}

// Improvement 1: applyCertifiedBlock must write the post-block StakingRegistry
// back to the runtime. Verifies that a STAKE_DEPOSIT committed in a block is
// immediately reflected in runtime.stakingRegistry().
void testStakeDepositUpdatesRegistryAfterBlock() {
    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    requireCondition(
        runtime.stakingRegistry().accountOrDefault(validatorAddr).bondedAmount().rawUnits() == 0,
        "Validator should have zero bonded stake before any deposit."
    );

    const Transaction tx = TransactionBuilder::buildSignedStakeLock(
        TransactionBuildRequest(
            validatorAddr,
            Amount::fromRawUnits(kStake),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    requireCondition(
        runtime.mutableMempool().admitTransaction(
            tx,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "STAKE_DEPOSIT should be admitted to mempool."
    );

    const auto pipeline = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );

    requireCondition(
        pipeline.finalized(),
        "Block with STAKE_DEPOSIT should finalize. Status="
            + nodo::node::runtimeBlockPipelineStatusToString(pipeline.status())
            + " Reason=" + pipeline.reason()
    );

    const std::int64_t bonded =
        runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits();

    requireCondition(
        bonded == kStake,
        "StakingRegistry must reflect bonded amount after block. Got: " + std::to_string(bonded)
    );
}

// Improvement 1 (continued): two consecutive blocks — STAKE_DEPOSIT then STAKE_TOP_UP —
// must accumulate bonded amounts correctly.
void testStakeTopUpAccumulatesAcrossConsecutiveBlocks() {
    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    // Block 1: initial deposit
    {
        const Transaction tx = TransactionBuilder::buildSignedStakeLock(
            TransactionBuildRequest(
                validatorAddr,
                Amount::fromRawUnits(kStake),
                Amount::fromRawUnits(kFee),
                1,
                kTimestamp + 10
            ),
            userSigner(),
            kChainId
        );
        requireCondition(
            runtime.mutableMempool().admitTransaction(
                tx, CryptoPolicy::developmentPolicy(),
                SecurityContext::USER_TRANSACTION, kTimestamp + 11
            ).accepted(),
            "Block 1: STAKE_DEPOSIT should be admitted."
        );
        requireCondition(
            RuntimeBlockPipeline::produceAndFinalizeNextBlock(
                runtime,
                RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
                validatorSigner()
            ).finalized(),
            "Block 1 should finalize."
        );
    }

    requireCondition(
        runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits() == kStake,
        "After block 1: bonded should equal the initial deposit."
    );

    // Block 2: top-up (round resets to 1 at the new height)
    {
        const Transaction tx = TransactionBuilder::buildSignedStakeTopUp(
            TransactionBuildRequest(
                validatorAddr,
                Amount::fromRawUnits(kStake),
                Amount::fromRawUnits(kFee),
                2,
                kTimestamp + 30
            ),
            userSigner(),
            kChainId
        );
        requireCondition(
            runtime.mutableMempool().admitTransaction(
                tx, CryptoPolicy::developmentPolicy(),
                SecurityContext::USER_TRANSACTION, kTimestamp + 31
            ).accepted(),
            "Block 2: STAKE_TOP_UP should be admitted."
        );
        requireCondition(
            RuntimeBlockPipeline::produceAndFinalizeNextBlock(
                runtime,
                RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 40),
                validatorSigner()
            ).finalized(),
            "Block 2 should finalize."
        );
    }

    const std::int64_t bonded =
        runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits();

    requireCondition(
        bonded == kStake * 2,
        "After block 2: bonded must equal deposit + top-up. Got: " + std::to_string(bonded)
    );
}

} // namespace

int main() {
    try {
        testStakeDepositUpdatesRegistryAfterBlock();
        testStakeTopUpAccumulatesAcrossConsecutiveBlocks();
        std::cout << "Staking registry writeback tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Staking registry writeback tests failed: " << error.what() << "\n";
        return 1;
    }
}
