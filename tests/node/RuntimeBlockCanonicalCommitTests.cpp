#include "consensus/BlockFinalizationPhase.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockVotingPhase.hpp"
#include "config/NetworkParameters.hpp"
#include "core/State.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "../common/ConsensusPhaseTestFixtures.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

crypto::KeyPair validatorKey() {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(
        "canonical-commit-validator"
    );
}

crypto::KeyPair userKey() {
    return test::consensusTestUserKey("canonical-commit-user");
}

config::GenesisConfig genesisConfig() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {config::BootstrapValidatorConfig(
            validatorKey().publicKey(), 1, 1, "canonical-validator"
        )},
        {test::fundedConsensusTestAccount(userKey())},
        "canonical-commit-test"
    );
}

node::NodeRuntime startRuntime() {
    const node::NodeRuntimeStartResult started =
        node::NodeRuntimeFactory::startFromGenesis(
            node::NodeRuntimeConfig(
                genesisConfig(),
                p2p::PeerInfo(
                    "canonical-peer",
                    "127.0.0.1:29998",
                    "nodo/test",
                    0,
                    kTimestamp
                ),
                16
            )
        );
    require(started.started(), "Runtime must start from genesis.");
    return started.runtime();
}

crypto::Signer validatorSigner() {
    static const crypto::Bls12381SignatureProvider provider;
    return crypto::Signer(validatorKey(), provider);
}

utils::Amount balanceAt(
    node::NodeRuntime& runtime,
    const std::string& address
) {
    return runtime.cachedAccountStateAtTip(
        static_cast<std::int64_t>(runtime.effectiveMinimumFeeRawUnits())
    ).accountOrDefault(address).balance();
}

utils::Amount totalAccountBalance(node::NodeRuntime& runtime) {
    utils::Amount total;
    for (const core::AccountState& account :
         runtime.cachedAccountStateAtTip(
             static_cast<std::int64_t>(
                 runtime.effectiveMinimumFeeRawUnits()
             )
         ).accounts()) {
        total = total + account.balance();
    }
    return total;
}

struct TestGossipMesh {
    std::shared_ptr<p2p::LoopbackTransportBus> bus;
    p2p::LoopbackTransport transport;
    p2p::GossipMesh mesh;

    TestGossipMesh()
        : bus(std::make_shared<p2p::LoopbackTransportBus>())
        , transport(bus)
        , mesh(
              p2p::GossipMeshConfig(
                  "canonical-peer",
                  "localnet",
                  "chain-localnet",
                  "1",
                  genesisConfig().deterministicId(),
                  60,
                  2
              ),
              transport
          )
    {}
};

consensus::BlockFinalizationPhaseResult finalizeThroughConsensus(
    node::NodeRuntime& runtime,
    const core::Block& block,
    const node::NodeDataDirectoryConfig* directoryConfig = nullptr
) {
    TestGossipMesh gossip;
    const crypto::Signer signer = validatorSigner();
    require(
        consensus::BlockVotingPhase::castPrevote(
            runtime, block, 1, kTimestamp + 3, signer, gossip.mesh
        ).cast(),
        "Prevote must be accepted."
    );
    require(
        consensus::BlockVotingPhase::castPrecommit(
            runtime, block, 1, kTimestamp + 4, signer, gossip.mesh
        ).cast(),
        "Precommit must be accepted."
    );

    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            genesisConfig().networkParameters().networkName()
        );
    require(cryptoContext.isValid(), "Crypto context must be valid.");

    return consensus::BlockFinalizationPhase::tryFinalize(
        runtime,
        block,
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        cryptoContext.policy(),
        cryptoContext.signatureProvider(),
        kTimestamp + 5,
        directoryConfig
    );
}

void testLocalAndDistributedPipelinesProduceEquivalentState() {
    node::NodeRuntime localRuntime = startRuntime();
    node::NodeRuntime distributedRuntime = startRuntime();
    test::admitConsensusTestTransfer(
        localRuntime, userKey(), 1, kTimestamp + 1
    );
    test::admitConsensusTestTransfer(
        distributedRuntime, userKey(), 1, kTimestamp + 1
    );

    const node::RuntimeBlockPipelineConfig config(10, 1, 1, kTimestamp + 2);
    const node::RuntimeBlockPipelineResult localResult =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            localRuntime, config, validatorSigner()
        );
    require(localResult.finalized(), "Local pipeline must finalize.");

    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(distributedRuntime, config);
    require(candidate.produced(), "Distributed pipeline must produce a candidate.");
    const consensus::BlockFinalizationPhaseResult distributedResult =
        finalizeThroughConsensus(distributedRuntime, candidate.block());
    require(distributedResult.finalized(), "Distributed pipeline must finalize.");

    require(
        localRuntime.blockchain().latestBlock().hash()
            == distributedRuntime.blockchain().latestBlock().hash(),
        "Both pipelines must commit the same canonical block."
    );
    require(
        localRuntime.supplyState().latestSupply()
            == distributedRuntime.supplyState().latestSupply(),
        "Both pipelines must apply the same supply delta."
    );
    require(
        localRuntime.mempool().size() == 0
            && distributedRuntime.mempool().size() == 0,
        "Both pipelines must remove finalized transactions from the mempool."
    );
    require(
        localRuntime.governanceExecutor().serialize()
            == distributedRuntime.governanceExecutor().serialize(),
        "Both pipelines must expose the same governance state."
    );
    require(
        localRuntime.cachedAccountStateAtTip(1).serialize()
            == distributedRuntime.cachedAccountStateAtTip(1).serialize(),
        "Both pipelines must commit identical account state."
    );
    require(
        balanceAt(localRuntime, core::State::feePoolAddress()).rawUnits() == 50,
        "The canonical transition must credit the validator fee share."
    );
    require(
        balanceAt(localRuntime, node::ProtectionTreasury::TREASURY_ADDRESS)
                .rawUnits() == 30,
        "The canonical transition must credit the treasury fee share."
    );
    require(
        totalAccountBalance(localRuntime)
            == localRuntime.supplyState().latestSupply(),
        "Account balances and canonical supply must remain conserved."
    );
    require(
        localRuntime.blockchain().latestBlock().stateRoot()
            == localResult.postStateRoot(),
        "The finalized block must commit the complete post-state root."
    );
    require(
        localRuntime.consensusRoundManager().currentState().height()
            == distributedRuntime.consensusRoundManager().currentState().height(),
        "Both pipelines must advance to the same consensus height."
    );
    require(
        distributedRuntime.statePruner().hasStateRoot(candidate.block().index()),
        "Distributed finalization must record the canonical state root."
    );
}

