#include "config/NetworkParameters.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/ConsensusEventLoop.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "core/TransactionBuilder.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/Hex.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/FinalizedBlockArtifactCodec.hpp"
#include "node/FinalizedBlockStore.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/PersistentBlockStateSync.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeAccountStateBuilder.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/RuntimeStateLoader.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/Peer.hpp"
#include "serialization/ProtocolMessageCodec.hpp"
#include "utils/Amount.hpp"

#include <algorithm>
#include <array>
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

constexpr std::int64_t kGenesisTimestamp = 1900700000;
constexpr std::int64_t kTransactionTimestamp = kGenesisTimestamp + 10;
constexpr const char* kProtocolVersion = "nodo/test";
constexpr const char* kCanonicalPayloadPrefix =
    "NODO_CANONICAL_PROTOCOL_HEX_V1:";

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const crypto::CryptoPolicy& developmentPolicy() {
    static const crypto::CryptoPolicy policy =
        crypto::CryptoPolicy::developmentPolicy();
    return policy;
}

std::filesystem::path testRoot() {
    return std::filesystem::temp_directory_path() /
        "nodo-three-node-protocol-e2e";
}

void cleanTestRoot() {
    std::error_code error;
    std::filesystem::remove_all(testRoot(), error);
}

struct CleanupGuard {
    ~CleanupGuard() {
        cleanTestRoot();
    }
};

struct NodeSpec {
    std::string nodeId;
    std::string endpoint;
    crypto::KeyPair validatorKey;
};

std::array<NodeSpec, 3> makeNodeSpecs() {
    return {
        NodeSpec{
            "e2e-node-a",
            "127.0.0.1:32101",
            crypto::KeyPair::createDeterministicBls12381KeyPair(
                "three-node-e2e-validator-a"
            )
        },
        NodeSpec{
            "e2e-node-b",
            "127.0.0.1:32102",
            crypto::KeyPair::createDeterministicBls12381KeyPair(
                "three-node-e2e-validator-b"
            )
        },
        NodeSpec{
            "e2e-node-c",
            "127.0.0.1:32103",
            crypto::KeyPair::createDeterministicBls12381KeyPair(
                "three-node-e2e-validator-c"
            )
        }
    };
}

crypto::KeyPair userKey() {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(
        "three-node-e2e-user"
    );
}

config::GenesisConfig makeGenesis(const std::array<NodeSpec, 3>& specs) {
    std::vector<config::BootstrapValidatorConfig> validators;
    validators.reserve(specs.size());

    for (const NodeSpec& spec : specs) {
        validators.emplace_back(
            spec.validatorKey.publicKey(),
            1,
            1,
            "three-node-e2e-" + spec.nodeId
        );
    }

    return config::GenesisConfig(
        config::NetworkParameters::developmentLocal(),
        kGenesisTimestamp,
        validators,
        {
            config::GenesisAccountConfig(
                userKey().address().value(),
                utils::Amount::fromRawUnits(2'000'000'000'000LL),
                0
            )
        },
        "three-node-protocol-e2e-genesis"
    );
}

p2p::PeerInfo peerInfo(const NodeSpec& spec) {
    return p2p::PeerInfo(
        spec.nodeId,
        spec.endpoint,
        kProtocolVersion,
        0,
        kGenesisTimestamp
    );
}

node::NodeRuntime startRuntime(
    const config::GenesisConfig& genesis,
    const NodeSpec& spec
) {
    const node::NodeRuntimeStartResult result =
        node::NodeRuntimeFactory::startFromGenesis(
            node::NodeRuntimeConfig(genesis, peerInfo(spec), 16)
        );
    require(
        result.started(),
        "Runtime start failed for " + spec.nodeId + ": " + result.reason()
    );
    return result.runtime();
}

