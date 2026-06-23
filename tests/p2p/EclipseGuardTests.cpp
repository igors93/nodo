#include "p2p/EclipseGuard.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::p2p::EclipseCheckOutcome;
using nodo::p2p::EclipseGuard;
using nodo::p2p::EclipseGuardConfig;
using nodo::p2p::PeerSubnetInfo;

void requireCondition(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

PeerSubnetInfo makePeer(
    const std::string& id,
    const std::string& ip
) {
    PeerSubnetInfo peer;
    peer.peerId       = id;
    peer.ipAddress    = ip;
    peer.subnetPrefix = PeerSubnetInfo::extractSubnetPrefix(ip);
    peer.port         = 9000;
    return peer;
}

void testAllowsPeerFromNewSubnet() {
    EclipseGuard guard;

    // Two peers from one subnet — adding a third from a NEW subnet
    const std::vector<PeerSubnetInfo> current = {
        makePeer("peer-1", "192.168.1.10"),
        makePeer("peer-2", "10.0.0.5")
    };

    const PeerSubnetInfo candidate = makePeer("peer-3", "172.16.0.1");

    const auto result = guard.checkAdmission(candidate, current);

    requireCondition(
        result.isAllowed(),
        "Peer from a new subnet should be allowed when diversity is low."
    );

    requireCondition(
        result.outcome() == EclipseCheckOutcome::ALLOWED,
        "Outcome should be ALLOWED for a peer from a fresh subnet."
    );
}

void testRejectsWhenSubnetSaturated() {
    EclipseGuardConfig config = EclipseGuardConfig::defaults();
    config.maxPeersPerSubnet = 2;
    config.maxSingleSubnetFraction = 1.0;  // disable fraction check for this test
    EclipseGuard guard(config);

    // Already 2 peers from 192.168.1.x
    const std::vector<PeerSubnetInfo> current = {
        makePeer("peer-a", "192.168.1.10"),
        makePeer("peer-b", "192.168.1.20"),
        makePeer("peer-c", "10.0.0.5"),
        makePeer("peer-d", "10.0.1.5"),
        makePeer("peer-e", "10.0.2.5"),
        makePeer("peer-f", "10.0.3.5"),
        makePeer("peer-g", "10.0.4.5"),
        makePeer("peer-h", "10.0.5.5"),
        makePeer("peer-i", "10.0.6.5"),
    };

    const PeerSubnetInfo candidate = makePeer("peer-new", "192.168.1.30");

    const auto result = guard.checkAdmission(candidate, current);

    requireCondition(
        !result.isAllowed(),
        "Peer should be rejected when its subnet already has maxPeersPerSubnet peers."
    );

    requireCondition(
        result.outcome() == EclipseCheckOutcome::REJECTED_SUBNET_SATURATED,
        "Outcome should be REJECTED_SUBNET_SATURATED."
    );
}

void testDiversityScoreForAllDistinctSubnets() {
    EclipseGuard guard;

    const std::vector<PeerSubnetInfo> peers = {
        makePeer("peer-1", "10.0.0.1"),
        makePeer("peer-2", "10.0.1.1"),
        makePeer("peer-3", "10.0.2.1"),
        makePeer("peer-4", "10.0.3.1"),
    };

    const double score = guard.diversityScore(peers);

    requireCondition(
        score == 1.0,
        "diversityScore should return 1.0 when all peers are from distinct subnets."
    );
}

void testRecommendEvictionsFromOverRepresentedSubnet() {
    EclipseGuard guard;

    // 3 peers from the same subnet, 1 from each of two others
    const std::vector<PeerSubnetInfo> peers = {
        makePeer("over-1", "192.168.1.10"),
        makePeer("over-2", "192.168.1.20"),
        makePeer("over-3", "192.168.1.30"),
        makePeer("other-1", "10.0.0.1"),
        makePeer("other-2", "10.0.1.1"),
    };

    const auto toEvict = guard.recommendEvictions(peers, 3);

    requireCondition(
        !toEvict.empty(),
        "recommendEvictions should return at least one peer to evict."
    );

    // The evicted peers should be from the over-represented subnet
    for (const auto& peerId : toEvict) {
        requireCondition(
            peerId == "over-1" || peerId == "over-2" || peerId == "over-3",
            "Evicted peer should be from the over-represented subnet."
        );
    }
}

void testExtractSubnetPrefixParsesIPv4() {
    requireCondition(
        PeerSubnetInfo::extractSubnetPrefix("192.168.1.42") == "192.168.1",
        "extractSubnetPrefix should return first three octets for 192.168.1.42."
    );

    requireCondition(
        PeerSubnetInfo::extractSubnetPrefix("10.0.0.1") == "10.0.0",
        "extractSubnetPrefix should return first three octets for 10.0.0.1."
    );

    requireCondition(
        PeerSubnetInfo::extractSubnetPrefix("255.255.255.0") == "255.255.255",
        "extractSubnetPrefix should handle 255.255.255.0."
    );

    requireCondition(
        PeerSubnetInfo::extractSubnetPrefix("").empty(),
        "extractSubnetPrefix should return empty for empty string."
    );

    requireCondition(
        PeerSubnetInfo::extractSubnetPrefix("not-an-ip").empty(),
        "extractSubnetPrefix should return empty for non-IP string."
    );
}

} // namespace

int main() {
    try {
        testAllowsPeerFromNewSubnet();
        testRejectsWhenSubnetSaturated();
        testDiversityScoreForAllDistinctSubnets();
        testRecommendEvictionsFromOverRepresentedSubnet();
        testExtractSubnetPrefixParsesIPv4();

        std::cout << "Nodo EclipseGuard tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo EclipseGuard tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
