#include "consensus/BlockFinalizationPhase.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockVotingPhase.hpp"
#include "config/NetworkParameters.hpp"
#include "core/StateRootCalculator.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/Signer.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "serialization/BlockCodec.hpp"
#include "../common/ConsensusPhaseTestFixtures.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kTimestamp = 1900000000LL;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

crypto::KeyPair validatorKey(const std::string& seed) {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(seed);
}

crypto::KeyPair userKey() {
    return test::consensusTestUserKey("canonical-slashing-user");
}

config::GenesisConfig genesisConfig() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            config::BootstrapValidatorConfig(
                validatorKey("slash-validator-a").publicKey(),
                1,
                1,
                "slash-validator-a"
            ),
            config::BootstrapValidatorConfig(
                validatorKey("slash-validator-b").publicKey(),
                1,
                1,
                "slash-validator-b"
            )
        },
        {test::fundedConsensusTestAccount(userKey())},
        "canonical-slashing-test"
    );
}

node::NodeRuntime startRuntime() {
    const auto started = node::NodeRuntimeFactory::startFromGenesis(
        node::NodeRuntimeConfig(
            genesisConfig(),
            p2p::PeerInfo(
                "slashing-peer",
                "127.0.0.1:29991",
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

crypto::Signer signer(const std::string& seed) {
    static const crypto::Bls12381SignatureProvider provider;
    return crypto::Signer(validatorKey(seed), provider);
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
                  "slashing-peer",
                  "localnet",
                  "chain-localnet",
                  "1",
                  genesisConfig().deterministicId(),
                  60,
                  4
              ),
              transport
          ) {}
};

consensus::BlockFinalizationPhaseResult finalize(
    node::NodeRuntime& runtime,
    const core::Block& block,
    std::int64_t timestamp
) {
    TestGossipMesh gossip;
    const std::vector<std::string> validatorSeeds = {
        "slash-validator-a", "slash-validator-b"
    };
    for (const std::string& seed : validatorSeeds) {
        const crypto::Signer validatorSigner = signer(seed);
        require(
            consensus::BlockVotingPhase::castPrevote(
                runtime,
                block,
                1,
                timestamp,
                validatorSigner,
                gossip.mesh
            ).cast(),
            "Historical validator prevote must be accepted."
        );
        require(
            consensus::BlockVotingPhase::castPrecommit(
                runtime,
                block,
                1,
                timestamp + 1,
                validatorSigner,
                gossip.mesh
            ).cast(),
            "Historical validator precommit must be accepted."
        );
    }

    const crypto::ProtocolCryptoContext context =
        crypto::ProtocolCryptoContext::fromNetworkName("localnet");
    return consensus::BlockFinalizationPhase::tryFinalize(
        runtime,
        block,
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        context.policy(),
        context.signatureProvider(),
        timestamp + 2
    );
}

consensus::DoubleVoteEvidence doubleVoteEvidence() {
    const crypto::KeyPair key = validatorKey("slash-validator-a");
    const crypto::Bls12381SignatureProvider provider;

    const auto makeVote = [&](const std::string& blockHash) {
        return consensus::ValidatorVoteRecord::createVote(
            key.address().value(),
            key.publicKey(),
            key.privateKeyForSigningOnly(),
            1,
            blockHash,
            "genesis-parent-hash",
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "canonical-slashing-reason",
            kTimestamp + 10,
            provider
        );
    };

    return consensus::DoubleVoteEvidence(
        makeVote("conflicting-block-a"),
        makeVote("conflicting-block-b"),
        kTimestamp + 11
    );
}

void testEvidenceIsFinalizedByTheNextBlock() {
    node::NodeRuntime runtime = startRuntime();
    test::admitConsensusTestTransfer(
        runtime, userKey(), 1, kTimestamp + 1
    );

    const auto firstCandidate = consensus::BlockProductionPhase::produce(
        runtime,
        node::RuntimeBlockPipelineConfig(10, 1, 1, kTimestamp + 2)
    );
    require(firstCandidate.produced(), "First candidate must be produced.");
    require(
        finalize(runtime, firstCandidate.block(), kTimestamp + 3).finalized(),
        "First block must finalize before its evidence is admissible."
    );

    const consensus::DoubleVoteEvidence evidence = doubleVoteEvidence();
    const auto evidenceCandidate = consensus::BlockProductionPhase::produce(
        runtime,
        node::RuntimeBlockPipelineConfig(10, 0, 1, kTimestamp + 20),
        {evidence}
    );
    require(
        evidenceCandidate.produced(),
        "An evidence-only next block must be produced."
    );
    require(
        evidenceCandidate.block().records().size() == 1 &&
        evidenceCandidate.block().records().front().type() ==
            core::LedgerRecordType::SLASHING_EVIDENCE,
        "The next block must contain the explicit slashing evidence record."
    );

    const core::Block restored = serialization::BlockCodec::deserialize(
        evidenceCandidate.block().serialize()
    );
    require(
        restored.hash() == evidenceCandidate.block().hash(),
        "The evidence block must survive canonical serialization."
    );

    require(
        finalize(runtime, evidenceCandidate.block(), kTimestamp + 21)
            .finalized(),
        "The evidence block must finalize."
    );

    const std::string offender =
        validatorKey("slash-validator-a").address().value();
    require(
        runtime.validatorPenaltyLedger().containsEvidence(
            evidence.evidenceId()
        ),
        "The finalized evidence must enter the canonical penalty ledger."
    );
    const core::ValidatorRegistryEntry* offenderEntry =
        runtime.validatorRegistry().entryForAddress(offender);
    require(
        offenderEntry != nullptr && offenderEntry->jailed(),
        "The offending validator must be jailed for the next height."
    );
    require(
        runtime.validatorRegistry().activeCount() == 1,
        "The non-offending validator must keep consensus live."
    );

    const core::StateTransitionPreviewContext stateContext =
        node::RuntimeAccountStateBuilder::previewContextAtTip(
            runtime,
            static_cast<std::int64_t>(
                runtime.effectiveMinimumFeeRawUnits()
            )
        );
    const std::string rebuiltRoot =
        core::StateRootCalculator::calculateProtocolStateRoot(
            stateContext.accountStateView(),
            stateContext.deterministicStateDomains()
        );
    require(
        rebuiltRoot == runtime.blockchain().latestBlock().stateRoot(),
        "Slashing state must be committed by the finalized state root."
    );

    const auto duplicate = consensus::BlockProductionPhase::produce(
        runtime,
        node::RuntimeBlockPipelineConfig(10, 0, 1, kTimestamp + 30),
        {evidence}
    );
    require(
        !duplicate.produced(),
        "Already-finalized evidence must not be accepted again."
    );
}

} // namespace

int main() {
    try {
        testEvidenceIsFinalizedByTheNextBlock();
        std::cout << "Canonical slashing pipeline tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Canonical slashing pipeline tests FAILED: "
                  << error.what() << "\n";
        return 1;
    }
}
