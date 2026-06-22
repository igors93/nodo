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
        std::string registeredGenesisId,
        std::string manifestGenesisId,
        std::string networkClass,
        std::uint64_t finalizedHeight,
        std::string latestFinalizedHash,
        std::size_t validatorCount,
        std::size_t connectedPeers,
        bool genesisVerified,
        bool genesisCompatible,
        bool keyPolicyPassed,
        std::string readinessStatus,
        std::vector<std::string> warnings,
        EvidenceCaptureHealth evidenceHealth
    );

    const std::string& networkName() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    // Registered genesis id from GenesisRegistry for the selected network.
    const std::string& registeredGenesisId() const;
    // Genesis id stored in the data directory manifest.
    const std::string& manifestGenesisId() const;
    const std::string& networkClass() const;
    std::uint64_t finalizedHeight() const;
    const std::string& latestFinalizedHash() const;
    std::size_t validatorCount() const;
    std::size_t connectedPeers() const;
    bool genesisVerified() const;
    bool genesisCompatible() const;
    bool keyPolicyPassed() const;
    const std::string& readinessStatus() const;
    const std::vector<std::string>& warnings() const;
    const EvidenceCaptureHealth& evidenceHealth() const;

    // genesisId() returns registeredGenesisId() for backward compatibility.
    const std::string& genesisId() const;

    std::string serialize() const;

private:
    std::string m_networkName;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::string m_registeredGenesisId;
    std::string m_manifestGenesisId;
    std::string m_networkClass;
    std::uint64_t m_finalizedHeight;
    std::string m_latestFinalizedHash;
    std::size_t m_validatorCount;
    std::size_t m_connectedPeers;
    bool m_genesisVerified;
    bool m_genesisCompatible;
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
        const std::string& registeredGenesisId,
        const std::string& manifestGenesisId,
        const std::string& networkClass,
        std::uint64_t finalizedHeight,
        const std::string& latestFinalizedHash,
        std::size_t validatorCount,
        std::size_t connectedPeers,
        bool genesisVerified,
        bool genesisCompatible,
        bool keyPolicyPassed,
        const std::vector<std::string>& warnings,
        EvidenceCaptureHealth evidenceHealth = EvidenceCaptureHealth()
    );
};

} // namespace nodo::node

#endif