void testGovernanceProposalIsCommittedWithoutPrematureExecution() {
    node::NodeRuntime runtime = startRuntime();
    const crypto::Ed25519SignatureProvider provider;
    const core::Transaction proposal =
        core::TransactionBuilder::buildSignedGovernanceProposal(
            core::GovernanceProposalPayload::parameterChange(
                "Minimum fee",
                "Set minimum fee after governance approval",
                "MINIMUM_FEE_RAW",
                "250",
                1
            ).serialize(),
            utils::Amount::fromRawUnits(100),
            1,
            kTimestamp + 1,
            crypto::Signer(userKey(), provider),
            genesisConfig().networkParameters().chainId()
        );
    const mempool::MempoolAdmissionResult admission =
        runtime.mutableMempool().admitTransaction(
            proposal,
            crypto::CryptoPolicy::developmentPolicy(),
            crypto::SecurityContext::USER_TRANSACTION,
            kTimestamp + 1
        );
    require(admission.accepted(), "Governance proposal must enter the mempool.");

    const node::RuntimeBlockPipelineResult result =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            node::RuntimeBlockPipelineConfig(10, 1, 1, kTimestamp + 2),
            validatorSigner()
        );

    require(result.finalized(), "Governance block must finalize: " + result.reason());
    require(
        runtime.governanceExecutor().hasProposal(proposal.id()) &&
        runtime.effectiveMinimumFeeRawUnits() ==
            genesisConfig().networkParameters().minimumFeeRawUnits(),
        "Finalization must record the proposal without applying it before a vote."
    );
    require(
        runtime.blockchain().latestBlock().stateRoot()
            == result.postStateRoot(),
        "Governance state must be included in the signed post-state root."
    );
    require(
        totalAccountBalance(runtime) == runtime.supplyState().latestSupply(),
        "Governance transaction fees must preserve account/supply conservation."
    );

    test::admitConsensusTestTransfer(
        runtime, userKey(), 2, kTimestamp + 3
    );
    const node::RuntimeBlockPipelineResult nextBlock =
        node::RuntimeBlockPipeline::produceAndFinalizeNextBlock(
            runtime,
            node::RuntimeBlockPipelineConfig(10, 1, 1, kTimestamp + 4),
            validatorSigner()
        );
    require(
        nextBlock.finalized(),
        "Unapproved governance proposals must not change the effective minimum fee: " +
            nextBlock.reason()
    );
}

void testPersistenceFailureDoesNotPartiallyCommitRuntime() {
    node::NodeRuntime runtime = startRuntime();
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);

    const node::RuntimeBlockPipelineConfig config(10, 1, 1, kTimestamp + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, config);
    require(candidate.produced(), "Candidate must be produced.");

    const std::size_t chainSizeBefore = runtime.blockchain().size();
    const std::size_t mempoolSizeBefore = runtime.mempool().size();
    const utils::Amount supplyBefore = runtime.supplyState().latestSupply();
    const std::uint64_t consensusHeightBefore =
        runtime.consensusRoundManager().currentState().height();
    const node::NodeDataDirectoryConfig invalidDirectory;

    const consensus::BlockFinalizationPhaseResult result =
        finalizeThroughConsensus(runtime, candidate.block(), &invalidDirectory);

    require(!result.finalized(), "Invalid persistence must reject finalization.");
    require(
        runtime.blockchain().size() == chainSizeBefore,
        "Persistence failure must not append the block."
    );
    require(
        runtime.mempool().size() == mempoolSizeBefore,
        "Persistence failure must not remove mempool transactions."
    );
    require(
        runtime.supplyState().latestSupply() == supplyBefore,
        "Persistence failure must not change supply."
    );
    require(
        runtime.consensusRoundManager().currentState().height()
            == consensusHeightBefore,
        "Persistence failure must not advance consensus height."
    );
    require(
        !runtime.finalizationRegistry().hasFinalizedHeight(
            candidate.block().index()
        ),
        "Persistence failure must not register finality."
    );
}

} // namespace

int main() {
    try {
        testLocalAndDistributedPipelinesProduceEquivalentState();
        testGovernanceProposalIsCommittedWithoutPrematureExecution();
        testPersistenceFailureDoesNotPartiallyCommitRuntime();
        std::cout << "Runtime canonical block commit tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Runtime canonical block commit tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
