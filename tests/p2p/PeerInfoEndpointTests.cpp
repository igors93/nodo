#include "p2p/PeerMessage.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::p2p::PeerInfo;

constexpr std::int64_t kTs = 1700000000;

PeerInfo makePeer(const std::string& endpoint) {
    return PeerInfo("peer-id", endpoint, "nodo/0.1", 0, kTs);
}

void testValidIpv4Endpoint() {
    assert(makePeer("127.0.0.1:9000").isValid());
}

void testValidLocalhostEndpoint() {
    assert(makePeer("localhost:9000").isValid());
}

void testMissingPort() {
    assert(!makePeer("127.0.0.1").isValid());
}

void testEmptyHost() {
    assert(!makePeer(":9000").isValid());
}

void testNonNumericPort() {
    assert(!makePeer("localhost:abc").isValid());
}

void testPortTooLarge() {
    assert(!makePeer("localhost:70000").isValid());
}

void testPortZeroInvalid() {
    assert(!makePeer("localhost:0").isValid());
}

void testPortAtBoundaryValid() {
    assert(makePeer("localhost:1").isValid());
    assert(makePeer("localhost:65535").isValid());
}

void testEmptyEndpoint() {
    assert(!makePeer("").isValid());
}

} // namespace

int main() {
    testValidIpv4Endpoint();
    testValidLocalhostEndpoint();
    testMissingPort();
    testEmptyHost();
    testNonNumericPort();
    testPortTooLarge();
    testPortZeroInvalid();
    testPortAtBoundaryValid();
    testEmptyEndpoint();
    return 0;
}
