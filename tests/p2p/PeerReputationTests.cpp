#include "p2p/PeerReputation.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::p2p::PeerReputation;

void requireCondition(bool condition, const std::string& failureMessage) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

void testPeerScoresAndBan() {
    PeerReputation pr(50, 2); // ban threshold = 50, max per subnet = 2

    // Peer starts at score 100
    requireCondition(pr.getScore("peer1") == 100, "Initial score should be 100");
    requireCondition(!pr.isBanned("peer1"), "Peer should not be banned initially");

    // Report negative behavior
    pr.reportBehavior("peer1", "192.168.1.10", -30);
    requireCondition(pr.getScore("peer1") == 70, "Score should drop to 70");
    requireCondition(!pr.isBanned("peer1"), "Score 70 should not trigger a ban");

    // Report more negative behavior
    pr.reportBehavior("peer1", "192.168.1.10", -25);
    requireCondition(pr.getScore("peer1") == 45, "Score should drop to 45");
    requireCondition(pr.isBanned("peer1"), "Score below 50 should trigger a ban");
}

void testEclipsePrevention() {
    PeerReputation pr(50, 2); // max 2 peers per Class C subnet

    // Try connecting peers from same Class C subnet
    requireCondition(pr.allowConnection("192.168.1.10"), "First connection from 192.168.1.10 should be allowed");
    requireCondition(pr.allowConnection("192.168.1.20"), "Second connection from 192.168.1.20 should be allowed");
    requireCondition(!pr.allowConnection("192.168.1.30"), "Third connection from same subnet 192.168.1.30 should be blocked!");

    // Release connection and try again
    pr.releaseConnection("192.168.1.10");
    requireCondition(pr.allowConnection("192.168.1.30"), "Should allow connection after one is released");
}

} // namespace

int main() {
    try {
        testPeerScoresAndBan();
        testEclipsePrevention();
        std::cout << "Nodo peer reputation tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo peer reputation tests failed: " << error.what() << "\n";
        return 1;
    }
}
