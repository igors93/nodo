#include "node/FinalizedArtifactGossipAdmission.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/NodeRuntime.hpp"

#include "config/NetworkParameters.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/Block.hpp"
#include "core/LedgerRecord.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/KeyPair.hpp"
#include "economics/ValidationWorkRecord.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace nodo;

constexpr std::int64_t kGenesisTimestamp = 1900800000;
constexpr std::int64_t kBlockTimestamp = kGenesisTimestamp + 20;
constexpr std::int64_t kFinalizedAt = kGenesisTimestamp + 30;
constexpr const char* kProtocolVersion = "nodo/test";

const std::filesystem::path kTestRoot =
    std::filesystem::temp_directory_path() /
    "nodo-finalized-artifact-gossip-admission";

void cleanup() {
    std::error_code error;
    std::filesystem::remove_all(kTestRoot, error);
}

struct CleanupGuard {
    ~CleanupGuard() { cleanup(); }
};

crypto::KeyPair validatorKey() {
    return crypto::KeyPair::createDeterministicBls12381KeyPair(
        "finalized-artifact-gossip-validator"
    );
}

config::GenesisConfig genesisConfig() {
    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kGenesisTimestamp,
        {
            config::BootstrapValidatorConfig(
                validatorKey().publicKey(),
                1,
                1,
                "finalized-artifact-gossip-validator"
            )
        },
        {},
        "finalized-artifact-gossip-genesis"
    );
}

p2p::PeerInfo localPeer() {
    return p2p::PeerInfo(
        "artifact-receiver",
        "127.0.0.1:39101",
        kProtocolVersion,
        0,
        kGenesisTimestamp
    );
}

struct RuntimeFixture {
    node::NodeRuntime runtime;
    core::Block canonicalBlock;
};

RuntimeFixture runtimeWithCanonicalBlock(
    const config::GenesisConfig& genesis
) {
    const node::NodeRuntimeStartResult started =
        node::NodeRuntimeFactory::startFromGenesis(
            node::NodeRuntimeConfig(genesis, localPeer(), 8)
        );
    if (!started.started()) {
        throw std::runtime_error(started.reason());
    }

    node::NodeRuntime runtime = started.runtime();
    const economics::ValidationWorkRecord work(
        validatorKey().address().value(),
        1,
        economics::ValidationWorkType::VALIDATE_BLOCK,
        economics::ValidationWorkResult::ACCEPTED,
        "artifact-gossip-canonical-target",
        "artifact-gossip-canonical-evidence",
        1,
        kBlockTimestamp - 1
    );
    core::Block canonicalBlock(
        1,
        runtime.blockchain().latestBlock().hash(),
        {
            core::LedgerRecord::fromValidationWorkRecord(
                work,
                kBlockTimestamp - 1
            )
        },
        kBlockTimestamp
    );
    runtime.mutableBlockchain().addBlock(canonicalBlock);
    return RuntimeFixture{
        std::move(runtime),
        std::move(canonicalBlock)
    };
}

consensus::FinalizedBlockRecord finalizedRecordFor(
    const core::Block& block,
    const core::ValidatorRegistry& validators
) {
    const crypto::KeyPair key = validatorKey();
    const crypto::Bls12381SignatureProvider provider;
    const consensus::ValidatorVoteRecord vote =
        consensus::ValidatorVoteRecord::createVote(
            key.address().value(),
            key.publicKey(),
            key.privateKeyForSigningOnly(),
            block.index(),
            block.hash(),
            block.previousHash(),
            1,
            consensus::ValidatorVoteDecision::PRECOMMIT,
            "finalized-artifact-gossip-test",
            kFinalizedAt - 1,
            provider
        );
    const consensus::QuorumCertificateBuildResult qc =
        consensus::QuorumCertificateBuilder::buildFromVotes(
            block.index(),
            block.hash(),
            block.previousHash(),
            1,
            {vote},
            validators,
            crypto::CryptoPolicy::developmentPolicy(),
            provider
        );
    if (!qc.certified()) {
        throw std::runtime_error(qc.reason());
    }
    return consensus::FinalizedBlockRecord(
        block.index(),
        block.hash(),
        block.previousHash(),
        1,
        kFinalizedAt,
        qc.certificate()
    );
}

p2p::GossipMeshConfig meshConfig(
    const std::string& nodeId,
    const config::GenesisConfig& genesis
) {
    return p2p::GossipMeshConfig(
        nodeId,
        genesis.networkParameters().networkName(),
        genesis.networkParameters().chainId(),
        kProtocolVersion,
        genesis.deterministicId(),
        60,
        4
    );
}

p2p::PeerMetadata peer(const std::string& nodeId, std::uint16_t port) {
    return p2p::PeerMetadata(
        nodeId,
        p2p::PeerEndpoint("127.0.0.1", port),
        "fingerprint-" + nodeId,
        kGenesisTimestamp,
        kGenesisTimestamp,
        0,
        false
    );
}

p2p::NetworkEnvelope deliverThroughGossip(
    p2p::GossipMesh& sender,
    p2p::GossipMesh& receiver,
    const std::string& payload,
    std::int64_t now
) {
    assert(sender.broadcast(
        p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT,
        payload,
        now
    ).acceptedCount() == 1);
    assert(sender.flushOutbound(now).acceptedCount() == 1);
    assert(receiver.receiveAvailable(now).acceptedCount() == 1);

    const std::vector<p2p::NetworkEnvelope> inbox = receiver.drainInbox(
        p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT
    );
    assert(inbox.size() == 1);
    return inbox.front();
}

