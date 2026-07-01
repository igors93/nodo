#include "p2p/PeerRegistry.hpp"
#include "p2p/PeerReputation.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace nodo::p2p;

namespace {

PeerMetadata makePeer(const std::string& nodeId) {
    return PeerMetadata(
        nodeId,
        PeerEndpoint("10.1.2.3", 30333),
        "fingerprint-" + nodeId,
        1000,
        1000,
        0,
        false
    );
}

void testPeerReputationTemporaryBanLifecycle() {
    PeerReputation reputation(50, 2, 120);
    reputation.reportBehavior(
        "node-a",
        "10.1.2.3",
        -60,
        1000,
        "p2p.invalid-message"
    );
    assert(reputation.getScore("node-a") == 40);
    assert(reputation.isBanned("node-a"));
    assert(reputation.isTemporarilyBanned("node-a", 1001));
    assert(reputation.bannedUntil("node-a") == 1120);
    assert(reputation.liftExpiredBans(1119) == 0);
    assert(reputation.isTemporarilyBanned("node-a", 1119));
    assert(reputation.liftExpiredBans(1120) == 1);
    assert(!reputation.isTemporarilyBanned("node-a", 1120));
}

void testRegistryBanIsPersistentAndLiftable() {
    PeerRegistry registry;
    assert(registry.registerPeer(makePeer("node-a")).success());
    assert(registry.banPeer("node-a", 1300, "p2p.temporary-ban").success());
    const PeerMetadata* banned = registry.peer("node-a");
    assert(banned != nullptr);
    assert(banned->quarantined());
    assert(banned->bannedAt(1200));
    assert(banned->bannedUntil() == 1300);
    assert(banned->banReason() == "p2p.temporary-ban");
    assert(registry.activePeersAt(1200).empty());
    assert(registry.liftExpiredPeerPenalties(1299) == 0);
    assert(registry.liftExpiredPeerPenalties(1300) == 1);
    const PeerMetadata* lifted = registry.peer("node-a");
    assert(lifted != nullptr);
    assert(!lifted->quarantined());
    assert(!lifted->bannedAt(1300));
    assert(lifted->bannedUntil() == 0);
    assert(registry.activePeersAt(1300).size() == 1);
}

} // namespace

int main() {
    testPeerReputationTemporaryBanLifecycle();
    testRegistryBanIsPersistentAndLiftable();
    std::cout << "PeerReputationBanPolicy tests passed\n";
    return 0;
}
