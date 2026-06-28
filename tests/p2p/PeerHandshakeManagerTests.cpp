#include "node/ChainSyncMessages.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>

using namespace nodo::node;
using namespace nodo::p2p;

int main() {
    const std::string challengeNonce(64, 'a');
    const nodo::crypto::KeyPair remoteIdentity =
        nodo::crypto::KeyPair::createDeterministicEd25519KeyPair(
            "peer-handshake-manager-remote"
        );
    const std::string sharedGenesisId = "localnet-genesis-test-v1";
    GossipMeshConfig localConfig("node-a", "localnet", "chain-localnet", "1", sharedGenesisId, 60, 2);
    GossipMeshConfig remoteConfig("node-b", "localnet", "chain-localnet", "1", sharedGenesisId, 60, 2);

    const NetworkEnvelope challenge =
        PeerHandshakeManager::createChallengeEnvelope(
            localConfig,
            "node-b",
            challengeNonce,
            999
        );
    const auto parsedChallenge =
        PeerHandshakeManager::challengeFromEnvelope(
            remoteConfig,
            challenge,
            1000
        );
    assert(parsedChallenge.has_value());
    assert(parsedChallenge->challengerNodeId() == "node-a");
    assert(parsedChallenge->challengedNodeId() == "node-b");
    assert(parsedChallenge->nonce() == challengeNonce);
    assert(!PeerHandshakeManager::challengeFromEnvelope(
        localConfig,
        challenge,
        1000
    ).has_value());

    PeerMetadata remotePeer(
        "node-b",
        PeerEndpoint("127.0.0.1", 19002),
        remoteIdentity.publicKey().fingerprint(),
        1000,
        1000,
        0,
        false
    );

    ChainStatusMessage status(
        "localnet",
        "chain-localnet",
        "1",
        10,
        "block-hash-10",
        10,
        "block-hash-10"
    );

    NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remoteConfig,
            remotePeer,
            status,
            "node-a",
            challengeNonce,
            remoteIdentity,
            1000
        );

    assert(hello.messageType() == NetworkMessageType::PEER_HELLO);
    assert(hello.senderNodeId() == "node-b");

    const PeerHandshakeResult accepted =
        PeerHandshakeManager::validateHello(
            localConfig, hello, challengeNonce, 1001
        );

    assert(accepted.accepted());

    GossipMeshConfig wrongChain("node-a", "localnet", "other-chain", "1", sharedGenesisId, 60, 2);
    const PeerHandshakeResult rejected =
        PeerHandshakeManager::validateHello(
            wrongChain, hello, challengeNonce, 1001
        );

    assert(!rejected.accepted());

    return 0;
}
