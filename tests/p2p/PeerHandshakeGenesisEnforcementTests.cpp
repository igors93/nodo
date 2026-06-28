#include "node/ChainSyncMessages.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>

using namespace nodo::node;
using namespace nodo::p2p;

int main() {
    const std::string challengeNonce(64, 'b');
    const nodo::crypto::KeyPair remoteIdentity =
        nodo::crypto::KeyPair::createDeterministicEd25519KeyPair(
            "peer-handshake-genesis-remote"
        );
    const std::string genesisA = "genesis-network-a-v1";
    const std::string genesisB = "genesis-network-b-v1";

    GossipMeshConfig localConfig("node-local", "localnet", "chain-1", "1", genesisA, 60, 2);
    GossipMeshConfig remoteConfigSameGenesis("node-remote", "localnet", "chain-1", "1", genesisA, 60, 2);
    GossipMeshConfig remoteConfigDifferentGenesis("node-remote", "localnet", "chain-1", "1", genesisB, 60, 2);

    PeerMetadata remotePeer(
        "node-remote",
        PeerEndpoint("127.0.0.1", 19002),
        remoteIdentity.publicKey().fingerprint(),
        1000, 1000, 0, false
    );

    ChainStatusMessage status(
        "localnet", "chain-1", "1",
        10, "block-hash-10",
        10, "block-hash-10"
    );

    // Peer with same genesis must be accepted.
    NetworkEnvelope helloSameGenesis =
        PeerHandshakeManager::createHelloEnvelope(
            remoteConfigSameGenesis,
            remotePeer,
            status,
            "node-local",
            challengeNonce,
            remoteIdentity,
            1000
        );

    const PeerHandshakeResult acceptedResult =
        PeerHandshakeManager::validateHello(
            localConfig, helloSameGenesis, challengeNonce, 1001
        );
    assert(acceptedResult.accepted());

    // Peer with different genesis must be rejected.
    NetworkEnvelope helloDifferentGenesis =
        PeerHandshakeManager::createHelloEnvelope(
            remoteConfigDifferentGenesis,
            remotePeer,
            status,
            "node-local",
            challengeNonce,
            remoteIdentity,
            1000
        );

    const PeerHandshakeResult rejectedResult =
        PeerHandshakeManager::validateHello(
            localConfig, helloDifferentGenesis, challengeNonce, 1001
        );
    assert(!rejectedResult.accepted());
    assert(rejectedResult.reason().find("genesis") != std::string::npos);

    // Peer with wrong network must also be rejected.
    GossipMeshConfig wrongNetConfig("node-remote", "wrongnet", "chain-1", "1", genesisA, 60, 2);
    ChainStatusMessage wrongNetStatus(
        "wrongnet", "chain-1", "1",
        10, "block-hash-10",
        10, "block-hash-10"
    );
    NetworkEnvelope helloWrongNet =
        PeerHandshakeManager::createHelloEnvelope(
            wrongNetConfig,
            remotePeer,
            wrongNetStatus,
            "node-local",
            challengeNonce,
            remoteIdentity,
            1000
        );

    const PeerHandshakeResult networkRejected =
        PeerHandshakeManager::validateHello(
            localConfig, helloWrongNet, challengeNonce, 1001
        );
    assert(!networkRejected.accepted());

    return 0;
}
