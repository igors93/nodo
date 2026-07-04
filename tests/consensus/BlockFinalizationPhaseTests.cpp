#include "consensus/BlockFinalizationPhase.hpp"

#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockVotingPhase.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/PeerMessage.hpp"
#include "../common/ConsensusPhaseTestFixtures.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

crypto::KeyPair userKey() {
    return test::consensusTestUserKey("block-finalization-phase-user");
}

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val").publicKey(),
            1, 1, "fin-val-meta"
          ) },
        { test::fundedConsensusTestAccount(userKey()) },
        "block-finalization-phase-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("fin-node", "127.0.0.1:29994", "nodo/test", 0, kTs);
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 10)
    );
    if (!result.started()) {
        throw std::runtime_error("startFromGenesis failed: " + result.reason());
    }
    return result.runtime();
}

struct TestGossipMesh {
    std::shared_ptr<p2p::LoopbackTransportBus> bus;
    p2p::LoopbackTransport transport;
    p2p::GossipMesh mesh;

    TestGossipMesh(const std::string& nodeId, const std::string& genesisId)
        : bus(std::make_shared<p2p::LoopbackTransportBus>())
        , transport(bus)
        , mesh(
              p2p::GossipMeshConfig(nodeId, "localnet", "chain-localnet", "1",
                                    genesisId, 60, 2, 100, 50),
              transport
          )
    {}
};

void testFinalizeSucceedsAfterPrecommitQuorum() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    // Phase 1: produce a candidate block.
    const node::RuntimeBlockPipelineConfig prodConfig(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();

    require(runtime.blockchain().size() == 1,
            "A candidate must remain outside the canonical chain before quorum.");

    TestGossipMesh tgm("fin-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    // Phase 3: cast prevote + precommit (only validator = immediate quorum).
    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 3, signer, tgm.mesh
    );
    consensus::BlockVotingPhase::castPrecommit(
        runtime, block, 1, kTs + 4, signer, tgm.mesh
    );

    // Phase 4: finalize.
    const crypto::ProtocolCryptoContext cryptoCtx =
        crypto::ProtocolCryptoContext::fromNetworkName(
            genesis.networkParameters().networkName()
        );
    require(cryptoCtx.isValid(), "Need a valid crypto context.");

    const consensus::BlockFinalizationPhaseResult finResult =
        consensus::BlockFinalizationPhase::tryFinalize(
            runtime,
            block,
            block.index(),
            block.hash(),
            block.previousHash(),
            1,              // round
            cryptoCtx.policy(),
            cryptoCtx.signatureProvider(),
            kTs + 5
        );

    if (!finResult.finalized()) { std::cout << "tryFinalize failed: " << finResult.reason() << std::endl; } require(finResult.finalized(),
            "Finalization must succeed when quorum of PRECOMMIT votes is present.");
    require(finResult.record().blockIndex() == block.index(),
            "Finalized record must reference the correct block height.");
    require(finResult.record().blockHash() == block.hash(),
            "Finalized record must reference the correct block hash.");
    require(runtime.blockchain().size() == 2,
            "Finalization must append the candidate exactly once after quorum.");
    require(runtime.blockchain().latestBlock().hash() == block.hash(),
            "The quorum-authorized candidate must become the canonical tip.");
}

void testFinalizeReturnNotEnoughVotesWithoutQuorum() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();

    const crypto::ProtocolCryptoContext cryptoCtx =
        crypto::ProtocolCryptoContext::fromNetworkName(
            genesis.networkParameters().networkName()
        );

    // No votes cast — quorum cannot be reached.
    const consensus::BlockFinalizationPhaseResult result =
        consensus::BlockFinalizationPhase::tryFinalize(
            runtime,
            block,
            block.index(),
            block.hash(),
            block.previousHash(),
            1,
            cryptoCtx.policy(),
            cryptoCtx.signatureProvider(),
            kTs + 3
        );

    require(!result.finalized(),
            "Finalization must not succeed without quorum.");
    require(result.insufficient(),
            "Without quorum, result must indicate insufficient votes.");
    require(runtime.blockchain().size() == 1,
            "A candidate without quorum must not enter the canonical chain.");
}

void testFinalizationRegistersInRuntimeRegistry() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();

    TestGossipMesh tgm("fin-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 3, signer, tgm.mesh
    );
    consensus::BlockVotingPhase::castPrecommit(
        runtime, block, 1, kTs + 4, signer, tgm.mesh
    );

    const crypto::ProtocolCryptoContext cryptoCtx =
        crypto::ProtocolCryptoContext::fromNetworkName(
            genesis.networkParameters().networkName()
        );

    const bool before =
        runtime.finalizationRegistry().hasFinalizedHeight(block.index());
    require(!before, "Registry must not have the height before finalization.");

    consensus::BlockFinalizationPhase::tryFinalize(
        runtime, block, block.index(), block.hash(), block.previousHash(),
        1, cryptoCtx.policy(), cryptoCtx.signatureProvider(), kTs + 5
    );

    const bool after =
        runtime.finalizationRegistry().hasFinalizedHeight(block.index());
    require(after,
            "Runtime finalizationRegistry must reflect the finalized height.");
    require(runtime.blockchain().latestBlock().hash() == block.hash(),
            "Registered finalization must append the matching block.");
}

} // namespace

int main() {
    try {
        testFinalizeSucceedsAfterPrecommitQuorum();
        testFinalizeReturnNotEnoughVotesWithoutQuorum();
        testFinalizationRegistersInRuntimeRegistry();

        std::cout << "BlockFinalizationPhase tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "BlockFinalizationPhase tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