class TestNode {
public:
    TestNode(
        const NodeSpec& spec,
        const config::GenesisConfig& genesis,
        const std::shared_ptr<p2p::LoopbackTransportBus>& bus,
        std::size_t index
    )
        : spec(spec),
          directory(testRoot() / ("node-" + std::to_string(index))),
          runtime(startRuntime(genesis, spec)),
          transport(bus),
          mesh(
              p2p::GossipMeshConfig(
                  spec.nodeId,
                  genesis.networkParameters().networkName(),
                  genesis.networkParameters().chainId(),
                  kProtocolVersion,
                  genesis.deterministicId(),
                  120,
                  8
              ),
              transport
          ) {}

    NodeSpec spec;
    node::NodeDataDirectoryConfig directory;
    node::NodeRuntime runtime;
    p2p::LoopbackTransport transport;
    p2p::GossipMesh mesh;
    std::optional<crypto::Signer> validatorSigner;
    std::unique_ptr<consensus::ConsensusEventLoop> consensusLoop;
};

p2p::PeerMetadata peerMetadata(
    const TestNode& node,
    std::size_t index
) {
    return p2p::PeerMetadata(
        node.spec.nodeId,
        p2p::PeerEndpoint("127.0.0.1", static_cast<std::uint16_t>(32101 + index)),
        "fingerprint-" + node.spec.nodeId,
        kGenesisTimestamp,
        kGenesisTimestamp,
        0,
        false
    );
}

void connectNodes(
    TestNode& left,
    std::size_t leftIndex,
    TestNode& right,
    std::size_t rightIndex
) {
    require(
        left.mesh.registerPeer(peerMetadata(right, rightIndex)).success(),
        "Unable to register " + right.spec.nodeId + " on " + left.spec.nodeId
    );
    require(
        right.mesh.registerPeer(peerMetadata(left, leftIndex)).success(),
        "Unable to register " + left.spec.nodeId + " on " + right.spec.nodeId
    );
    require(
        left.mesh.connectPeer(right.spec.nodeId).success(),
        "Unable to connect " + left.spec.nodeId + " to " + right.spec.nodeId
    );
    require(
        right.mesh.connectPeer(left.spec.nodeId).success(),
        "Unable to connect " + right.spec.nodeId + " to " + left.spec.nodeId
    );
}

void pumpNetwork(TestNode& first, TestNode& second, std::int64_t now) {
    first.mesh.flushOutbound(now);
    second.mesh.flushOutbound(now);
    first.mesh.receiveAvailable(now);
    second.mesh.receiveAvailable(now);
}

