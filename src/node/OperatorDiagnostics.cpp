#include "node/OperatorDiagnostics.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

OperatorDiagnosticsReport::OperatorDiagnosticsReport()
    : m_finalizedHeight(0), m_validatorCount(0), m_connectedPeers(0),
      m_inboundPeers(0), m_outboundPeers(0), m_discoveryActive(false),
      m_genesisVerified(false), m_genesisCompatible(false),
      m_keyPolicyPassed(false), m_readinessStatus("NOT_READY"),
      m_defenseRestrictionsActive(false) {}

OperatorDiagnosticsReport::OperatorDiagnosticsReport(
    std::string networkName, std::string chainId, std::string protocolVersion,
    std::string registeredGenesisId, std::string manifestGenesisId,
    std::string networkClass, std::uint64_t finalizedHeight,
    std::string latestFinalizedHash, std::size_t validatorCount,
    std::size_t connectedPeers, std::size_t inboundPeers,
    std::size_t outboundPeers, bool discoveryActive, bool genesisVerified,
    bool genesisCompatible, bool keyPolicyPassed, std::string readinessStatus,
    std::string latestImportStatus, std::string latestImportRejectionReason,
    bool defenseRestrictionsActive, std::vector<std::string> warnings,
    EvidenceCaptureHealth evidenceHealth)
    : m_networkName(std::move(networkName)), m_chainId(std::move(chainId)),
      m_protocolVersion(std::move(protocolVersion)),
      m_registeredGenesisId(std::move(registeredGenesisId)),
      m_manifestGenesisId(std::move(manifestGenesisId)),
      m_networkClass(std::move(networkClass)),
      m_finalizedHeight(finalizedHeight),
      m_latestFinalizedHash(std::move(latestFinalizedHash)),
      m_validatorCount(validatorCount), m_connectedPeers(connectedPeers),
      m_inboundPeers(inboundPeers), m_outboundPeers(outboundPeers),
      m_discoveryActive(discoveryActive), m_genesisVerified(genesisVerified),
      m_genesisCompatible(genesisCompatible),
      m_keyPolicyPassed(keyPolicyPassed),
      m_readinessStatus(std::move(readinessStatus)),
      m_latestImportStatus(std::move(latestImportStatus)),
      m_latestImportRejectionReason(std::move(latestImportRejectionReason)),
      m_defenseRestrictionsActive(defenseRestrictionsActive),
      m_warnings(std::move(warnings)),
      m_evidenceHealth(std::move(evidenceHealth)) {}

const std::string &OperatorDiagnosticsReport::networkName() const {
  return m_networkName;
}
const std::string &OperatorDiagnosticsReport::chainId() const {
  return m_chainId;
}
const std::string &OperatorDiagnosticsReport::protocolVersion() const {
  return m_protocolVersion;
}
const std::string &OperatorDiagnosticsReport::registeredGenesisId() const {
  return m_registeredGenesisId;
}
const std::string &OperatorDiagnosticsReport::manifestGenesisId() const {
  return m_manifestGenesisId;
}
const std::string &OperatorDiagnosticsReport::genesisId() const {
  return m_registeredGenesisId;
}
const std::string &OperatorDiagnosticsReport::networkClass() const {
  return m_networkClass;
}
std::uint64_t OperatorDiagnosticsReport::finalizedHeight() const {
  return m_finalizedHeight;
}
const std::string &OperatorDiagnosticsReport::latestFinalizedHash() const {
  return m_latestFinalizedHash;
}
std::size_t OperatorDiagnosticsReport::validatorCount() const {
  return m_validatorCount;
}
std::size_t OperatorDiagnosticsReport::connectedPeers() const {
  return m_connectedPeers;
}
std::size_t OperatorDiagnosticsReport::inboundPeers() const {
  return m_inboundPeers;
}
std::size_t OperatorDiagnosticsReport::outboundPeers() const {
  return m_outboundPeers;
}
bool OperatorDiagnosticsReport::discoveryActive() const {
  return m_discoveryActive;
}
bool OperatorDiagnosticsReport::genesisVerified() const {
  return m_genesisVerified;
}
bool OperatorDiagnosticsReport::genesisCompatible() const {
  return m_genesisCompatible;
}
bool OperatorDiagnosticsReport::keyPolicyPassed() const {
  return m_keyPolicyPassed;
}
const std::string &OperatorDiagnosticsReport::readinessStatus() const {
  return m_readinessStatus;
}
const std::string &OperatorDiagnosticsReport::latestImportStatus() const {
  return m_latestImportStatus;
}
const std::string &
OperatorDiagnosticsReport::latestImportRejectionReason() const {
  return m_latestImportRejectionReason;
}
bool OperatorDiagnosticsReport::defenseRestrictionsActive() const {
  return m_defenseRestrictionsActive;
}
const std::vector<std::string> &OperatorDiagnosticsReport::warnings() const {
  return m_warnings;
}
const EvidenceCaptureHealth &OperatorDiagnosticsReport::evidenceHealth() const {
  return m_evidenceHealth;
}

