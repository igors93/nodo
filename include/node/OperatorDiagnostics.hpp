#ifndef NODO_NODE_OPERATOR_DIAGNOSTICS_HPP
#define NODO_NODE_OPERATOR_DIAGNOSTICS_HPP

#include "config/NetworkParameters.hpp"
#include "node/EvidenceCaptureHealth.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class OperatorDiagnosticsReport {
public:
    OperatorDiagnosticsReport();

    OperatorDiagnosticsReport(
        std::string networkName,
        std::string chainId,
        std::string protocolVersion,
        std::uint64_t finalizedHeight,
        std::size_t validatorCount,
        std::size_t connectedPeers,
        bool genesisVerified,
        bool keyPolicyPassed,
        std::string readinessStatus,
        std::vector<std::string> warnings,
        EvidenceCaptureHealth evidenceHealth
    );

    const std::string& networkName() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    std::uint64_t finalizedHeight() const;
    std::size_t validatorCount() const;
    std::size_t connectedPeers() const;
    bool genesisVerified() const;
    bool keyPolicyPassed() const;
    const std::string& readinessStatus() const;
    const std::vector<std::string>& warnings() const;
    const EvidenceCaptureHealth& evidenceHealth() const;

    std::string serialize() const;

private:
    std::string m_networkName;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::uint64_t m_finalizedHeight;
    std::size_t m_validatorCount;
    std::size_t m_connectedPeers;
    bool m_genesisVerified;
    bool m_keyPolicyPassed;
    std::string m_readinessStatus;
    std::vector<std::string> m_warnings;
    EvidenceCaptureHealth m_evidenceHealth;
};

/*
 * OperatorDiagnostics collects a human-readable runtime summary for node
 * operators and monitoring systems.
 */
class OperatorDiagnostics {
public:
    static OperatorDiagnosticsReport collect(
        const config::NetworkParameters& params,
        std::uint64_t finalizedHeight,
        std::size_t validatorCount,
        std::size_t connectedPeers,
        bool genesisVerified,
        bool keyPolicyPassed,
        const std::vector<std::string>& warnings,
        EvidenceCaptureHealth evidenceHealth = EvidenceCaptureHealth()
    );
};

} // namespace nodo::node

#endif
