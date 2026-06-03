#include "node/ChainSyncMessages.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>

using namespace nodo::node;
using namespace nodo::p2p;

int main() {
    const std::string genesisA = "genesis-network-a-v1";
    const std::string genesisB = "genesis-network-b-v1";

    GossipMeshConfig localConfig("node-local", "localnet", "chain-1", "1", genesisA, 60, 2);
    GossipMeshConfig remoteConfigSameGenesis("node-remote", "localnet", "chain-1", "1", genesisA, 60, 2);
    GossipMeshConfig remoteConfigDifferentGenesis("node-remote", "localnet", "chain-1", "1", genesisB, 60, 2);

    PeerMetadata remotePeer(
        "node-remote",
        PeerEndpoint("127.0.0.1", 19002),
        "fingerprint-node-remote",
        1000, 1000, 0, false
    );

    ChainStatusMessage status(
        "localnet", "chain-1", "1",
        10, "block-hash-10",
        10, "block-hash-10"
    );

    // Peer with same genesis must be accepted.
    NetworkEnvelope helloSameGenesis =
        PeerHandshakeManager::createHelloEnvelope(remoteConfigSameGenesis, remotePeer, status, 1000);

    const PeerHandshakeResult acceptedResult =
        PeerHandshakeManager::validateHello(localConfig, helloSameGenesis, 1001);
    assert(acceptedResult.accepted());

    // Peer with different genesis must be rejected.
    NetworkEnvelope helloDifferentGenesis =
        PeerHandshakeManager::createHelloEnvelope(remoteConfigDifferentGenesis, remotePeer, status, 1000);

    const PeerHandshakeResult rejectedResult =
        PeerHandshakeManager::validateHello(localConfig, helloDifferentGenesis, 1001);
    assert(!rejectedResult.accepted());
    assert(rejectedResult.reason().find("genesis") != std::string::npos);

    // Peer with wrong network must also be rejected.
    GossipMeshConfig wrongNetConfig("node-remote", "wrongnet", "chain-1", "1", genesisA, 60, 2);
    NetworkEnvelope helloWrongNet =
        PeerHandshakeManager::createHelloEnvelope(wrongNetConfig, remotePeer, status, 1000);

    const PeerHandshakeResult networkRejected =
        PeerHandshakeManager::validateHello(localConfig, helloWrongNet, 1001);
    assert(!networkRejected.accepted());

    return 0;
}
