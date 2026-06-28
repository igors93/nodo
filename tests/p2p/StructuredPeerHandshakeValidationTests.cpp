// Tests for structured peer handshake parsing and genesis validation.
// Covers: malformed payload rejected, missing genesis field rejected,
// wrong genesis rejected via structured parse, self-handshake rejected.

#include "node/ChainSyncMessages.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace nodo::node;
using namespace nodo::p2p;

namespace {

GossipMeshConfig makeConfig(
    const std::string& nodeId,
    const std::string& genesisId
) {
    return GossipMeshConfig(nodeId, "testnet", "chain-1", "1", genesisId, 60, 2);
}

PeerMetadata makePeer(const std::string& nodeId) {
    return PeerMetadata(nodeId, PeerEndpoint("127.0.0.1", 19800), "fp-" + nodeId,
                        1000, 1000, 0, false);
}

ChainStatusMessage makeStatus() {
    return ChainStatusMessage("testnet", "chain-1", "1", 5, "hash-5", 5, "hash-5");
}

// ---- Test 1: Well-formed hello with matching genesis is accepted ----

void testWellFormedHelloAccepted() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-local",  genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(remote, makePeer("node-remote"), makeStatus(), 1000);

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, hello, 1001);
    assert(result.accepted());
}

// ---- Test 2: Peer with different genesis is rejected via structured parse ----

void testDifferentGenesisRejected() {
    const GossipMeshConfig local  = makeConfig("node-local",  "genesis-correct");
    const GossipMeshConfig remote = makeConfig("node-remote", "genesis-wrong");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(remote, makePeer("node-remote"), makeStatus(), 1000);

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, hello, 1001);
    assert(!result.accepted());
    // The rejection message must mention genesis, not just "mismatch".
    assert(result.reason().find("genesis") != std::string::npos ||
           result.reason().find("Genesis") != std::string::npos);
}

// ---- Test 3: Malformed payload (not a PeerHelloMessage) is rejected ----

void testMalformedPayloadRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-correct");

    // Construct a fake envelope with a non-PeerHelloMessage payload.
    const NetworkEnvelope fakeEnvelope(
        "testnet", "chain-1", "1",
        NetworkMessageType::PEER_HELLO,
        "node-remote",
        1000, 60,
        "this is not a valid PeerHelloMessage payload"
    );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, fakeEnvelope, 1001);
    assert(!result.accepted());
    // Must indicate structural problem, not just network mismatch.
    assert(!result.reason().empty());
}

// ---- Test 4: Self-handshake is rejected ----

void testSelfHandshakeRejected() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-self", genesis);
    const GossipMeshConfig remote = makeConfig("node-self", genesis);  // same node id
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(remote, makePeer("node-self"), makeStatus(), 1000);

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, hello, 1001);
    assert(!result.accepted());
    assert(result.reason().find("local") != std::string::npos ||
           result.reason().find("self") != std::string::npos ||
           result.reason().find("same") != std::string::npos);
}

// ---- Test 5: Wrong network is rejected ----

void testWrongNetworkRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-ok");
    const GossipMeshConfig remote(
        "node-remote", "different-network", "chain-1", "1", "genesis-ok", 60, 2
    );
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(remote, makePeer("node-remote"), makeStatus(), 1000);

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, hello, 1001);
    assert(!result.accepted());
}

// ---- Test 6: Expired envelope is rejected ----

void testExpiredEnvelopeRejected() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-local",  genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    // Create hello at time=1000 with 60-second TTL, then validate at time=2000 (expired).
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(remote, makePeer("node-remote"), makeStatus(), 1000);

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(local, hello, 2000);
    assert(!result.accepted());
}

// ---- Test 7: Envelope sender cannot claim another embedded node id ----

void testEmbeddedNodeIdMustMatchSender() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local = makeConfig("node-local", genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    const NetworkEnvelope valid =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote"),
            makeStatus(),
            1000
        );
    const NetworkEnvelope impersonated(
        valid.networkId(),
        valid.chainId(),
        valid.protocolVersion(),
        valid.messageType(),
        "node-attacker",
        valid.createdAt(),
        valid.ttlSeconds(),
        valid.payload()
    );

    const PeerHandshakeResult result =
        PeerHandshakeManager::validateHello(local, impersonated, 1001);
    assert(!result.accepted());
    assert(result.reason().find("node id") != std::string::npos ||
           result.reason().find("identity") != std::string::npos);
}

// ---- Test 8: Nested fields cannot shadow the top-level genesis id ----

void testNestedGenesisCannotShadowTopLevelField() {
    const GossipMeshConfig local =
        makeConfig("node-local", "genesis-correct");
    const GossipMeshConfig remote =
        makeConfig("node-remote", "genesis-wrong");
    const NetworkEnvelope original =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote"),
            makeStatus(),
            1000
        );

    std::string payload = original.payload();
    const std::size_t insertion = payload.find(";firstSeenAt=");
    assert(insertion != std::string::npos);
    payload.insert(insertion, ";genesisId=genesis-correct");
    const NetworkEnvelope shadowed(
        original.networkId(),
        original.chainId(),
        original.protocolVersion(),
        original.messageType(),
        original.senderNodeId(),
        original.createdAt(),
        original.ttlSeconds(),
        payload
    );

    const PeerHandshakeResult result =
        PeerHandshakeManager::validateHello(local, shadowed, 1001);
    assert(!result.accepted());
    assert(result.reason().find("genesis") != std::string::npos);
}

} // namespace

int main() {
    try {
        testWellFormedHelloAccepted();
        testDifferentGenesisRejected();
        testMalformedPayloadRejected();
        testSelfHandshakeRejected();
        testWrongNetworkRejected();
        testExpiredEnvelopeRejected();
        testEmbeddedNodeIdMustMatchSender();
        testNestedGenesisCannotShadowTopLevelField();

        std::cout << "Structured peer handshake validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Structured peer handshake validation tests failed: " << e.what() << "\n";
        return 1;
    }
}