std::string OperatorDiagnosticsReport::serialize() const {
  std::ostringstream oss;
  oss << "OperatorDiagnostics{\n"
      << "  network=" << m_networkName << "\n"
      << "  networkClass=" << m_networkClass << "\n"
      << "  chainId=" << m_chainId << "\n"
      << "  protocol=" << m_protocolVersion << "\n"
      << "  registeredGenesisId="
      << (m_registeredGenesisId.empty() ? "(none)" : m_registeredGenesisId)
      << "\n"
      << "  manifestGenesisId="
      << (m_manifestGenesisId.empty() ? "(none)" : m_manifestGenesisId) << "\n"
      << "  genesisCompatible=" << (m_genesisCompatible ? "yes" : "no") << "\n"
      << "  finalizedHeight=" << m_finalizedHeight << "\n"
      << "  latestFinalizedHash="
      << (m_latestFinalizedHash.empty() ? "(none)" : m_latestFinalizedHash)
      << "\n"
      << "  validators=" << m_validatorCount << "\n"
      << "  peers=" << m_connectedPeers << "\n"
      << "  inboundPeers=" << m_inboundPeers << "\n"
      << "  outboundPeers=" << m_outboundPeers << "\n"
      << "  discoveryActive=" << (m_discoveryActive ? "yes" : "no") << "\n"
      << "  genesisVerified=" << (m_genesisVerified ? "yes" : "no") << "\n"
      << "  keyPolicyPassed=" << (m_keyPolicyPassed ? "yes" : "no") << "\n"
      << "  readiness=" << m_readinessStatus << "\n"
      << "  latestImportStatus="
      << (m_latestImportStatus.empty() ? "(none)" : m_latestImportStatus)
      << "\n"
      << "  latestImportRejectionReason="
      << (m_latestImportRejectionReason.empty() ? "(none)"
                                                : m_latestImportRejectionReason)
      << "\n"
      << "  defenseRestrictionsActive="
      << (m_defenseRestrictionsActive ? "yes" : "no") << "\n"
      << "  evidenceCapture=" << m_evidenceHealth.serialize() << "\n";
  for (const auto &w : m_warnings) {
    oss << "  WARNING: " << w << "\n";
  }
  oss << "}";
  return oss.str();
}

OperatorDiagnosticsReport OperatorDiagnostics::collect(
    const config::NetworkParameters &params,
    const std::string &registeredGenesisId,
    const std::string &manifestGenesisId, const std::string &networkClass,
    std::uint64_t finalizedHeight, const std::string &latestFinalizedHash,
    std::size_t validatorCount, std::size_t connectedPeers,
    std::size_t inboundPeers, std::size_t outboundPeers, bool discoveryActive,
    bool genesisVerified, bool genesisCompatible, bool keyPolicyPassed,
    const std::string &latestImportStatus,
    const std::string &latestImportRejectionReason,
    bool defenseRestrictionsActive, const std::vector<std::string> &warnings,
    EvidenceCaptureHealth evidenceHealth) {
  const bool ready = keyPolicyPassed && genesisVerified && genesisCompatible &&
                     connectedPeers > 0;

  return OperatorDiagnosticsReport(
      params.networkName(), params.chainId(), params.protocolVersion(),
      registeredGenesisId, manifestGenesisId, networkClass, finalizedHeight,
      latestFinalizedHash, validatorCount, connectedPeers, inboundPeers,
      outboundPeers, discoveryActive, genesisVerified, genesisCompatible,
      keyPolicyPassed, ready ? "READY" : "NOT_READY", latestImportStatus,
      latestImportRejectionReason, defenseRestrictionsActive, warnings,
      std::move(evidenceHealth));
}

} // namespace nodo::node
