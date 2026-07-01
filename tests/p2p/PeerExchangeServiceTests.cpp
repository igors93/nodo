#include "p2p/PeerReconnectionPolicy.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace nodo::p2p;

namespace {

PeerMetadata peer(
    const std::string& nodeId,
    const std::string& host,
    std::uint16_t port,
    const std::string& fingerprint
) {
    return PeerMetadata(
        nodeId,
        PeerEndpoint(host, port),
        fingerprint,
        1000,
        1000,
        0,
        false
    );
}

void testCanonicalPayloadIsSortedAndLimited() {
    std::vector<PeerMetadata> peers;
    peers.push_back(peer("node-z", "10.0.3.2", 30333, "fp-z"));
    peers.push_back(peer("node-a", "10.0.4.2", 30334, "fp-a"));
    peers.push_back(peer("node-m", "10.0.5.2", 30335, "fp-m"));

    const auto payload = PeerExchangeService::buildPayload(peers, 2);
    assert(payload.size() == 2);
    assert(payload[0].nodeId == "node-a");
    assert(payload[1].nodeId == "node-m");

    const std::string serialized = PeerExchangeService::serializePayload(payload);
    const auto decoded = PeerExchangeService::deserializePayload(serialized);
    assert(decoded.size() == payload.size());
    assert(PeerExchangeService::serializePayload(decoded) == serialized);
}

void testAuthenticatedAdmissionUsesEclipseGuardAndBackoff() {
    PeerReconnectionPolicy policy;
    EclipseGuardConfig config = EclipseGuardConfig::defaults();
    config.maxPeersPerSubnet = 1;
    config.maxTotalPeers = 8;
    config.minSubnetDiversity = 1;
    config.maxSingleSubnetFraction = 1.0;

    std::vector<PeerSubnetInfo> active{
        PeerSubnetInfo{"already", "10.0.0.2", "10.0.0", 30333}
    };

    std::vector<PeerExchangeEntry> entries{
        PeerExchangeEntry{
            "blocked-same-subnet",
            PeerEndpoint("10.0.0.3", 30334).serialize(),
            "fp-blocked"
        },
        PeerExchangeEntry{
            "accepted-other-subnet",
            PeerEndpoint("10.0.1.3", 30335).serialize(),
            "fp-accepted"
        }
    };

    const PeerExchangeAdmissionResult result =
        PeerExchangeService::admitAuthenticatedEntries(
            entries,
            "local-node",
            active,
            config,
            policy,
            200
        );

    assert(result.acceptedCount() == 1);
    assert(result.rejectedCount() == 1);
    assert(result.acceptedEntries()[0].nodeId == "accepted-other-subnet");
    assert(policy.isTracked("accepted-other-subnet"));
    assert(!policy.isTracked("blocked-same-subnet"));
    assert(policy.candidatesForReconnect(200).empty());
    assert(policy.candidatesForReconnect(200 + PeerReconnectionPolicy::BASE_DELAY_SECONDS).size() == 1);
}

void testAdmissionRejectsSelfDuplicateAndMalformedEntries() {
    PeerReconnectionPolicy policy;
    EclipseGuardConfig config = EclipseGuardConfig::defaults();
    config.maxPeersPerSubnet = 8;
    config.maxSingleSubnetFraction = 1.0;
    config.minSubnetDiversity = 1;

    std::vector<PeerExchangeEntry> entries{
        PeerExchangeEntry{
            "local-node",
            PeerEndpoint("10.0.2.2", 30333).serialize(),
            "fp-self"
        },
        PeerExchangeEntry{
            "node-a",
            PeerEndpoint("10.0.3.2", 30334).serialize(),
            "fp-a"
        },
        PeerExchangeEntry{
            "node-a",
            PeerEndpoint("10.0.4.2", 30335).serialize(),
            "fp-a2"
        },
        PeerExchangeEntry{
            "bad-node",
            "not-an-endpoint",
            "fp-bad"
        }
    };

    const PeerExchangeAdmissionResult result =
        PeerExchangeService::admitAuthenticatedEntries(
            entries,
            "local-node",
            {},
            config,
            policy,
            500
        );

    assert(result.acceptedCount() == 1);
    assert(result.rejectedCount() == 3);
    assert(policy.isTracked("node-a"));
    assert(!policy.isTracked("local-node"));
    assert(!policy.isTracked("bad-node"));
}

} // namespace

int main() {
    testCanonicalPayloadIsSortedAndLimited();
    testAuthenticatedAdmissionUsesEclipseGuardAndBackoff();
    testAdmissionRejectsSelfDuplicateAndMalformedEntries();
    std::cout << "PeerExchangeService tests passed\n";
    return 0;
}
