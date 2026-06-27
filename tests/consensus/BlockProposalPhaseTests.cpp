#include "consensus/BlockProposalPhase.hpp"

#include "consensus/BlockProductionPhase.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
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
    return test::consensusTestUserKey("block-proposal-phase-user");
}

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("prop-val").publicKey(),
            1, 1, "prop-val-meta"
          ) },
        { test::fundedConsensusTestAccount(userKey()) },
        "block-proposal-phase-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("prop-node", "127.0.0.1:29992", "nodo/test", 0, kTs);
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

void testProposalSucceedsWithValidBlock() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block to test proposal phase.");

    TestGossipMesh tgm("prop-node", genesis.deterministicId());

    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("prop-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    const consensus::BlockProposalResult result =
        consensus::BlockProposalPhase::propose(
            candidate.block(),
            signer.address(),
            1,       // round
            kTs + 3, // now
            signer,
            tgm.mesh,
            provider
        );

    require(result.proposed(),
            "Proposal phase must return proposed() for a valid block and signer.");
}

void testProposalHasNoSideEffectsOnChain() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const node::RuntimeBlockPipelineConfig prodConfig(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, prodConfig);
    require(candidate.produced(), "Need a produced block.");

    TestGossipMesh tgm("prop-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("prop-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    const std::uint64_t tipBefore = runtime.blockchain().latestBlock().index();

    consensus::BlockProposalPhase::propose(
        candidate.block(), signer.address(), 1, kTs + 3,
        signer, tgm.mesh, provider
    );

    require(runtime.blockchain().latestBlock().index() == tipBefore,
            "Proposal phase must NOT modify the local blockchain.");
    require(runtime.consensusRoundManager().currentState().height() == 1,
            "Proposal phase must NOT advance the consensus round.");
}

} // namespace

int main() {
    try {
        testProposalSucceedsWithValidBlock();
        testProposalHasNoSideEffectsOnChain();

        std::cout << "BlockProposalPhase tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "BlockProposalPhase tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
