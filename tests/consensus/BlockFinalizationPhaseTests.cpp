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

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val").publicKey(),
            1, 1, "fin-val-meta"
          ) },
        {},
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
                                    genesisId, 60, 2),
              transport
          )
    {}
};

void testFinalizeSucceedsAfterPrecommitQuorum() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    // Phase 1: produce a candidate block.
    const node::RuntimeBlockPipelineConfig prodConfig(10, 0, 1, kTs + 1);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();

    // Add block to chain (CEL would do this; here we do it manually).
    runtime.mutableBlockchain().addBlock(block);

    TestGossipMesh tgm("fin-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    // Phase 3: cast prevote + precommit (only validator = immediate quorum).
    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 1, signer, tgm.mesh
    );
    consensus::BlockVotingPhase::castPrecommit(
        runtime, block, 1, kTs + 2, signer, tgm.mesh
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
            kTs + 3
        );

    require(finResult.finalized(),
            "Finalization must succeed when quorum of PRECOMMIT votes is present.");
    require(finResult.record().blockIndex() == block.index(),
            "Finalized record must reference the correct block height.");
    require(finResult.record().blockHash() == block.hash(),
            "Finalized record must reference the correct block hash.");
}

void testFinalizeReturnNotEnoughVotesWithoutQuorum() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 0, 1, kTs + 1);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();

    runtime.mutableBlockchain().addBlock(block);

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
            kTs + 1
        );

    require(!result.finalized(),
            "Finalization must not succeed without quorum.");
    require(result.insufficient(),
            "Without quorum, result must indicate insufficient votes.");
}

void testFinalizationRegistersInRuntimeRegistry() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 0, 1, kTs + 1);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");
    const core::Block block = candidate.block();
    runtime.mutableBlockchain().addBlock(block);

    TestGossipMesh tgm("fin-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("fin-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 1, signer, tgm.mesh
    );
    consensus::BlockVotingPhase::castPrecommit(
        runtime, block, 1, kTs + 2, signer, tgm.mesh
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
        1, cryptoCtx.policy(), cryptoCtx.signatureProvider(), kTs + 3
    );

    const bool after =
        runtime.finalizationRegistry().hasFinalizedHeight(block.index());
    require(after,
            "Runtime finalizationRegistry must reflect the finalized height.");
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