core::Transaction makeTransaction(
    const config::GenesisConfig& genesis
) {
    const crypto::Ed25519SignatureProvider provider;
    return core::TransactionBuilder::buildSignedTransfer(
        core::TransactionBuildRequest(
            "three-node-e2e-recipient",
            utils::Amount::fromRawUnits(1'000),
            utils::Amount::fromRawUnits(100),
            1,
            kTransactionTimestamp
        ),
        crypto::Signer(userKey(), provider),
        genesis.networkParameters().chainId()
    );
}

void submitAndPropagateTransaction(
    TestNode& submitter,
    TestNode& receiver,
    const config::GenesisConfig& genesis,
    const core::Transaction& transaction
) {
    const crypto::CryptoPolicy& policy = developmentPolicy();
    const auto admission = submitter.runtime.mutableMempool().admitTransaction(
        transaction,
        policy,
        crypto::SecurityContext::USER_TRANSACTION,
        kTransactionTimestamp + 1
    );
    require(admission.accepted(), "Submitting node rejected the test transaction.");

    const std::string payload = node::PersistentMempoolStore::serializeForGossip(
        transaction,
        userKey().publicKey(),
        kTransactionTimestamp + 1
    );
    require(!payload.empty(), "Transaction gossip serialization failed.");
    require(
        submitter.mesh.broadcast(
            p2p::NetworkMessageType::TRANSACTION_GOSSIP,
            payload,
            kTransactionTimestamp + 2
        ).acceptedCount() == 1,
        "Transaction was not queued for the online peer."
    );

    pumpNetwork(submitter, receiver, kTransactionTimestamp + 3);
    const std::vector<p2p::NetworkEnvelope> messages =
        receiver.mesh.drainInbox(p2p::NetworkMessageType::TRANSACTION_GOSSIP);
    require(messages.size() == 1, "Online peer did not receive transaction gossip.");
    const auto decoded = node::PersistentMempoolStore::deserializeGossip(
            messages.front().payload(),
            policy,
            crypto::SecurityContext::USER_TRANSACTION,
            genesis.networkParameters().chainId()
        );
    require(
        decoded.has_value() && receiver.runtime.mutableMempool().admitTransaction(
            decoded->transaction, policy, crypto::SecurityContext::USER_TRANSACTION,
            decoded->acceptedAt
        ).success(),
        "Online peer rejected the propagated transaction."
    );
    require(
        submitter.runtime.mempool().contains(transaction.id()) &&
        receiver.runtime.mempool().contains(transaction.id()),
        "The transaction must be present on both online nodes before consensus."
    );
}

void configureConsensus(
    TestNode& node,
    const crypto::Bls12381SignatureProvider& provider
) {
    node.validatorSigner.emplace(node.spec.validatorKey, provider);
    node.consensusLoop = std::make_unique<consensus::ConsensusEventLoop>(
        node.runtime,
        node.mesh,
        developmentPolicy(),
        provider
    );
    node.consensusLoop->setLocalValidatorAddress(
        node.validatorSigner->address()
    );
    node.consensusLoop->setLocalSigner(&node.validatorSigner.value());
    node.consensusLoop->setRecoveryPath(
        node.directory.consensusRecoveryPath()
    );
    node.consensusLoop->setDataDirectoryConfig(&node.directory);
    node.consensusLoop->setBlockProducerCallback(
        [&node](std::uint64_t, std::uint64_t round, std::int64_t now)
            -> std::optional<core::Block> {
            const consensus::BlockCandidateResult candidate =
                consensus::BlockProductionPhase::produce(
                    node.runtime,
                    node::RuntimeBlockPipelineConfig(16, 1, round, now)
                );
            if (!candidate.produced()) {
                return std::nullopt;
            }
            return candidate.block();
        }
    );
}

void finalizeWithTwoOfThreeValidators(
    TestNode& first,
    TestNode& second
) {
    for (std::int64_t step = 0; step < 20; ++step) {
        const std::int64_t now = kTransactionTimestamp + 20 + step;
        pumpNetwork(first, second, now);

        const consensus::ConsensusTickResult firstResult =
            first.consensusLoop->tick(now);
        const consensus::ConsensusTickResult secondResult =
            second.consensusLoop->tick(now);
        require(
            !firstResult.hasError(),
            first.spec.nodeId + " consensus failed: " + firstResult.errorMessage
        );
        require(
            !secondResult.hasError(),
            second.spec.nodeId + " consensus failed: " + secondResult.errorMessage
        );

        if (first.runtime.blockchain().size() == 2 &&
            second.runtime.blockchain().size() == 2) {
            break;
        }
    }

    require(
        first.runtime.blockchain().size() == 2 &&
        second.runtime.blockchain().size() == 2,
        "Two online validators did not finalize the proposed block."
    );
    require(
        first.runtime.blockchain().latestBlock().hash() ==
            second.runtime.blockchain().latestBlock().hash(),
        "Online validators finalized different block hashes."
    );

    for (TestNode* node : {&first, &second}) {
        const consensus::FinalizedBlockRecord* record =
            node->runtime.finalizationRegistry().recordForHeight(1);
        require(record != nullptr, node->spec.nodeId + " has no finalization record.");
        require(
            record->quorumCertificate().voteCount() == 2,
            "Finalization must contain the two-of-three PRECOMMIT quorum."
        );
        require(
            node->runtime.mempool().empty(),
            "Finalized transaction was not removed from the mempool."
        );
        require(
            std::filesystem::exists(
                node::FinalizedBlockStore::blockFilePath(node->directory, 1)
            ),
            node->spec.nodeId + " did not persist the finalized artifact."
        );
    }
}

std::string wrapCanonical(const std::vector<unsigned char>& bytes) {
    return std::string(kCanonicalPayloadPrefix) + crypto::hexEncode(bytes);
}

std::vector<unsigned char> unwrapCanonical(const std::string& payload) {
    const std::string prefix(kCanonicalPayloadPrefix);
    require(
        payload.rfind(prefix, 0) == 0,
        "Sync message does not use canonical protocol framing."
    );
    const std::string hex = payload.substr(prefix.size());
    require(crypto::isHexString(hex), "Sync message contains invalid hex framing.");
    return crypto::hexDecode(hex);
}

node::PersistentBlockSyncBatch buildSyncBatch(
    const TestNode& source,
    const node::NetworkBlockSyncRequest& request,
    std::int64_t now
) {
    require(request.isValid(), "Source received an invalid sync request.");
    require(
        request.locator().fromHeight() == 1,
        "The lagging node must request the first finalized height."
    );
    require(
        std::find(
            request.locator().knownAncestorHashes().begin(),
            request.locator().knownAncestorHashes().end(),
            source.runtime.blockchain().blocks().front().hash()
        ) != request.locator().knownAncestorHashes().end(),
        "Sync request is not anchored to the shared genesis block."
    );

    const core::Block& block = source.runtime.blockchain().blocks().at(1);
    const node::FinalizedBlockArtifact artifact =
        node::FinalizedBlockArtifactCodec::readBlockArtifactFile(
            node::FinalizedBlockStore::blockFilePath(source.directory, block.index())
        );
    require(
        artifact.block().hash() == block.hash(),
        "Persisted source artifact does not match its runtime chain."
    );

    const node::PersistentBlockSyncItem item(
        block.index(),
        block.hash(),
        block.previousHash(),
        block.serialize(),
        block.stateRoot(),
        now,
        artifact.finalizedRecord().serialize()
    );
    return node::PersistentBlockSyncBatch(
        source.spec.nodeId,
        block.index(),
        block.index(),
        {item},
        now
    );
}

void synchronizeLaggingNode(
    TestNode& restartedSource,
    TestNode& lagging,
    const config::GenesisConfig& genesis,
    std::int64_t now
) {
    const core::Block& sourceTip =
        restartedSource.runtime.blockchain().latestBlock();
    const core::Block& laggingTip = lagging.runtime.blockchain().latestBlock();

    const node::PersistentSyncCheckpoint checkpoint =
        node::PersistentSyncCheckpoint::genesis(
            laggingTip.hash(),
            "genesis-state-root",
            now
        );
    const node::ChainStatusMessage remoteStatus(
        genesis.networkParameters().networkName(),
        genesis.networkParameters().chainId(),
        kProtocolVersion,
        sourceTip.index(),
        sourceTip.hash(),
        sourceTip.index(),
        sourceTip.hash()
    );
    const node::PersistentSyncPlan plan =
        node::PersistentBlockStateSyncPlanner::planFromRemoteStatus(
            checkpoint,
            remoteStatus,
            lagging.spec.nodeId,
            restartedSource.spec.nodeId,
            node::NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH,
            now
        );
    require(plan.requestBlocks(), "Lagging node did not plan a block sync request.");

    require(
        lagging.mesh.sendTo(
            restartedSource.spec.nodeId,
            p2p::NetworkMessageType::BLOCK_SYNC_REQUEST,
            wrapCanonical(
                serialization::ProtocolMessageCodec::encodeNetworkBlockSyncRequest(
                    plan.blockRequest().value()
                )
            ),
            now
        ).acceptedCount() == 1,
        "Lagging node could not queue its sync request."
    );
    pumpNetwork(lagging, restartedSource, now + 1);

    const std::vector<p2p::NetworkEnvelope> requests =
        restartedSource.mesh.drainInbox(
            p2p::NetworkMessageType::BLOCK_SYNC_REQUEST
        );
    require(requests.size() == 1, "Restarted source did not receive the sync request.");
    const node::NetworkBlockSyncRequest decodedRequest =
        serialization::ProtocolMessageCodec::decodeNetworkBlockSyncRequest(
            unwrapCanonical(requests.front().payload())
        );
    require(
        decodedRequest.requesterNodeId() == requests.front().senderNodeId(),
        "Sync requester identity does not match the authenticated sender."
    );

    const node::PersistentBlockSyncBatch response =
        buildSyncBatch(restartedSource, decodedRequest, now + 2);
    require(response.isValid(), "Restarted source built an invalid sync batch.");
    require(
        restartedSource.mesh.sendTo(
            lagging.spec.nodeId,
            p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE,
            wrapCanonical(
                node::PersistentBlockStateSyncCodec::encodeBlockSyncBatch(response)
            ),
            now + 2
        ).acceptedCount() == 1,
        "Restarted source could not queue its sync response."
    );
    pumpNetwork(restartedSource, lagging, now + 3);

    const std::vector<p2p::NetworkEnvelope> responses =
        lagging.mesh.drainInbox(
            p2p::NetworkMessageType::BLOCK_SYNC_RESPONSE
        );
    require(responses.size() == 1, "Lagging node did not receive the sync response.");
    const node::PersistentBlockSyncBatch decodedResponse =
        node::PersistentBlockStateSyncCodec::decodeBlockSyncBatch(
            unwrapCanonical(responses.front().payload())
        );
    require(
        decodedResponse.sourcePeerId() == responses.front().senderNodeId(),
        "Sync batch source does not match the authenticated sender."
    );

    node::PersistentSyncCheckpointStore cpStore(lagging.directory.rootPath());
    const node::PersistentSyncApplyResult applied =
        node::PersistentBlockStateSyncApplier::importFinalizedBatch(
            checkpoint,
            decodedResponse,
            lagging.runtime,
            lagging.directory,
            &cpStore,
            now + 4
        );
    require(applied.applied(), "Lagging node rejected sync: " + applied.reason());
    require(
        applied.checkpoint().has_value() &&
        node::PersistentSyncCheckpointStore(lagging.directory.rootPath())
            .read().loaded(),
        "Lagging node could not persist its sync checkpoint."
    );
}

std::size_t proposerIndex(
    const std::array<std::unique_ptr<TestNode>, 3>& nodes,
    const config::GenesisConfig& genesis
) {
    const node::NodeRuntime& runtime = nodes.front()->runtime;
    const consensus::ConsensusRoundState& state =
        runtime.consensusRoundManager().currentState();
    const std::string proposer = consensus::ProposerSchedule::selectProposer(
        runtime.validatorRegistry(),
        genesis.networkParameters().chainId(),
        state.height(),
        state.round()
    );

    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (nodes[index]->spec.validatorKey.address().value() == proposer) {
            return index;
        }
    }
    throw std::runtime_error("Scheduled proposer is absent from the three-node fixture.");
}

