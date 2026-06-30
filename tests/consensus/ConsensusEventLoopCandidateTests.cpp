#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/ConsensusRecoveryStore.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "../common/ConsensusPhaseTestFixtures.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900500000;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const crypto::CryptoPolicy& developmentPolicy() {
    static const crypto::CryptoPolicy policy =
        crypto::CryptoPolicy::developmentPolicy();
    return policy;
}

crypto::KeyPair userKey() {
    return test::consensusTestUserKey("consensus-event-loop-user");
}

struct ValidatorFixture {
    crypto::KeyPair keyPair;
};

std::vector<ValidatorFixture> makeValidators(std::size_t count) {
    std::vector<ValidatorFixture> validators;
    validators.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        validators.push_back({
            crypto::KeyPair::createDeterministicBls12381KeyPair(
                "consensus-event-loop-validator-" + std::to_string(index)
            )
        });
    }
    return validators;
}

config::GenesisConfig makeGenesis(
    const std::vector<ValidatorFixture>& validators,
    const std::string& seed
) {
    std::vector<config::BootstrapValidatorConfig> bootstrap;
    bootstrap.reserve(validators.size());
    for (std::size_t index = 0; index < validators.size(); ++index) {
        bootstrap.emplace_back(
            validators[index].keyPair.publicKey(),
            1,
            1,
            "candidate-validator-" + std::to_string(index)
        );
    }

    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        bootstrap,
        { test::fundedConsensusTestAccount(userKey()) },
        seed
    );
}

node::NodeRuntime startRuntime(const config::GenesisConfig& genesis) {
    const p2p::PeerInfo peer(
        "candidate-test-node",
        "127.0.0.1:29995",
        "nodo/test",
        0,
        kTimestamp
    );
    const auto result = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(genesis, peer, 16)
    );
    if (!result.started()) {
        throw std::runtime_error("Runtime start failed: " + result.reason());
    }
    return result.runtime();
}

class TestNetwork {
private:
    std::shared_ptr<p2p::LoopbackTransportBus> m_bus;
    p2p::LoopbackTransport m_transport;

public:
    explicit TestNetwork(const config::GenesisConfig& genesis)
        : m_bus(std::make_shared<p2p::LoopbackTransportBus>())
        , m_transport(m_bus)
        , mesh(
              p2p::GossipMeshConfig(
                  "candidate-test-node",
                  genesis.networkParameters().networkName(),
                  genesis.networkParameters().chainId(),
                  "nodo/test",
                  genesis.deterministicId(),
                  60,
                  3
              ),
              m_transport
          )
    {}

    p2p::GossipMesh mesh;
};

const ValidatorFixture& proposerFixture(
    const std::vector<ValidatorFixture>& validators,
    const node::NodeRuntime& runtime
) {
    const auto& state = runtime.consensusRoundManager().currentState();
    const std::string expected = consensus::ProposerSchedule::selectProposer(
        runtime.validatorRegistry(),
        runtime.config().genesisConfig().networkParameters().chainId(),
        state.height(),
        state.round()
    );

    for (const auto& validator : validators) {
        if (validator.keyPair.address().value() == expected) return validator;
    }
    throw std::runtime_error("Scheduled proposer key was not found.");
}

void configureProducer(
    consensus::ConsensusEventLoop& loop,
    node::NodeRuntime& runtime,
    const crypto::Signer& signer
) {
    loop.setLocalValidatorAddress(signer.address());
    loop.setLocalSigner(&signer);
    loop.setBlockProducerCallback(
        [&runtime](std::uint64_t, std::uint64_t round, std::int64_t now)
            -> std::optional<core::Block> {
            const consensus::BlockCandidateResult candidate =
                consensus::BlockProductionPhase::produce(
                    runtime,
                    node::RuntimeBlockPipelineConfig(16, 1, round, now)
                );
            if (!candidate.produced()) return std::nullopt;
            return candidate.block();
        }
    );
}

void testCandidateDoesNotEnterChainWithoutQuorum() {
    const auto validators = makeValidators(2);
    const config::GenesisConfig genesis =
        makeGenesis(validators, "candidate-no-quorum");
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
    TestNetwork network(genesis);
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(proposerFixture(validators, runtime).keyPair, provider);

    consensus::ConsensusEventLoop loop(
        runtime,
        network.mesh,
        developmentPolicy(),
        provider
    );
    configureProducer(loop, runtime, signer);

    const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

    require(!result.blockFinalized,
            "One of two validators must not finalize without peer votes.");
    require(runtime.blockchain().size() == 1,
            "An unfinalized candidate must remain outside the canonical chain.");
    require(!runtime.finalizationRegistry().hasFinalizedHeight(1),
            "The candidate height must remain unfinalized without quorum.");
}

void testSingleValidatorCandidateAppendsOnlyDuringFinalization() {
    const auto validators = makeValidators(1);
    const config::GenesisConfig genesis =
        makeGenesis(validators, "candidate-finalization-append");
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
    TestNetwork network(genesis);
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(validators.front().keyPair, provider);

    consensus::ConsensusEventLoop loop(
        runtime,
        network.mesh,
        developmentPolicy(),
        provider
    );
    configureProducer(loop, runtime, signer);

    require(runtime.blockchain().size() == 1,
            "The test must begin with genesis only.");

    const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

    require(result.blockFinalized,
            "A single-validator development chain must finalize its own quorum.");
    require(runtime.blockchain().size() == 2,
            "The finalized candidate must be appended exactly once.");
    require(runtime.finalizationRegistry().hasFinalizedHeight(1),
            "Finalization must be recorded in the runtime registry.");
}

