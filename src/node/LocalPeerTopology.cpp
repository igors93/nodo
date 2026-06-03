#include "node/LocalPeerTopology.hpp"

#include <sstream>

namespace nodo::node {

std::string localTopologyAddStatusToString(LocalTopologyAddStatus status) {
    switch (status) {
        case LocalTopologyAddStatus::ADDED:              return "ADDED";
        case LocalTopologyAddStatus::DUPLICATE_NODE_ID:  return "DUPLICATE_NODE_ID";
        case LocalTopologyAddStatus::DUPLICATE_ENDPOINT: return "DUPLICATE_ENDPOINT";
        case LocalTopologyAddStatus::INVALID_IDENTITY:   return "INVALID_IDENTITY";
        case LocalTopologyAddStatus::GENESIS_MISMATCH:   return "GENESIS_MISMATCH";
        default:                                         return "INVALID_IDENTITY";
    }
}

LocalTopologyAddResult::LocalTopologyAddResult()
    : m_added(false),
      m_status(LocalTopologyAddStatus::INVALID_IDENTITY),
      m_reason("Uninitialized topology add result.") {}

LocalTopologyAddResult LocalTopologyAddResult::added() {
    LocalTopologyAddResult r;
    r.m_added = true;
    r.m_status = LocalTopologyAddStatus::ADDED;
    r.m_reason.clear();
    return r;
}

LocalTopologyAddResult LocalTopologyAddResult::rejected(
    LocalTopologyAddStatus status, std::string reason
) {
    LocalTopologyAddResult r;
    r.m_added = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool LocalTopologyAddResult::isAdded() const { return m_added; }
LocalTopologyAddStatus LocalTopologyAddResult::status() const { return m_status; }
const std::string& LocalTopologyAddResult::reason() const { return m_reason; }

// ----

LocalPeerTopology::LocalPeerTopology()
    : m_nodes(), m_genesisId() {}

LocalTopologyAddResult LocalPeerTopology::addNode(const LocalNodeIdentity& identity) {
    if (!identity.isValid()) {
        return LocalTopologyAddResult::rejected(
            LocalTopologyAddStatus::INVALID_IDENTITY,
            "node identity is not valid: " + identity.rejectionReason()
        );
    }

    if (hasNode(identity.nodeId())) {
        return LocalTopologyAddResult::rejected(
            LocalTopologyAddStatus::DUPLICATE_NODE_ID,
            "node id '" + identity.nodeId() + "' is already in the topology"
        );
    }

    if (hasEndpoint(identity.endpoint())) {
        return LocalTopologyAddResult::rejected(
            LocalTopologyAddStatus::DUPLICATE_ENDPOINT,
            "endpoint '" + identity.endpoint() + "' is already in use"
        );
    }

    if (!m_genesisId.empty() && m_genesisId != identity.genesisId()) {
        return LocalTopologyAddResult::rejected(
            LocalTopologyAddStatus::GENESIS_MISMATCH,
            "node '" + identity.nodeId() + "' has genesis '" + identity.genesisId() +
            "' but topology has genesis '" + m_genesisId + "'"
        );
    }

    if (m_genesisId.empty()) {
        m_genesisId = identity.genesisId();
    }

    m_nodes.push_back(identity);
    return LocalTopologyAddResult::added();
}

bool LocalPeerTopology::hasNode(const std::string& nodeId) const {
    for (const auto& n : m_nodes) {
        if (n.nodeId() == nodeId) return true;
    }
    return false;
}

bool LocalPeerTopology::hasEndpoint(const std::string& endpoint) const {
    for (const auto& n : m_nodes) {
        if (n.endpoint() == endpoint) return true;
    }
    return false;
}

bool LocalPeerTopology::hasGenesisId() const { return !m_genesisId.empty(); }
const std::string& LocalPeerTopology::genesisId() const { return m_genesisId; }
const std::vector<LocalNodeIdentity>& LocalPeerTopology::nodes() const { return m_nodes; }
std::size_t LocalPeerTopology::size() const { return m_nodes.size(); }
bool LocalPeerTopology::empty() const { return m_nodes.empty(); }

std::string LocalPeerTopology::serialize() const {
    std::ostringstream oss;
    oss << "LocalPeerTopology{"
        << "genesisId=" << m_genesisId
        << ";nodeCount=" << m_nodes.size()
        << ";nodes=[";
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << m_nodes[i].nodeId() << "@" << m_nodes[i].endpoint();
    }
    oss << "]}";
    return oss.str();
}

} // namespace nodo::node
