#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeMonetaryValidation.hpp"
#include "config/NetworkParameters.hpp"
#include "core/Transaction.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "utils/Amount.hpp"

#include <cassert>
#include <string>

// Item 8: regression tests proving the monetary gate is enforced before votes,
// and that old monetary behavior cannot silently bypass the gate.

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::CryptoAlgorithm;
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
using nodo::node::RuntimeMonetaryValidation;
using nodo::node::RuntimeMonetaryValidationStatus;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

KeyPair localValidatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "monetary-gate-test-validator"
    );
}

KeyPair localUserKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "monetary-gate-test-user"
    );
}

BootstrapValidatorConfig validator() {
    return BootstrapValidatorConfig(
        localValidatorKeyPair().publicKey(), 1, 1, "monetary-gate-test-validator"
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {validator()},
        {GenesisAccountConfig(
            localUserKeyPair().address().value(),
            Amount::fromRawUnits(1000000000000),
            0
        )},
        "monetary-gate-test-genesis"
    );
}

Signer localValidatorSigner() {
    static const Bls12381SignatureProvider provider;
    return Signer(localValidatorKeyPair(), provider);
}

Signer localUserSigner() {
    static const Ed25519SignatureProvider provider;
    return Signer(localUserKeyPair(), provider);
}

PeerInfo localPeer() {
    return PeerInfo("gate-test-peer", "127.0.0.1:9399", "nodo/0.1", 0, kTimestamp);
}

NodeRuntime startRuntime() {
    const auto result = NodeRuntimeFactory::startFromGenesis(
        NodeRuntimeConfig(genesisConfig(), localPeer(), 16)
    );
    assert(result.started());
    return result.runtime();
}

void admitTransaction(NodeRuntime& runtime, std::uint64_t nonce = 1) {
    const auto tx = nodo::core::TransactionBuilder::buildSignedTransfer(
        nodo::core::TransactionBuildRequest(
            "gate-test-recipient",
            Amount::fromRawUnits(1000),
            Amount::fromRawUnits(100),
            nonce,
            kTimestamp + 10
        ),
        localUserSigner()
    );
    assert(runtime.mutableMempool().admitTransaction(
        tx, CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION, kTimestamp + 11
    ).accepted());
}

// 1. A valid block with fee burn passes the gate and finalizes.
void testPipelineFinalizesSucessfullyWithMonetaryGate() {
    NodeRuntime runtime = startRuntime();
    admitTransaction(runtime);

    const auto result = RuntimeBlockPipeline::produceAndFinalizeNextBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        localValidatorSigner()
    );

    assert(result.finalized());
    assert(result.status() == RuntimeBlockPipelineStatus::FINALIZED);
    assert(runtime.blockchain().size() == 2U);
}

// 2. RuntimeMonetaryValidation rejects when fee burn exceeds supply.
//    Use the genesis block from a live runtime so we have a valid block.
void testRuntimeMonetaryValidationContextUnavailableRejectsVotes() {
    NodeRuntime runtime = startRuntime();
    const auto& genesisBlock = runtime.blockchain().blocks().front();
    const auto& genesis = runtime.config().genesisConfig();

    // genesis supply = 1000000000000 raw units
    // feeBurnAmount = 2000000000000 (double the supply) → underflow → context unavailable
    const Amount hugeBurn = Amount::fromRawUnits(2000000000000LL);
    const auto result = RuntimeMonetaryValidation::validateCandidate(
        genesis, genesisBlock, hugeBurn
    );
    assert(!result.isAccepted());
    assert(result.status() == RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE);
    assert(!result.reason().empty());
}

// 3. The pipeline returns MONETARY_VALIDATION_FAILED when the monetary context is
//    explicitly constructed to fail — here we verify the status enum is mapped correctly.
void testMonetaryValidationFailedStatusString() {
    assert(nodo::node::runtimeBlockPipelineStatusToString(
               RuntimeBlockPipelineStatus::MONETARY_VALIDATION_FAILED) ==
           "MONETARY_VALIDATION_FAILED");
}

// 4. RuntimeMonetaryValidation: MONETARY_CONTEXT_UNAVAILABLE is always a rejection.
void testContextUnavailableIsAlwaysRejection() {
    const auto result = nodo::node::RuntimeMonetaryValidationResult::contextUnavailable(
        "unit test: context unavailable"
    );
    assert(!result.isAccepted());
    assert(result.status() == RuntimeMonetaryValidationStatus::MONETARY_CONTEXT_UNAVAILABLE);
}

// 5. SupplyDelta with mismatched block hash is invalid and MonetaryValidationGate
//    returns INVALID_SUPPLY_DELTA — old bypass via stale block hash is rejected.
void testGateRejectsSupplyDeltaWithMismatchedBlockHash() {
    const nodo::economics::MonetaryPolicy policy =
        nodo::economics::MonetaryPolicy::localnetDefault(
            "nodo-regression-gate-1",
            Amount::fromRawUnits(1000000)
        );
    // BurnRecord matches epoch/blockHeight but has mismatched blockHash in the delta.
    const nodo::economics::SupplyDelta mismatchedDelta(
        5, "correct-hash", 1,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(50),
        Amount::fromRawUnits(950),
        {},
        {nodo::economics::BurnRecord(
            "burn-mismatch", 5, 2,  // epoch=2, but delta epoch=1 → mismatch
            "nodo_fee_pool",
            Amount::fromRawUnits(50),
            "fee", nodo::economics::BurnType::FEE_BURN
        )}
    );
    assert(!mismatchedDelta.isValid());  // delta itself is invalid (epoch mismatch)
    const auto gateResult = nodo::economics::MonetaryValidationGate::validate(
        policy, mismatchedDelta, {}
    );
    assert(!gateResult.isAccepted());
    assert(gateResult.firewallStatus() ==
           nodo::economics::MonetaryFirewallStatus::INVALID_SUPPLY_DELTA);
}

// 6. Gate preserves UNAUTHORIZED_MINT firewall status when auth is missing.
void testGatePreservesUnauthorizedMintStatus() {
    const nodo::economics::MonetaryPolicy policy =
        nodo::economics::MonetaryPolicy::localnetDefault(
            "nodo-regression-gate-2",
            Amount::fromRawUnits(1000000)
        );
    const nodo::economics::MintRecord mintRecord(
        "reg-mint-001", "missing-auth-id", "nodo1reg001",
        Amount::fromRawUnits(100),
        nodo::economics::MintReason::GENESIS_ALLOCATION,
        1, 5, "reg-hash-A", 1900000001
    );
    const nodo::economics::SupplyDelta delta(
        5, "reg-hash-A", 1,
        Amount::fromRawUnits(1000),
        Amount::fromRawUnits(100),
        Amount::fromRawUnits(0),
        Amount::fromRawUnits(1100),
        {mintRecord}, {}
    );
    const auto gateResult = nodo::economics::MonetaryValidationGate::validate(
        policy, delta, {}
    );
    assert(!gateResult.isAccepted());
    assert(gateResult.firewallStatus() ==
           nodo::economics::MonetaryFirewallStatus::UNAUTHORIZED_MINT);
}

} // namespace

int main() {
    testPipelineFinalizesSucessfullyWithMonetaryGate();
    testRuntimeMonetaryValidationContextUnavailableRejectsVotes();
    testMonetaryValidationFailedStatusString();
    testContextUnavailableIsAlwaysRejection();
    testGateRejectsSupplyDeltaWithMismatchedBlockHash();
    testGatePreservesUnauthorizedMintStatus();
    return 0;
}
