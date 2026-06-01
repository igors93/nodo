#include "node/ChainSyncMessages.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>

using namespace nodo::node;
using namespace nodo::p2p;

int main() {
    GossipMeshConfig localConfig("node-a", "localnet", "chain-localnet", "1", 60, 2);
    GossipMeshConfig remoteConfig("node-b", "localnet", "chain-localnet", "1", 60, 2);

    PeerMetadata remotePeer(
        "node-b",
        PeerEndpoint("127.0.0.1", 19002),
        "fingerprint-node-b",
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
        PeerHandshakeManager::createHelloEnvelope(remoteConfig, remotePeer, status, 1000);

    assert(hello.messageType() == NetworkMessageType::PEER_HELLO);
    assert(hello.senderNodeId() == "node-b");

    const PeerHandshakeResult accepted =
        PeerHandshakeManager::validateHello(localConfig, hello, 1001);

    assert(accepted.accepted());

    GossipMeshConfig wrongChain("node-a", "localnet", "other-chain", "1", 60, 2);
    const PeerHandshakeResult rejected =
        PeerHandshakeManager::validateHello(wrongChain, hello, 1001);

    assert(!rejected.accepted());

    return 0;
}
