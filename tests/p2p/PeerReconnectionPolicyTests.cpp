#include "p2p/PeerReconnectionPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace nodo::p2p;

void testBootstrapCandidateIsImmediateAndSingleFlight() {
    PeerReconnectionPolicy policy;
    const PeerEndpoint endpoint("127.0.0.1", 30333);

    policy.recordCandidate("node-b", endpoint.serialize(), 100, true);
    assert(policy.trackedCount() == 1);
    auto ready = policy.candidatesForReconnect(100);
    assert(ready.size() == 1);
    assert(ready[0].nodeId == "node-b");

    policy.recordAttempt("node-b", 100);
    assert(policy.candidatesForReconnect(101).empty());

    policy.recordFailure("node-b", 101);
    const PeerReconnectionState* state = policy.state("node-b");
    assert(state != nullptr);
    assert(!state->attemptInFlight);
    assert(state->attempts == 1);
    assert(state->nextRetryAt == 101 + PeerReconnectionPolicy::backoffDelayForAttempt(1));
    assert(policy.candidatesForReconnect(state->nextRetryAt).size() == 1);
}

void testDisconnectedPeerUsesBaseBackoff() {
    PeerReconnectionPolicy policy;
    const PeerEndpoint endpoint("127.0.0.1", 30334);

    policy.recordDisconnect("node-c", endpoint.serialize(), 200);
    assert(policy.candidatesForReconnect(200).empty());
    assert(policy.candidatesForReconnect(200 + PeerReconnectionPolicy::BASE_DELAY_SECONDS).size() == 1);
}

void testMaxFailuresQuarantinePeer() {
    PeerReconnectionPolicy policy;
    const PeerEndpoint endpoint("127.0.0.1", 30335);
    policy.recordCandidate("node-d", endpoint.serialize(), 300, true);

    std::int64_t now = 300;
    for (std::uint32_t i = 0; i < PeerReconnectionPolicy::MAX_ATTEMPTS; ++i) {
        policy.recordAttempt("node-d", now);
        policy.recordFailure("node-d", now + 1);
        now += PeerReconnectionPolicy::MAX_DELAY_SECONDS + 10;
    }

    assert(policy.isQuarantined("node-d"));
    assert(policy.quarantineCount() == 1);
    assert(policy.candidatesForReconnect(now).empty());
}

void testPeerExchangeFeedsReconnectPolicy() {
    PeerReconnectionPolicy policy;
    PeerMetadata peer(
        "node-e",
        PeerEndpoint("10.0.0.2", 30336),
        "fingerprint-e",
        1000,
        1000,
        0,
        false
    );

    const auto payload = PeerExchangeService::buildPayload({peer}, 10);
    assert(payload.size() == 1);
    const std::string serialized = PeerExchangeService::serializePayload(payload);
    const auto decoded = PeerExchangeService::deserializePayload(serialized);
    assert(decoded.size() == 1);

    PeerExchangeService::mergeInto(decoded, policy, 400);
    assert(policy.isTracked("node-e"));
    assert(policy.candidatesForReconnect(400).size() == 1);
}

int main() {
    testBootstrapCandidateIsImmediateAndSingleFlight();
    testDisconnectedPeerUsesBaseBackoff();
    testMaxFailuresQuarantinePeer();
    testPeerExchangeFeedsReconnectPolicy();
    std::cout << "PeerReconnectionPolicy tests passed\n";
    return 0;
}
