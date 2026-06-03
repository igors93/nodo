#include "node/LocalNodeIdentity.hpp"

#include <sstream>

namespace nodo::node {

namespace {

bool isValidEndpoint(const std::string& endpoint) {
    // Expect "host:port" format where port is a positive integer.
    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon == endpoint.size() - 1) {
        return false;
    }
    const std::string portStr = endpoint.substr(colon + 1);
    for (char c : portStr) {
        if (c < '0' || c > '9') return false;
    }
    if (portStr.empty() || portStr.size() > 5) return false;
    const int port = std::stoi(portStr);
    return port > 0 && port <= 65535;
}

} // namespace

LocalNodeIdentity::LocalNodeIdentity()
    : m_nodeId(),
      m_endpoint(),
      m_validatorKeySeed(),
      m_dataDirectory(),
      m_genesisId() {}

LocalNodeIdentity::LocalNodeIdentity(
    std::string nodeId,
    std::string endpoint,
    std::string validatorKeySeed,
    std::filesystem::path dataDirectory,
    std::string genesisId
)
    : m_nodeId(std::move(nodeId)),
      m_endpoint(std::move(endpoint)),
      m_validatorKeySeed(std::move(validatorKeySeed)),
      m_dataDirectory(std::move(dataDirectory)),
      m_genesisId(std::move(genesisId)) {}

const std::string& LocalNodeIdentity::nodeId() const { return m_nodeId; }
const std::string& LocalNodeIdentity::endpoint() const { return m_endpoint; }
const std::string& LocalNodeIdentity::validatorKeySeed() const { return m_validatorKeySeed; }
const std::filesystem::path& LocalNodeIdentity::dataDirectory() const { return m_dataDirectory; }
const std::string& LocalNodeIdentity::genesisId() const { return m_genesisId; }

bool LocalNodeIdentity::isValid() const {
    if (!m_validated) validate();
    return m_valid;
}

const std::string& LocalNodeIdentity::rejectionReason() const {
    if (!m_validated) validate();
    return m_rejectionReason;
}

void LocalNodeIdentity::validate() const {
    m_validated = true;

    if (m_nodeId.empty()) {
        m_valid = false;
        m_rejectionReason = "nodeId must not be empty";
        return;
    }

    if (m_endpoint.empty()) {
        m_valid = false;
        m_rejectionReason = "endpoint must not be empty";
        return;
    }

    if (!isValidEndpoint(m_endpoint)) {
        m_valid = false;
        m_rejectionReason = "endpoint '" + m_endpoint + "' is not a valid host:port";
        return;
    }

    if (m_dataDirectory.empty()) {
        m_valid = false;
        m_rejectionReason = "dataDirectory must not be empty";
        return;
    }

    if (m_genesisId.empty()) {
        m_valid = false;
        m_rejectionReason = "genesisId must not be empty";
        return;
    }

    m_valid = true;
    m_rejectionReason.clear();
}

std::string LocalNodeIdentity::serialize() const {
    std::ostringstream oss;
    oss << "LocalNodeIdentity{"
        << "nodeId=" << m_nodeId
        << ";endpoint=" << m_endpoint
        << ";dataDirectory=" << m_dataDirectory.string()
        << ";genesisId=" << m_genesisId
        << ";valid=" << (isValid() ? "true" : "false")
        << "}";
    return oss.str();
}

} // namespace nodo::node
