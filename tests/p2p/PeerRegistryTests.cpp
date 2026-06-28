#include "p2p/PeerRegistry.hpp"

#include <cassert>

int main() {
    nodo::p2p::PeerRegistry registry;

    nodo::p2p::PeerMetadata peer(
        "node-A",
        nodo::p2p::PeerEndpoint("127.0.0.1", 18181),
        "fingerprint-A",
        1000,
        1000,
        0,
        false
    );

    const auto registered = registry.registerPeer(peer);
    assert(registered.success());
    assert(registry.contains("node-A"));
    assert(registry.size() == 1);
    assert(registry.activePeers().size() == 1);

    const auto heartbeat = registry.updateHeartbeat("node-A", 1010);
    assert(heartbeat.success());
    assert(registry.peer("node-A") != nullptr);
    assert(registry.peer("node-A")->lastSeenAt() == 1010);

    const auto penalized = registry.adjustScore(
        "node-A",
        -10,
        "invalid synchronization message"
    );
    assert(penalized.success());
    assert(registry.peer("node-A")->score() == -10);

    const auto quarantined = registry.quarantinePeer("node-A", "test quarantine");
    assert(quarantined.success());
    assert(registry.activePeers().empty());

    nodo::p2p::PeerMetadata refreshedPeer(
        "node-A",
        nodo::p2p::PeerEndpoint("127.0.0.1", 18182),
        "fingerprint-A",
        1020,
        1020,
        0,
        false
    );
    assert(registry.registerPeer(refreshedPeer).success());
    assert(registry.peer("node-A")->endpoint().port() == 18182);
    assert(registry.peer("node-A")->score() == -10);
    assert(registry.peer("node-A")->quarantined());
    assert(registry.containsIdentityKey(peer.identityKey()));
    assert(registry.peerByIdentityKey(peer.identityKey()) != nullptr);
    assert(registry.peerByIdentityKey(peer.identityKey())->nodeId() == "node-A");

    nodo::p2p::PeerMetadata nodeIdTakeover(
        "node-A",
        nodo::p2p::PeerEndpoint("127.0.0.1", 18183),
        "fingerprint-attacker",
        1030,
        1030,
        0,
        false
    );
    assert(!registry.registerPeer(nodeIdTakeover).success());
    assert(registry.peer("node-A")->publicKeyFingerprint() == "fingerprint-A");
    assert(registry.peer("node-A")->score() == -10);

    nodo::p2p::PeerMetadata identityAlias(
        "node-A-rotated",
        nodo::p2p::PeerEndpoint("127.0.0.1", 18184),
        "FINGERPRINT-A",
        1040,
        1040,
        0,
        false
    );
    assert(!registry.registerPeer(identityAlias).success());
    assert(!registry.contains("node-A-rotated"));
    assert(registry.size() == 1);
    assert(registry.isValid());

    return 0;
}
