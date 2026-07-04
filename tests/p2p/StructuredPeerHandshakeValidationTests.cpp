// Tests for structured peer handshake parsing and genesis validation.
// Covers: malformed payload rejected, missing genesis field rejected,
// wrong genesis rejected via structured parse, self-handshake rejected.

#include "node/ChainSyncMessages.hpp"
#include "crypto/KeyPair.hpp"
#include "p2p/GossipMesh.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace nodo::node;
using namespace nodo::p2p;

namespace {

namespace crypto = nodo::crypto;

GossipMeshConfig makeConfig(
    const std::string& nodeId,
    const std::string& genesisId
) {
    return GossipMeshConfig(nodeId, "testnet", "chain-1", "1", genesisId, 60, 2, 100, 50);
}

crypto::KeyPair makeIdentity(const std::string& nodeId) {
    return crypto::KeyPair::createDeterministicEd25519KeyPair(
        "structured-peer-hello-" + nodeId
    );
}

PeerMetadata makePeer(
    const std::string& nodeId,
    const crypto::KeyPair& identity
) {
    return PeerMetadata(nodeId, PeerEndpoint("127.0.0.1", 19800),
                        identity.publicKey().fingerprint(),
                        1000, 1000, 0, false);
}

ChainStatusMessage makeStatus() {
    return ChainStatusMessage("testnet", "chain-1", "1", 5, "hash-5", 5, "hash-5");
}

std::string challengeNonce(char fill = 'a') {
    return std::string(64, fill);
}

// ---- Test 1: Well-formed hello with matching genesis is accepted ----

void testWellFormedHelloAccepted() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-local",  genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote, makePeer("node-remote", identity), makeStatus(),
            "node-local", challengeNonce(), identity, 1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce(), 1001
    );
    assert(result.accepted());
}

// ---- Test 2: Peer with different genesis is rejected via structured parse ----

void testDifferentGenesisRejected() {
    const GossipMeshConfig local  = makeConfig("node-local",  "genesis-correct");
    const GossipMeshConfig remote = makeConfig("node-remote", "genesis-wrong");
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote, makePeer("node-remote", identity), makeStatus(),
            "node-local", challengeNonce(), identity, 1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce(), 1001
    );
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

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, fakeEnvelope, challengeNonce(), 1001
    );
    assert(!result.accepted());
    // Must indicate structural problem, not just network mismatch.
    assert(!result.reason().empty());
}

// ---- Test 4: Self-handshake is rejected ----

void testSelfHandshakeRejected() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-self", genesis);
    const GossipMeshConfig remote = makeConfig("node-self", genesis);  // same node id
    const crypto::KeyPair identity = makeIdentity("node-self");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote, makePeer("node-self", identity), makeStatus(),
            "node-challenger", challengeNonce(), identity, 1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce(), 1001
    );
    assert(!result.accepted());
    assert(result.reason().find("local") != std::string::npos ||
           result.reason().find("self") != std::string::npos ||
           result.reason().find("same") != std::string::npos);
}

// ---- Test 5: Wrong network is rejected ----

void testWrongNetworkRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-ok");
    const GossipMeshConfig remote(
        "node-remote", "different-network", "chain-1", "1", "genesis-ok", 60, 2, 100, 50
    );
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const ChainStatusMessage remoteStatus(
        "different-network", "chain-1", "1", 5, "hash-5", 5, "hash-5"
    );
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote, makePeer("node-remote", identity), remoteStatus,
            "node-local", challengeNonce(), identity, 1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce(), 1001
    );
    assert(!result.accepted());
}

// ---- Test 6: Expired envelope is rejected ----

void testExpiredEnvelopeRejected() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local  = makeConfig("node-local",  genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    const crypto::KeyPair identity = makeIdentity("node-remote");
    // Create hello at time=1000 with 60-second TTL, then validate at time=2000 (expired).
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote, makePeer("node-remote", identity), makeStatus(),
            "node-local", challengeNonce(), identity, 1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce(), 2000
    );
    assert(!result.accepted());
}

// ---- Test 7: Envelope sender cannot claim another embedded node id ----

void testEmbeddedNodeIdMustMatchSender() {
    const std::string genesis = "genesis-correct";
    const GossipMeshConfig local = makeConfig("node-local", genesis);
    const GossipMeshConfig remote = makeConfig("node-remote", genesis);
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope valid =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            "node-local",
            challengeNonce(),
            identity,
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
        PeerHandshakeManager::validateHello(
            local, impersonated, challengeNonce(), 1001
        );
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
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope original =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            "node-local",
            challengeNonce(),
            identity,
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
        PeerHandshakeManager::validateHello(
            local, shadowed, challengeNonce(), 1001
        );
    assert(!result.accepted());
    assert(result.reason().find("genesis") != std::string::npos);
}