void testThreeNodeProtocolJourney() {
    cleanTestRoot();
    CleanupGuard cleanup;

    const std::array<NodeSpec, 3> specs = makeNodeSpecs();
    const config::GenesisConfig genesis = makeGenesis(specs);
    const auto bus = std::make_shared<p2p::LoopbackTransportBus>();

    std::array<std::unique_ptr<TestNode>, 3> nodes;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const node::NodeDataDirectoryConfig directory(
            testRoot() / ("node-" + std::to_string(index))
        );
        const node::NodeDataDirectoryInitResult initialized =
            node::NodeDataDirectory::initialize(
                directory,
                genesis,
                peerInfo(specs[index]),
                kGenesisTimestamp + 1
            );
        require(
            initialized.initialized(),
            "Data directory initialization failed for " + specs[index].nodeId +
                ": " + initialized.reason()
        );
        nodes[index] = std::make_unique<TestNode>(specs[index], genesis, bus, index);
    }

    const std::size_t producerIndex = proposerIndex(nodes, genesis);
    std::size_t voterIndex = 0;
    while (voterIndex == producerIndex) {
        ++voterIndex;
    }
    std::size_t laggingIndex = 0;
    while (laggingIndex == producerIndex || laggingIndex == voterIndex) {
        ++laggingIndex;
    }

    TestNode& producer = *nodes[producerIndex];
    TestNode& voter = *nodes[voterIndex];
    TestNode& lagging = *nodes[laggingIndex];
    connectNodes(producer, producerIndex, voter, voterIndex);

    const core::Transaction transaction = makeTransaction(genesis);
    submitAndPropagateTransaction(voter, producer, genesis, transaction);
    require(
        lagging.runtime.mempool().empty(),
        "Offline node must not receive live transaction gossip."
    );

    const crypto::Bls12381SignatureProvider validatorProvider;
    configureConsensus(producer, validatorProvider);
    configureConsensus(voter, validatorProvider);
    finalizeWithTwoOfThreeValidators(producer, voter);
    require(
        lagging.runtime.blockchain().size() == 1,
        "Offline node must remain at genesis before catch-up sync."
    );

    producer.consensusLoop.reset();
    voter.consensusLoop.reset();
    const node::RuntimeStateLoadResult restarted =
        node::RuntimeStateLoader::loadFromDataDirectory(
            producer.directory,
            genesis,
            peerInfo(producer.spec)
        );
    require(
        restarted.loaded(),
        "Finalized node failed to restart: " + restarted.reason()
    );
    producer.runtime = restarted.runtime();
    require(
        producer.runtime.blockchain().size() == 2 &&
        producer.runtime.finalizationRegistry().hasFinalizedHeight(1),
        "Restarted node did not recover its finalized chain and QC."
    );

    connectNodes(producer, producerIndex, lagging, laggingIndex);
    synchronizeLaggingNode(producer, lagging, genesis, kTransactionTimestamp + 100);

    require(
        lagging.runtime.blockchain().latestBlock().hash() ==
            producer.runtime.blockchain().latestBlock().hash(),
        "Lagging node did not converge to the restarted source tip."
    );
    require(
        lagging.runtime.finalizationRegistry().hasFinalizedHeight(1),
        "Lagging node did not import the block finalization proof."
    );
    require(
        lagging.runtime.supplyState().serialize() ==
            producer.runtime.supplyState().serialize(),
        "Lagging node supply state differs after canonical sync."
    );

    const std::int64_t minimumFee = static_cast<std::int64_t>(
        genesis.networkParameters().minimumFeeRawUnits()
    );
    const core::AccountStateView producerAccounts =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesis,
            producer.runtime.blockchain(),
            minimumFee
        );
    const core::AccountStateView laggingAccounts =
        node::RuntimeAccountStateBuilder::accountStateViewAtTip(
            genesis,
            lagging.runtime.blockchain(),
            minimumFee
        );
    require(
        laggingAccounts.serialize() == producerAccounts.serialize(),
        "Lagging node account balances or nonces differ after sync."
    );
    require(
        laggingAccounts.accountOrDefault(userKey().address().value()).nonce() == 1,
        "Synchronized sender nonce does not reflect the finalized transaction."
    );
    require(
        lagging.runtime.mempool().empty(),
        "Lagging node retained a finalized transaction in its mempool."
    );

    const node::RuntimeStateLoadResult laggingRestart =
        node::RuntimeStateLoader::loadFromDataDirectory(
            lagging.directory,
            genesis,
            peerInfo(lagging.spec)
        );
    require(
        laggingRestart.loaded(),
        "Synchronized node failed to restart: " + laggingRestart.reason()
    );
    require(
        laggingRestart.runtime().blockchain().latestBlock().hash() ==
            producer.runtime.blockchain().latestBlock().hash(),
        "Synchronized tip was not durable across restart."
    );
    require(
        laggingRestart.runtime().supplyState().serialize() ==
            producer.runtime.supplyState().serialize(),
        "Synchronized supply state was not durable across restart."
    );
}

} // namespace

int main() {
    try {
        testThreeNodeProtocolJourney();
        std::cout << "Three-node protocol end-to-end test passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Three-node protocol end-to-end test FAILED: "
                  << error.what() << '\n';
        return 1;
    }
}
