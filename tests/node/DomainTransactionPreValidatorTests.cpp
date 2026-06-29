#include "config/NetworkParameters.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"
#include "core/TransactionBuilder.hpp"
#include "core/TransactionType.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::core::Block;
using nodo::core::LedgerRecord;
using nodo::core::StateTransitionPreview;
using nodo::core::StateTransitionPreviewStatus;
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
using nodo::node::ProtocolStateTransition;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr std::int64_t kFee       = 100;
const std::string kChainId        = "nodo-localnet-1";

void requireCondition(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair("domain-prevalidator-validator");
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair("domain-prevalidator-user");
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
                "domain-prevalidator-validator"
            )
        },
        {
            GenesisAccountConfig(
                userKeyPair().address().value(),
                Amount::fromRawUnits(1'000'000'000'000LL),
                0
            )
        },
        "domain-prevalidator-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "domain-prevalidator-peer",
        "127.0.0.1:9903",
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

Block blockWithTransaction(
    const NodeRuntime& runtime,
    const Transaction& tx
) {
    const LedgerRecord record = LedgerRecord::fromTransaction(
        tx,
        CryptoPolicy::developmentPolicy(),
        SecurityContext::USER_TRANSACTION,
        kTimestamp + 10
    );
    return Block(
        1,
        runtime.blockchain().latestBlock().hash(),
        {record},
        kTimestamp + 10
    );
}

// Improvement 3: The DomainTransactionPreValidator set by contextForNextBlockWithRegistry
// must be invoked in StateTransitionPreview::previewBlock and must reject a STAKE_WITHDRAW
// transaction when the validator has no bonded stake.
void testStakeWithdrawWithZeroBondedIsRejectedByPreValidator() {
    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    // Validator has zero bonded stake at genesis.
    requireCondition(
        runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .bondedAmount()
            .rawUnits() == 0,
        "Validator must have zero bonded stake before the test."
    );

    const Transaction tx = TransactionBuilder::buildSignedStakeWithdraw(
        TransactionBuildRequest(
            validatorAddr,
            Amount::fromRawUnits(1'000'000),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    const Block candidate = blockWithTransaction(runtime, tx);

    auto [context, tracker] =
        ProtocolStateTransition::contextForNextBlockWithRegistry(runtime, 0);

    requireCondition(
        context.hasDomainTransactionPreValidator(),
        "Context from contextForNextBlockWithRegistry must have a domain pre-validator."
    );

    const auto result = StateTransitionPreview::previewBlock(candidate, context);

    requireCondition(
        !result.accepted(),
        "previewBlock must reject STAKE_WITHDRAW when validator has zero bonded stake."
    );
    requireCondition(
        result.status() == StateTransitionPreviewStatus::INSUFFICIENT_BALANCE,
        "Rejection status must be INSUFFICIENT_BALANCE for failed domain constraint."
    );
}

// Improvement 3 (continued): VALIDATOR_UNJAIL_REQUEST for a validator that is not
// jailed must be rejected by the domain pre-validator.
void testUnjailRequestForNonJailedValidatorIsRejectedByPreValidator() {
    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    // The validator is not jailed at genesis.
    requireCondition(
        !runtime.stakingRegistry()
            .accountOrDefault(validatorAddr)
            .jailed(),
        "Validator must not be jailed before the test."
    );

    const Transaction tx = TransactionBuilder::buildSignedValidatorUnjailRequest(
        TransactionBuildRequest(
            validatorAddr,
            Amount(),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    const Block candidate = blockWithTransaction(runtime, tx);

    auto [context, tracker] =
        ProtocolStateTransition::contextForNextBlockWithRegistry(runtime, 0);

    requireCondition(
        context.hasDomainTransactionPreValidator(),
        "Context must have a domain pre-validator."
    );

    const auto result = StateTransitionPreview::previewBlock(candidate, context);

    requireCondition(
        !result.accepted(),
        "previewBlock must reject VALIDATOR_UNJAIL_REQUEST for a non-jailed validator."
    );
    requireCondition(
        result.status() == StateTransitionPreviewStatus::INSUFFICIENT_BALANCE,
        "Rejection status must be INSUFFICIENT_BALANCE for failed domain constraint."
    );
}

// Improvement 3 (continued): Without the domain pre-validator (structuralOnly context),
// the same STAKE_WITHDRAW is not rejected at the account-balance level.
// This confirms the rejection in the previous tests comes from the pre-validator,
// not from some other check.
void testStakeWithdrawPassesStructuralPreviewWithoutPreValidator() {
    NodeRuntime runtime = startRuntime();
    const std::string validatorAddr = validatorKeyPair().address().value();

    const Transaction tx = TransactionBuilder::buildSignedStakeWithdraw(
        TransactionBuildRequest(
            validatorAddr,
            Amount::fromRawUnits(1'000'000),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    const Block candidate = blockWithTransaction(runtime, tx);

    const auto result = StateTransitionPreview::previewBlock(candidate, 0);

    requireCondition(
        result.accepted(),
        "Structural-only previewBlock must accept the STAKE_WITHDRAW (no domain check)."
    );
}

} // namespace

int main() {
    try {
        testStakeWithdrawWithZeroBondedIsRejectedByPreValidator();
        testUnjailRequestForNonJailedValidatorIsRejectedByPreValidator();
        testStakeWithdrawPassesStructuralPreviewWithoutPreValidator();
        std::cout << "Domain transaction pre-validator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Domain transaction pre-validator tests failed: " << error.what() << "\n";
        return 1;
    }
}