std::string tamperSignature(std::string payload) {
    const std::string marker = "signatureHex=";
    const std::size_t markerPosition = payload.find(marker);
    if (markerPosition == std::string::npos) {
        throw std::runtime_error("Fixture has no signatureHex field.");
    }
    const std::size_t signaturePosition = markerPosition + marker.size();
    if (signaturePosition >= payload.size()) {
        throw std::runtime_error("Fixture signatureHex field is empty.");
    }
    payload[signaturePosition] =
        payload[signaturePosition] == '0' ? '1' : '0';
    return payload;
}

void testMalformedAndTamperedArtifactsAreRejectedThroughGossipInbox() {
    cleanup();
    CleanupGuard cleanupGuard;

    const config::GenesisConfig genesis = genesisConfig();
    RuntimeFixture fixture = runtimeWithCanonicalBlock(genesis);
    node::NodeRuntime& runtime = fixture.runtime;
    const core::Block& canonicalBlock = fixture.canonicalBlock;
    const consensus::FinalizedBlockRecord validRecord = finalizedRecordFor(
        canonicalBlock,
        runtime.validatorRegistry()
    );
    const crypto::Bls12381SignatureProvider provider;
    const crypto::CryptoPolicy policy =
        crypto::CryptoPolicy::developmentPolicy();
    const node::FinalizedBlockRecordStore store(kTestRoot);

    auto bus = std::make_shared<p2p::LoopbackTransportBus>();
    p2p::LoopbackTransport senderTransport(bus);
    p2p::LoopbackTransport receiverTransport(bus);
    p2p::GossipMesh sender(
        meshConfig("artifact-sender", genesis),
        senderTransport
    );
    p2p::GossipMesh receiver(
        meshConfig("artifact-receiver", genesis),
        receiverTransport
    );
    assert(sender.registerPeer(peer("artifact-receiver", 39101)).success());
    assert(receiver.registerPeer(peer("artifact-sender", 39102)).success());
    assert(sender.connectPeer("artifact-receiver").success());
    assert(receiver.connectPeer("artifact-sender").success());

    const p2p::NetworkEnvelope malformed = deliverThroughGossip(
        sender,
        receiver,
        "not-a-finalized-block-record",
        kFinalizedAt
    );
    const auto malformedResult =
        node::FinalizedArtifactGossipAdmission::admit(
            malformed,
            runtime,
            policy,
            provider,
            store
        );
    assert(
        malformedResult.status() ==
        node::FinalizedArtifactGossipAdmissionStatus::MALFORMED_PAYLOAD
    );
    assert(runtime.finalizationRegistry().size() == 0);
    assert(!store.load(canonicalBlock.index()).has_value());

    const core::Block differentBlock(
        canonicalBlock.index(),
        canonicalBlock.previousHash(),
        canonicalBlock.records(),
        canonicalBlock.timestamp() + 1
    );
    const consensus::FinalizedBlockRecord differentRecord = finalizedRecordFor(
        differentBlock,
        runtime.validatorRegistry()
    );
    const p2p::NetworkEnvelope mismatched = deliverThroughGossip(
        sender,
        receiver,
        differentRecord.serialize(),
        kFinalizedAt + 1
    );
    const auto mismatchResult =
        node::FinalizedArtifactGossipAdmission::admit(
            mismatched,
            runtime,
            policy,
            provider,
            store
        );
    assert(
        mismatchResult.status() ==
        node::FinalizedArtifactGossipAdmissionStatus::BLOCK_MISMATCH
    );
    assert(runtime.finalizationRegistry().size() == 0);
    assert(!store.load(canonicalBlock.index()).has_value());

    const p2p::NetworkEnvelope tampered = deliverThroughGossip(
        sender,
        receiver,
        tamperSignature(validRecord.serialize()),
        kFinalizedAt + 2
    );
    const auto tamperedResult =
        node::FinalizedArtifactGossipAdmission::admit(
            tampered,
            runtime,
            policy,
            provider,
            store
        );
    assert(
        tamperedResult.status() ==
        node::FinalizedArtifactGossipAdmissionStatus::INVALID_QUORUM_CERTIFICATE
    );
    assert(runtime.finalizationRegistry().size() == 0);
    assert(!store.load(canonicalBlock.index()).has_value());

    const p2p::NetworkEnvelope valid = deliverThroughGossip(
        sender,
        receiver,
        validRecord.serialize(),
        kFinalizedAt + 3
    );
    const auto validResult = node::FinalizedArtifactGossipAdmission::admit(
        valid,
        runtime,
        policy,
        provider,
        store
    );
    assert(validResult.acceptedRecord());
    assert(runtime.finalizationRegistry().isFinalizedBlock(
        canonicalBlock.index(),
        canonicalBlock.hash()
    ));
    const auto persisted = store.load(canonicalBlock.index());
    assert(persisted.has_value());
    assert(persisted->serialize() == validRecord.serialize());

    const p2p::NetworkEnvelope duplicate = deliverThroughGossip(
        sender,
        receiver,
        validRecord.serialize(),
        kFinalizedAt + 4
    );
    const auto duplicateResult =
        node::FinalizedArtifactGossipAdmission::admit(
            duplicate,
            runtime,
            policy,
            provider,
            store
        );
    assert(duplicateResult.duplicateRecord());
    assert(runtime.finalizationRegistry().size() == 1);
}

} // namespace

int main() {
    testMalformedAndTamperedArtifactsAreRejectedThroughGossipInbox();
    return 0;
}
