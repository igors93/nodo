#include "node/OperatorDiagnostics.hpp"

#include <sstream>

namespace nodo::node {

OperatorDiagnosticsReport::OperatorDiagnosticsReport()
    : m_finalizedHeight(0),
      m_validatorCount(0),
      m_connectedPeers(0),
      m_genesisVerified(false),
      m_keyPolicyPassed(false),
      m_readinessStatus("NOT_READY") {}

OperatorDiagnosticsReport::OperatorDiagnosticsReport(
    std::string networkName,
    std::string chainId,
    std::string protocolVersion,
    std::uint64_t finalizedHeight,
    std::size_t validatorCount,
    std::size_t connectedPeers,
    bool genesisVerified,
    bool keyPolicyPassed,
    std::string readinessStatus,
    std::vector<std::string> warnings
)
    : m_networkName(std::move(networkName)),
      m_chainId(std::move(chainId)),
      m_protocolVersion(std::move(protocolVersion)),
      m_finalizedHeight(finalizedHeight),
      m_validatorCount(validatorCount),
      m_connectedPeers(connectedPeers),
      m_genesisVerified(genesisVerified),
      m_keyPolicyPassed(keyPolicyPassed),
      m_readinessStatus(std::move(readinessStatus)),
      m_warnings(std::move(warnings)) {}

const std::string& OperatorDiagnosticsReport::networkName() const { return m_networkName; }
const std::string& OperatorDiagnosticsReport::chainId() const { return m_chainId; }
const std::string& OperatorDiagnosticsReport::protocolVersion() const { return m_protocolVersion; }
std::uint64_t OperatorDiagnosticsReport::finalizedHeight() const { return m_finalizedHeight; }
std::size_t OperatorDiagnosticsReport::validatorCount() const { return m_validatorCount; }
std::size_t OperatorDiagnosticsReport::connectedPeers() const { return m_connectedPeers; }
bool OperatorDiagnosticsReport::genesisVerified() const { return m_genesisVerified; }
bool OperatorDiagnosticsReport::keyPolicyPassed() const { return m_keyPolicyPassed; }
const std::string& OperatorDiagnosticsReport::readinessStatus() const { return m_readinessStatus; }
const std::vector<std::string>& OperatorDiagnosticsReport::warnings() const { return m_warnings; }

std::string OperatorDiagnosticsReport::serialize() const {
    std::ostringstream oss;
    oss << "OperatorDiagnostics{\n"
        << "  network=" << m_networkName << "\n"
        << "  chainId=" << m_chainId << "\n"
        << "  protocol=" << m_protocolVersion << "\n"
        << "  finalizedHeight=" << m_finalizedHeight << "\n"
        << "  validators=" << m_validatorCount << "\n"
        << "  peers=" << m_connectedPeers << "\n"
        << "  genesisVerified=" << (m_genesisVerified ? "yes" : "no") << "\n"
        << "  keyPolicyPassed=" << (m_keyPolicyPassed ? "yes" : "no") << "\n"
        << "  readiness=" << m_readinessStatus << "\n";
    for (const auto& w : m_warnings) {
        oss << "  WARNING: " << w << "\n";
    }
    oss << "}";
    return oss.str();
}

OperatorDiagnosticsReport OperatorDiagnostics::collect(
    const config::NetworkParameters& params,
    std::uint64_t finalizedHeight,
    std::size_t validatorCount,
    std::size_t connectedPeers,
    bool genesisVerified,
    bool keyPolicyPassed,
    const std::vector<std::string>& warnings
) {
    return OperatorDiagnosticsReport(
        params.networkName(),
        params.chainId(),
        params.protocolVersion(),
        finalizedHeight,
        validatorCount,
        connectedPeers,
        genesisVerified,
        keyPolicyPassed,
        keyPolicyPassed && genesisVerified && connectedPeers > 0 ? "READY" : "NOT_READY",
        warnings
    );
}

} // namespace nodo::node
