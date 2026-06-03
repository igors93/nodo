// Tests for LocalNodeIdentity and LocalPeerTopology.
// Validates: node identity validation, duplicate endpoint rejection,
// duplicate node ID rejection, genesis mismatch detection.

#include "node/LocalNodeIdentity.hpp"
#include "node/LocalPeerTopology.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using nodo::node::LocalNodeIdentity;
using nodo::node::LocalPeerTopology;
using nodo::node::LocalTopologyAddStatus;

LocalNodeIdentity makeNode(
    const std::string& id,
    const std::string& endpoint,
    const std::string& genesis = "genesis-abc"
) {
    return LocalNodeIdentity(
        id,
        endpoint,
        "seed-" + id,
        std::filesystem::temp_directory_path() / ("nodo-topology-test-" + id),
        genesis
    );
}

// ---- LocalNodeIdentity validation ----

void testValidIdentityIsAccepted() {
    const auto n = makeNode("node-A", "127.0.0.1:9100");
    assert(n.isValid());
}

void testEmptyNodeIdIsInvalid() {
    const LocalNodeIdentity n("", "127.0.0.1:9100", "seed", "/tmp/dir", "genesis");
    assert(!n.isValid());
    assert(n.rejectionReason().find("nodeId") != std::string::npos);
}

void testEmptyEndpointIsInvalid() {
    const LocalNodeIdentity n("node-A", "", "seed", "/tmp/dir", "genesis");
    assert(!n.isValid());
    assert(n.rejectionReason().find("endpoint") != std::string::npos);
}

void testMalformedEndpointIsInvalid() {
    // No port.
    const LocalNodeIdentity n1("node-A", "127.0.0.1", "seed", "/tmp/dir", "genesis");
    assert(!n1.isValid());

    // Port out of range.
    const LocalNodeIdentity n2("node-A", "127.0.0.1:99999", "seed", "/tmp/dir", "genesis");
    assert(!n2.isValid());

    // Port zero.
    const LocalNodeIdentity n3("node-A", "127.0.0.1:0", "seed", "/tmp/dir", "genesis");
    assert(!n3.isValid());
}

void testEmptyDataDirectoryIsInvalid() {
    const LocalNodeIdentity n("node-A", "127.0.0.1:9100", "seed", "", "genesis");
    assert(!n.isValid());
    assert(n.rejectionReason().find("dataDirectory") != std::string::npos);
}

void testEmptyGenesisIdIsInvalid() {
    const LocalNodeIdentity n("node-A", "127.0.0.1:9100", "seed", "/tmp/dir", "");
    assert(!n.isValid());
    assert(n.rejectionReason().find("genesisId") != std::string::npos);
}

// ---- LocalPeerTopology ----

void testTopologyStartsEmpty() {
    LocalPeerTopology topo;
    assert(topo.empty());
    assert(topo.size() == 0);
    assert(!topo.hasGenesisId());
}

void testAddingValidNodeSucceeds() {
    LocalPeerTopology topo;
    const auto result = topo.addNode(makeNode("node-A", "127.0.0.1:9100"));
    assert(result.isAdded());
    assert(topo.size() == 1);
    assert(topo.hasNode("node-A"));
    assert(topo.hasGenesisId());
}

void testAddingMultipleNodesWithSameGenesisSuceeeds() {
    LocalPeerTopology topo;
    assert(topo.addNode(makeNode("node-A", "127.0.0.1:9100", "shared-genesis")).isAdded());
    assert(topo.addNode(makeNode("node-B", "127.0.0.1:9101", "shared-genesis")).isAdded());
    assert(topo.addNode(makeNode("node-C", "127.0.0.1:9102", "shared-genesis")).isAdded());
    assert(topo.size() == 3);
    assert(topo.genesisId() == "shared-genesis");
}

void testDuplicateNodeIdIsRejected() {
    LocalPeerTopology topo;
    assert(topo.addNode(makeNode("node-A", "127.0.0.1:9100")).isAdded());

    const auto result = topo.addNode(makeNode("node-A", "127.0.0.1:9200"));
    assert(!result.isAdded());
    assert(result.status() == LocalTopologyAddStatus::DUPLICATE_NODE_ID);
    assert(result.reason().find("node-A") != std::string::npos);
}

void testDuplicateEndpointIsRejected() {
    LocalPeerTopology topo;
    assert(topo.addNode(makeNode("node-A", "127.0.0.1:9100")).isAdded());

    const auto result = topo.addNode(makeNode("node-B", "127.0.0.1:9100"));
    assert(!result.isAdded());
    assert(result.status() == LocalTopologyAddStatus::DUPLICATE_ENDPOINT);
    assert(result.reason().find("9100") != std::string::npos);
}

void testGenesisMismatchIsRejected() {
    LocalPeerTopology topo;
    assert(topo.addNode(makeNode("node-A", "127.0.0.1:9100", "genesis-1")).isAdded());

    const auto result = topo.addNode(makeNode("node-B", "127.0.0.1:9101", "genesis-2"));
    assert(!result.isAdded());
    assert(result.status() == LocalTopologyAddStatus::GENESIS_MISMATCH);
    assert(result.reason().find("genesis-1") != std::string::npos);
    assert(result.reason().find("genesis-2") != std::string::npos);
}

void testInvalidIdentityIsRejected() {
    LocalPeerTopology topo;
    const LocalNodeIdentity invalid("", "127.0.0.1:9100", "seed", "/tmp/dir", "genesis");
    const auto result = topo.addNode(invalid);
    assert(!result.isAdded());
    assert(result.status() == LocalTopologyAddStatus::INVALID_IDENTITY);
}

void testTopologySerializesNonEmpty() {
    LocalPeerTopology topo;
    topo.addNode(makeNode("node-A", "127.0.0.1:9100", "shared"));
    topo.addNode(makeNode("node-B", "127.0.0.1:9101", "shared"));
    const auto s = topo.serialize();
    assert(!s.empty());
    assert(s.find("node-A") != std::string::npos);
    assert(s.find("node-B") != std::string::npos);
}

} // namespace

int main() {
    testValidIdentityIsAccepted();
    testEmptyNodeIdIsInvalid();
    testEmptyEndpointIsInvalid();
    testMalformedEndpointIsInvalid();
    testEmptyDataDirectoryIsInvalid();
    testEmptyGenesisIdIsInvalid();
    testTopologyStartsEmpty();
    testAddingValidNodeSucceeds();
    testAddingMultipleNodesWithSameGenesisSuceeeds();
    testDuplicateNodeIdIsRejected();
    testDuplicateEndpointIsRejected();
    testGenesisMismatchIsRejected();
    testInvalidIdentityIsRejected();
    testTopologySerializesNonEmpty();

    std::cout << "Local node topology tests passed.\n";
    return 0;
}
