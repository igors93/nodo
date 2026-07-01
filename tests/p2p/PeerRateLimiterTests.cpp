#include "p2p/PeerRateLimiter.hpp"

#include <cassert>
#include <string>

namespace {

void testFirstMessageAllowed() {
    nodo::p2p::PeerRateLimiter limiter(5, 60);
    assert(limiter.shouldAllow("peer-a", 1000));
}

void testFloodBlockedAfterThreshold() {
    nodo::p2p::PeerRateLimiter limiter(3, 60);
    assert(limiter.shouldAllow("peer-a", 1000));
    assert(limiter.shouldAllow("peer-a", 1001));
    assert(limiter.shouldAllow("peer-a", 1002));
    // 4th message exceeds limit of 3
    assert(!limiter.shouldAllow("peer-a", 1003));
    assert(!limiter.shouldAllow("peer-a", 1004));
}

void testWindowResetAllowsNewMessages() {
    nodo::p2p::PeerRateLimiter limiter(2, 10);
    assert(limiter.shouldAllow("peer-a", 1000));
    assert(limiter.shouldAllow("peer-a", 1001));
    assert(!limiter.shouldAllow("peer-a", 1002)); // blocked

    // Jump past the 10-second window.
    assert(limiter.shouldAllow("peer-a", 1011)); // new window starts
    assert(limiter.shouldAllow("peer-a", 1012));
    assert(!limiter.shouldAllow("peer-a", 1013)); // blocked again
}

void testDifferentPeersAreIndependent() {
    nodo::p2p::PeerRateLimiter limiter(2, 60);
    assert(limiter.shouldAllow("peer-a", 1000));
    assert(limiter.shouldAllow("peer-a", 1001));
    assert(!limiter.shouldAllow("peer-a", 1002));

    // peer-b has its own window.
    assert(limiter.shouldAllow("peer-b", 1000));
    assert(limiter.shouldAllow("peer-b", 1001));
}

void testMessageCountReturnsCorrectValue() {
    nodo::p2p::PeerRateLimiter limiter(10, 60);
    assert(limiter.messageCount("peer-a", 1000) == 0);
    limiter.shouldAllow("peer-a", 1000);
    limiter.shouldAllow("peer-a", 1001);
    assert(limiter.messageCount("peer-a", 1002) == 2);
}

void testDefaultConstructorHasSensibleDefaults() {
    nodo::p2p::PeerRateLimiter limiter;
    assert(limiter.maxMessagesPerWindow() == nodo::p2p::DEFAULT_RATE_LIMIT_MESSAGES);
    assert(limiter.windowSeconds() == nodo::p2p::DEFAULT_RATE_LIMIT_WINDOW_SECONDS);
}

void testLimitsArePerMessageType() {
    nodo::p2p::PeerRateLimiter limiter(2, 60);
    assert(limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PING, 1000));
    assert(limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PING, 1001));
    assert(!limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PING, 1002));

    assert(limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PONG, 1003));
    assert(limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PONG, 1004));
    assert(!limiter.shouldAllow("peer-a", nodo::p2p::NetworkMessageType::PONG, 1005));

    assert(limiter.messageCount("peer-a", nodo::p2p::NetworkMessageType::PING, 1006) == 2);
    assert(limiter.messageCount("peer-a", nodo::p2p::NetworkMessageType::PONG, 1006) == 2);
    assert(limiter.messageCount("peer-a", 1006) == 4);
}

} // namespace

int main() {
    testFirstMessageAllowed();
    testFloodBlockedAfterThreshold();
    testWindowResetAllowsNewMessages();
    testDifferentPeersAreIndependent();
    testMessageCountReturnsCorrectValue();
    testDefaultConstructorHasSensibleDefaults();
    testLimitsArePerMessageType();
    return 0;
}
