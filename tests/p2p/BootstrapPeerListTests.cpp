#include "p2p/BootstrapPeerList.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

void testValidPeerIsAccepted() {
    const nodo::p2p::PeerEndpoint ep("127.0.0.1", 9000);
    assert(nodo::p2p::BootstrapPeerList::isValidPeer(ep));
}

void testInvalidPeerRejected() {
    const nodo::p2p::PeerEndpoint ep("", 0);
    assert(!nodo::p2p::BootstrapPeerList::isValidPeer(ep));
}

void testValidateAllPasses() {
    std::vector<nodo::p2p::PeerEndpoint> peers = {
        nodo::p2p::PeerEndpoint("192.168.1.1", 9000),
        nodo::p2p::PeerEndpoint("10.0.0.1", 9001)
    };
    std::string reason;
    assert(nodo::p2p::BootstrapPeerList::validateAll(peers, reason));
    assert(reason.empty());
}

void testValidateAllEmptyListFails() {
    std::string reason;
    assert(!nodo::p2p::BootstrapPeerList::validateAll({}, reason));
    assert(!reason.empty());
}

void testParseFromLines() {
    const std::vector<std::string> lines = {
        "# comment line",
        "127.0.0.1:9000",
        "192.168.1.2:9001",
        "invalid-no-port",
        ""
    };
    const auto peers = nodo::p2p::BootstrapPeerList::parseFromLines(lines);
    assert(peers.size() == 2);
    assert(peers[0].host() == "127.0.0.1");
    assert(peers[0].port() == 9000);
    assert(peers[1].host() == "192.168.1.2");
    assert(peers[1].port() == 9001);
}

void testParseEmptyLines() {
    const auto peers = nodo::p2p::BootstrapPeerList::parseFromLines({});
    assert(peers.empty());
}

void testParseInvalidPortIgnored() {
    const auto peers = nodo::p2p::BootstrapPeerList::parseFromLines({"host:notaport"});
    assert(peers.empty());
}

void testParsePortWithTrailingTextIgnored() {
    const auto peers = nodo::p2p::BootstrapPeerList::parseFromLines({"host:9000abc"});
    assert(peers.empty());
}

} // namespace

int main() {
    testValidPeerIsAccepted();
    testInvalidPeerRejected();
    testValidateAllPasses();
    testValidateAllEmptyListFails();
    testParseFromLines();
    testParseEmptyLines();
    testParseInvalidPortIgnored();
    testParsePortWithTrailingTextIgnored();
    return 0;
}
