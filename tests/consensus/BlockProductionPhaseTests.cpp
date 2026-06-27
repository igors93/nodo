#include "consensus/BlockProductionPhase.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/PeerMessage.hpp"
#include "../common/ConsensusPhaseTestFixtures.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace nodo;

constexpr std::int64_t kTs = 1900000000LL;

void require(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

crypto::KeyPair userKey() {
    return test::consensusTestUserKey("block-production-phase-user");
}

config::GenesisConfig minimalGenesis() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTs,
        { config::BootstrapValidatorConfig(
            crypto::KeyPair::createDeterministicBls12381KeyPair("prod-val").publicKey(),
            1, 1, "prod-val-meta"
          ) },
        { test::fundedConsensusTestAccount(userKey()) },
        "block-production-phase-test"
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer("prod-node", "127.0.0.1:29991", "nodo/test", 0, kTs);
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 10)
    );
    if (!result.started()) {
        throw std::runtime_error("startFromGenesis failed: " + result.reason());
    }
    return result.runtime();
}

void testProduceSucceedsWithValidRuntime() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const node::RuntimeBlockPipelineConfig config(
        10,    // maxTransactions
        0,     // minTransactions
        1,     // round
        kTs + 2
    );

    const consensus::BlockCandidateResult result =
        consensus::BlockProductionPhase::produce(runtime, config);

    require(result.produced(), "Production phase must succeed with valid runtime. Reason: " + result.reason());
    require(result.block().index() == 1,
            "Produced block must be at consensus height 1.");
    require(result.block().isValid(false),
            "Produced block must be structurally valid.");
}

void testProduceFailsWithInvalidConfig() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);

    // An invalid config (round 0 is never valid).
    const node::RuntimeBlockPipelineConfig config(10, 0, 0, kTs + 1);

    const consensus::BlockCandidateResult result =
        consensus::BlockProductionPhase::produce(runtime, config);

    require(!result.produced(),
            "Production phase must fail when pipeline config is invalid.");
    require(!result.reason().empty(),
            "Failure must carry a rejection reason.");
}

void testProduceDoesNotFinalizeOrAdvanceRound() {
    const config::GenesisConfig genesis = minimalGenesis();
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTs + 1);

    const std::uint64_t heightBefore =
        runtime.consensusRoundManager().currentState().height();

    const node::RuntimeBlockPipelineConfig config(10, 1, 1, kTs + 2);
    const consensus::BlockCandidateResult candidate =
        consensus::BlockProductionPhase::produce(runtime, config);
    require(candidate.produced(),
            "Production must succeed before side effects are evaluated.");

    const std::uint64_t heightAfter =
        runtime.consensusRoundManager().currentState().height();

    require(heightBefore == heightAfter,
            "Production phase must NOT advance the consensus round.");
    require(runtime.blockchain().latestBlock().index() == 0,
            "Production phase must NOT add the block to the blockchain.");
}

} // namespace

int main() {
    try {
        testProduceSucceedsWithValidRuntime();
        testProduceFailsWithInvalidConfig();
        testProduceDoesNotFinalizeOrAdvanceRound();

        std::cout << "BlockProductionPhase tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "BlockProductionPhase tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