void testMissingProposalStillAdvancesRound() {
    const auto validators = makeValidators(2);
    const config::GenesisConfig genesis =
        makeGenesis(validators, "candidate-proposer-timeout");
    node::NodeRuntime runtime = startRuntime(genesis);
    TestNetwork network(genesis);
    const crypto::Bls12381SignatureProvider provider;

    consensus::ConsensusEventLoop loop(
        runtime,
        network.mesh,
        developmentPolicy(),
        provider
    );

    const auto timeout =
        runtime.consensusRoundManager().roundTimeout().expiresAt();
    const consensus::ConsensusTickResult result = loop.tick(timeout);

    require(result.roundAdvanced,
            "Consensus must advance when the scheduled proposer sends no block.");
    require(runtime.consensusRoundManager().currentState().round() == 2,
            "The proposer timeout must move consensus to round 2.");
    require(runtime.blockchain().size() == 1,
            "A timeout without a proposal must not modify the blockchain.");
}


void testConsensusLoopPersistsSignedPrevoteBeforeBroadcast() {
    const auto validators = makeValidators(2);
    const config::GenesisConfig genesis =
        makeGenesis(validators, "candidate-signed-prevote-recovery");
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
    TestNetwork network(genesis);
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(proposerFixture(validators, runtime).keyPair, provider);

    const std::filesystem::path recoveryPath =
        std::filesystem::temp_directory_path() /
        "nodo-consensus-event-loop-signed-prevote.state";
    std::error_code cleanupError;
    std::filesystem::remove(recoveryPath, cleanupError);

    consensus::ConsensusEventLoop loop(
        runtime,
        network.mesh,
        developmentPolicy(),
        provider
    );
    loop.setRecoveryPath(recoveryPath);
    configureProducer(loop, runtime, signer);

    const consensus::ConsensusTickResult result = loop.tick(kTimestamp + 3);

    require(!result.blockFinalized,
            "Two validators should not finalize with only the local prevote.");

    const auto stored = consensus::ConsensusRecoveryStore::load(recoveryPath);
    require(stored.has_value(),
            "Consensus loop must persist recovery state after local PREVOTE.");
    require(stored->votedPrevote(),
            "Recovery state must record that the local validator prevoted.");
    require(stored->persistedPrevote().has_value(),
            "Recovery state must contain the exact signed PREVOTE for rebroadcast.");
    require(stored->persistedPrevote()->decision() == consensus::ValidatorVoteDecision::PREVOTE,
            "Persisted vote must be a PREVOTE.");
    require(stored->persistedPrevote()->verify(developmentPolicy(), provider),
            "Persisted PREVOTE must be a valid signed vote, not a boolean marker.");

    std::filesystem::remove(recoveryPath, cleanupError);
}

void testProposerRetriesAfterTransientProductionFailure() {
    const auto validators = makeValidators(1);
    const config::GenesisConfig genesis =
        makeGenesis(validators, "candidate-production-retry");
    node::NodeRuntime runtime = startRuntime(genesis);
    test::admitConsensusTestTransfer(runtime, userKey(), 1, kTimestamp + 1);
    TestNetwork network(genesis);
    const crypto::Bls12381SignatureProvider provider;
    const crypto::Signer signer(validators.front().keyPair, provider);

    consensus::ConsensusEventLoop loop(
        runtime,
        network.mesh,
        developmentPolicy(),
        provider
    );
    loop.setLocalValidatorAddress(signer.address());
    loop.setLocalSigner(&signer);

    std::size_t productionAttempts = 0;
    loop.setBlockProducerCallback(
        [&runtime, &productionAttempts](
            std::uint64_t,
            std::uint64_t round,
            std::int64_t now
        ) -> std::optional<core::Block> {
            ++productionAttempts;
            if (productionAttempts == 1) {
                return std::nullopt;
            }
            const consensus::BlockCandidateResult candidate =
                consensus::BlockProductionPhase::produce(
                    runtime,
                    node::RuntimeBlockPipelineConfig(16, 1, round, now)
                );
            return candidate.produced()
                ? std::optional<core::Block>(candidate.block())
                : std::nullopt;
        }
    );

    const consensus::ConsensusTickResult first = loop.tick(kTimestamp + 3);
    require(!first.blockFinalized,
            "A failed production attempt must not finalize a block.");
    require(productionAttempts == 1,
            "The first tick must invoke production exactly once.");

    const consensus::ConsensusTickResult second = loop.tick(kTimestamp + 4);
    require(productionAttempts == 2,
            "The proposer must retry production in the same round.");
    require(second.blockFinalized,
            "A successful retry must proceed through finalization.");
    require(runtime.blockchain().size() == 2,
            "The retried candidate must be appended exactly once.");
}

} // namespace

int main() {
    try {
        testCandidateDoesNotEnterChainWithoutQuorum();
        testSingleValidatorCandidateAppendsOnlyDuringFinalization();
        testMissingProposalStillAdvancesRound();
        testConsensusLoopPersistsSignedPrevoteBeforeBroadcast();
        testProposerRetriesAfterTransientProductionFailure();
        std::cout << "Consensus event loop candidate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Consensus event loop candidate tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
