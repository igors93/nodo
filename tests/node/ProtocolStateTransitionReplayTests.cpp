#include "config/NetworkParameters.hpp"
#include "core/StateRootCalculator.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtocolStateTransition.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateVerifier.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
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
using nodo::node::ProtocolReplayState;
using nodo::node::ProtocolStateTransition;
using nodo::node::RuntimeAccountStateBuilder;
using nodo::node::RuntimeBlockPipeline;
using nodo::node::RuntimeBlockPipelineConfig;
using nodo::node::RuntimeStateVerifier;
using nodo::p2p::PeerInfo;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;
constexpr std::int64_t kStake = 1'000'000;
constexpr std::int64_t kFee = 100;
constexpr std::int64_t kGenesisStake = 1'000'000;
const std::string kChainId = "nodo-localnet-1";

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

KeyPair validatorKeyPair() {
    return KeyPair::createDeterministicBls12381KeyPair(
        "protocol-replay-validator"
    );
}

KeyPair userKeyPair() {
    return KeyPair::createDeterministicEd25519KeyPair(
        "protocol-replay-user"
    );
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
                1,
                1,
                "protocol-replay-validator"
            )
        },
        {
            GenesisAccountConfig(
                userKeyPair().address().value(),
                Amount::fromRawUnits(1'000'000'000'000LL),
                0
            )
        },
        "protocol-replay-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "protocol-replay-peer",
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
    require(result.started(), "Runtime should start from genesis.");
    return result.runtime();
}

std::int64_t minimumFeeRawUnits(const GenesisConfig& genesis) {
    const std::uint64_t raw =
        genesis.networkParameters().minimumFeeRawUnits();
    require(
        raw <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
        "Genesis minimum fee is too large for tests."
    );
    return static_cast<std::int64_t>(raw);
}

void testReplayAdvancesAccountsAndDomainsTogether() {
    NodeRuntime runtime = startRuntime();
    const GenesisConfig genesis = genesisConfig();
    const std::string validatorAddress = validatorKeyPair().address().value();

    const Transaction tx = TransactionBuilder::buildSignedStakeDeposit(
        TransactionBuildRequest(
            validatorAddress,
            Amount::fromRawUnits(kStake),
            Amount::fromRawUnits(kFee),
            1,
            kTimestamp + 10
        ),
        userSigner(),
        kChainId
    );

    require(
        runtime.mutableMempool().admitTransaction(
            tx,
            CryptoPolicy::developmentPolicy(),
            SecurityContext::USER_TRANSACTION,
            kTimestamp + 11
        ).accepted(),
        "STAKE_DEPOSIT should enter the mempool."
    );

    const auto pipeline = RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock(
        runtime,
        RuntimeBlockPipelineConfig(100, 1, 1, kTimestamp + 20),
        validatorSigner()
    );
    require(pipeline.finalized(), "Block with STAKE_DEPOSIT should finalize.");

    const ProtocolReplayState replay = ProtocolStateTransition::replayToTip(
        genesis,
        runtime.blockchain(),
        minimumFeeRawUnits(genesis)
    );

    require(
        replay.stateRoot == pipeline.postStateRoot(),
        "Unified replay state root must match the finalized block commitment."
    );
    require(
        replay.accounts.serialize() ==
            RuntimeAccountStateBuilder::accountStateViewAtTip(
                genesis,
                runtime.blockchain(),
                minimumFeeRawUnits(genesis)
            ).serialize(),
        "Account helper must be backed by the same canonical replay path."
    );
    require(
        replay.execution.staking.accountOrDefault(validatorAddress)
            .bondedAmount()
            .rawUnits() == kGenesisStake + kStake,
        "Unified replay must advance staking domains with account execution."
    );
    require(
        replay.execution.validators.serialize() == runtime.validatorRegistry().serialize(),
        "Unified replay validator registry must match the finalized runtime."
    );
    require(
        replay.execution.supply == runtime.supplyState().latestSupply(),
        "Unified replay supply must match the finalized runtime supply."
    );
    require(
        RuntimeStateVerifier::calculateLatestStateRoot(
            genesis,
            runtime.blockchain()
        ) == replay.stateRoot,
        "RuntimeStateVerifier must verify the full protocol state root, not an account-only root."
    );

    const std::string accountOnlyRoot =
        nodo::core::StateRootCalculator::calculateAccountStateRoot(replay.accounts);
    require(
        !accountOnlyRoot.empty() && accountOnlyRoot != replay.stateRoot,
        "The canonical protocol state root must include non-account domains."
    );
}

void testInitialReplayCommitsGenesisDomains() {
    const GenesisConfig genesis = genesisConfig();
    const ProtocolReplayState replay =
        ProtocolStateTransition::initialReplayState(genesis);

    require(!replay.stateRoot.empty(), "Genesis replay root must be non-empty.");
    require(
        replay.execution.validators.size() == 1,
        "Genesis replay must seed the bootstrap validator domain."
    );
    require(
        replay.execution.staking.accountOrDefault(
            validatorKeyPair().address().value()
        ).bondedAmount().rawUnits() == kGenesisStake,
        "Genesis replay must seed bootstrap stake domain."
    );

    const std::string accountOnlyRoot =
        nodo::core::StateRootCalculator::calculateAccountStateRoot(replay.accounts);
    require(
        !accountOnlyRoot.empty() && accountOnlyRoot != replay.stateRoot,
        "Genesis protocol root must commit to domains in addition to accounts."
    );
}

} // namespace

int main() {
    try {
        testInitialReplayCommitsGenesisDomains();
        testReplayAdvancesAccountsAndDomainsTogether();
        std::cout << "Protocol state transition replay tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Protocol state transition replay tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
