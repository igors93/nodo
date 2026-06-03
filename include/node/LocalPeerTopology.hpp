#ifndef NODO_NODE_LOCAL_PEER_TOPOLOGY_HPP
#define NODO_NODE_LOCAL_PEER_TOPOLOGY_HPP

#include "node/LocalNodeIdentity.hpp"

#include <string>
#include <vector>

namespace nodo::node {

enum class LocalTopologyAddStatus {
    ADDED,
    DUPLICATE_NODE_ID,
    DUPLICATE_ENDPOINT,
    INVALID_IDENTITY,
    GENESIS_MISMATCH
};

std::string localTopologyAddStatusToString(LocalTopologyAddStatus status);

class LocalTopologyAddResult {
public:
    LocalTopologyAddResult();

    static LocalTopologyAddResult added();
    static LocalTopologyAddResult rejected(LocalTopologyAddStatus status, std::string reason);

    bool isAdded() const;
    LocalTopologyAddStatus status() const;
    const std::string& reason() const;

private:
    bool m_added;
    LocalTopologyAddStatus m_status;
    std::string m_reason;
};

/*
 * LocalPeerTopology manages a set of local node identities for a local
 * testnet. It enforces uniqueness of node identifiers and endpoints, and
 * ensures all nodes share the same genesis.
 *
 * Security principle:
 * Duplicate node identifiers and duplicate endpoints are rejected at add
 * time. Genesis mismatch is detected as soon as a second node is added
 * with a different genesis identifier.
 */
class LocalPeerTopology {
public:
    LocalPeerTopology();

    LocalTopologyAddResult addNode(const LocalNodeIdentity& identity);

    bool hasNode(const std::string& nodeId) const;
    bool hasEndpoint(const std::string& endpoint) const;
    bool hasGenesisId() const;
    const std::string& genesisId() const;

    const std::vector<LocalNodeIdentity>& nodes() const;
    std::size_t size() const;
    bool empty() const;

    std::string serialize() const;

private:
    std::vector<LocalNodeIdentity> m_nodes;
    std::string m_genesisId;
};

} // namespace nodo::node

#endif
