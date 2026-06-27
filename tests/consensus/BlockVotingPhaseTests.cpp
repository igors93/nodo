#include "consensus/BlockVotingPhase.hpp"

#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
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
    return test::consensusTestUserKey("block-voting-phase-user");
}

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("vote-val").publicKey(),
            1, 1, "vote-val-meta"
          ) },
        { test::fundedConsensusTestAccount(userKey()) },
        "block-voting-phase-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("vote-node", "127.0.0.1:29993", "nodo/test", 0, kTs);
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

// Produce a candidate at height 1 without mutating the canonical chain.
core::Block produceCandidate(node::NodeRuntime& runtime) {
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);
    const node::RuntimeBlockPipelineConfig config(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, config);
    if (!candidate.produced()) {
        throw std::runtime_error(
            "produceCandidate: production failed: " + candidate.reason()
        );
    }
    return candidate.block();
}

void testPrevoteIsAcceptedAndRecordedInVotePool() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    const core::Block block = produceCandidate(runtime);

    TestGossipMesh tgm("vote-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("vote-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    const consensus::VoteCastResult result =
        consensus::BlockVotingPhase::castPrevote(
            runtime, block, 1, kTs + 3, signer, tgm.mesh
        );

    require(result.cast(), "castPrevote must succeed for an active validator.");

    // Verify the PREVOTE is now in the VotePool.
    const consensus::VotePool& pool =
        runtime.consensusRoundManager().voteCollector().votePool();
    const auto votes = pool.votesForBlock(block.index(), block.hash(), 1);

    bool foundPrevote = false;
    for (const auto& v : votes) {
        if (v.decision() == consensus::ValidatorVoteDecision::PREVOTE) {
            foundPrevote = true;
        }
    }
    require(foundPrevote, "VotePool must contain a PREVOTE after castPrevote.");
}

void testPrecommitIsAcceptedAndRecordedInVotePool() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    const core::Block block = produceCandidate(runtime);

    TestGossipMesh tgm("vote-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("vote-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    // First cast prevote.
    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 3, signer, tgm.mesh
    );

    // Then cast precommit.
    const consensus::VoteCastResult result =
        consensus::BlockVotingPhase::castPrecommit(
            runtime, block, 1, kTs + 4, signer, tgm.mesh
        );

    require(result.cast(), "castPrecommit must succeed for an active validator.");

    const consensus::VotePool& pool =
        runtime.consensusRoundManager().voteCollector().votePool();
    const auto votes = pool.votesForBlock(block.index(), block.hash(), 1);

    bool foundPrecommit = false;
    for (const auto& v : votes) {
        if (v.decision() == consensus::ValidatorVoteDecision::PRECOMMIT) {
            foundPrecommit = true;
        }
    }
    require(foundPrecommit, "VotePool must contain a PRECOMMIT after castPrecommit.");
}

void testVotingPhaseHasNoSideEffectsOnBlockchain() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    const core::Block block = produceCandidate(runtime);

    TestGossipMesh tgm("vote-node", genesis.deterministicId());
    const crypto::KeyPair kp =
        crypto::KeyPair::createDeterministicBls12381KeyPair("vote-val");
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(kp, provider);

    const std::uint64_t tipBefore = runtime.blockchain().latestBlock().index();

    consensus::BlockVotingPhase::castPrevote(
        runtime, block, 1, kTs + 3, signer, tgm.mesh
    );

    require(runtime.blockchain().latestBlock().index() == tipBefore,
            "Voting phase must NOT modify the blockchain tip.");
    require(runtime.consensusRoundManager().currentState().height() == 1,
            "Voting phase must NOT advance the consensus round.");
}

} // namespace

int main() {
    try {
        testPrevoteIsAcceptedAndRecordedInVotePool();
        testPrecommitIsAcceptedAndRecordedInVotePool();
        testVotingPhaseHasNoSideEffectsOnBlockchain();

        std::cout << "BlockVotingPhase tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "BlockVotingPhase tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
