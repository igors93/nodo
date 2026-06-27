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

    const auto quarantined = registry.quarantinePeer("node-A", "test quarantine");
    assert(quarantined.success());
    assert(registry.activePeers().empty());
    assert(registry.isValid());

    return 0;
}