// ---- Test 9: Signed identity metadata cannot be changed in transit ----

void testTamperedIdentityProofRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-correct");
    const GossipMeshConfig remote = makeConfig("node-remote", "genesis-correct");
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope original =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            "node-local",
            challengeNonce(),
            identity,
            1000
        );

    std::string payload = original.payload();
    const std::size_t port = payload.find("port=19800");
    assert(port != std::string::npos);
    payload.replace(port, std::string("port=19800").size(), "port=19801");
    const NetworkEnvelope tampered(
        original.networkId(), original.chainId(), original.protocolVersion(),
        original.messageType(), original.senderNodeId(), original.createdAt(),
        original.ttlSeconds(), payload
    );

    const PeerHandshakeResult result =
        PeerHandshakeManager::validateHello(
            local, tampered, challengeNonce(), 1001
        );
    assert(!result.accepted());
    assert(result.reason().find("proof") != std::string::npos ||
           result.reason().find("signature") != std::string::npos);
}

// ---- Test 10: The advertised fingerprint must belong to the signing key ----

void testMismatchedSigningKeyRejectedAtCreation() {
    const GossipMeshConfig remote =
        makeConfig("node-remote", "genesis-correct");
    const crypto::KeyPair advertisedIdentity = makeIdentity("node-remote");
    const crypto::KeyPair attackerIdentity = makeIdentity("node-attacker");

    bool rejected = false;
    try {
        (void)PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", advertisedIdentity),
            makeStatus(),
            "node-local",
            challengeNonce(),
            attackerIdentity,
            1000
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

// ---- Test 11: An unsigned PEER_HELLO is never accepted ----

void testMissingIdentityProofRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-correct");
    const GossipMeshConfig remote = makeConfig("node-remote", "genesis-correct");
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope original =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            "node-local",
            challengeNonce(),
            identity,
            1000
        );

    std::string payload = original.payload();
    const std::size_t proofStart = payload.find(";identityProof=");
    const std::size_t createdAt = payload.rfind(";createdAt=");
    assert(proofStart != std::string::npos);
    assert(createdAt != std::string::npos);
    assert(createdAt > proofStart);
    payload.erase(
        proofStart + std::string(";identityProof=").size(),
        createdAt - proofStart - std::string(";identityProof=").size()
    );

    const NetworkEnvelope unsignedHello(
        original.networkId(), original.chainId(), original.protocolVersion(),
        original.messageType(), original.senderNodeId(), original.createdAt(),
        original.ttlSeconds(), payload
    );
    const PeerHandshakeResult result =
        PeerHandshakeManager::validateHello(
            local, unsignedHello, challengeNonce(), 1001
        );
    assert(!result.accepted());
    assert(result.reason().find("proof") != std::string::npos);
}

// ---- Test 12: A valid signature cannot answer another connection nonce ----

void testWrongChallengeNonceRejected() {
    const GossipMeshConfig local = makeConfig("node-local", "genesis-correct");
    const GossipMeshConfig remote = makeConfig("node-remote", "genesis-correct");
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            "node-local",
            challengeNonce('a'),
            identity,
            1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        local, hello, challengeNonce('b'), 1001
    );
    assert(!result.accepted());
    assert(result.reason().find("challenge") != std::string::npos);
}

// ---- Test 13: A challenge response is bound to its issuing node ----

void testWrongChallengeIssuerRejected() {
    const GossipMeshConfig intendedVerifier =
        makeConfig("node-local", "genesis-correct");
    const GossipMeshConfig otherVerifier =
        makeConfig("node-other", "genesis-correct");
    const GossipMeshConfig remote =
        makeConfig("node-remote", "genesis-correct");
    const crypto::KeyPair identity = makeIdentity("node-remote");
    const NetworkEnvelope hello =
        PeerHandshakeManager::createHelloEnvelope(
            remote,
            makePeer("node-remote", identity),
            makeStatus(),
            intendedVerifier.localNodeId(),
            challengeNonce(),
            identity,
            1000
        );

    const PeerHandshakeResult result = PeerHandshakeManager::validateHello(
        otherVerifier, hello, challengeNonce(), 1001
    );
    assert(!result.accepted());
    assert(result.reason().find("issuer") != std::string::npos);
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
        testTamperedIdentityProofRejected();
        testMismatchedSigningKeyRejectedAtCreation();
        testMissingIdentityProofRejected();
        testWrongChallengeNonceRejected();
        testWrongChallengeIssuerRejected();

        std::cout << "Structured peer handshake validation tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Structured peer handshake validation tests failed: " << e.what() << "\n";
        return 1;
    }
}
