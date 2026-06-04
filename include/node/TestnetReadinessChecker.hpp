#ifndef NODO_NODE_TESTNET_READINESS_CHECKER_HPP
#define NODO_NODE_TESTNET_READINESS_CHECKER_HPP

#include "config/NetworkParameters.hpp"
#include "crypto/KeyStore.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

enum class ReadinessStatus {
    READY,
    NOT_READY
};

std::string readinessStatusToString(ReadinessStatus status);

class ReadinessDiagnostic {
public:
    ReadinessDiagnostic();
    ReadinessDiagnostic(std::string checkName, bool passed, std::string detail);

    const std::string& checkName() const;
    bool passed() const;
    const std::string& detail() const;

    std::string serialize() const;

private:
    std::string m_checkName;
    bool m_passed;
    std::string m_detail;
};

/*
 * TestnetReadinessChecker evaluates whether a node meets the minimum
 * requirements to start on an official network.
 *
 * Security principle:
 * A node must not silently join a testnet or mainnet if it is misconfigured.
 * This checker produces a human-readable list of checks with pass/fail
 * status so that an operator knows exactly what must be fixed before the
 * node can participate.
 */
class TestnetReadinessCheckerConfig {
public:
    TestnetReadinessCheckerConfig();

    TestnetReadinessCheckerConfig(
        std::size_t connectedPeers,
        bool genesisVerified,
        std::uint64_t finalizedHeight,
        bool governanceLifecycleVerifierWired,
        bool defenseModeInactive,
        bool legacyPathsBlockedOnOfficialNetworks,
        bool treasuryReportConsistencyVerified,
        bool evidenceCaptureHealthy = true,
        bool chainAuditCompleted = false,
        bool genesisRegistered = false,
        std::string networkClass = "",
        std::string registeredGenesisId = "",
        std::string manifestGenesisId = "",
        bool finalityVerificationActive = false,
        bool peerCompatibilityPassed = true,
        bool latestImportSucceeded = true,
        std::string latestImportRejectionReason = "",
        bool defenseRestrictionsActive = false
    );

    std::size_t connectedPeers() const;
    bool genesisVerified() const;
    std::uint64_t finalizedHeight() const;
    bool governanceLifecycleVerifierWired() const;
    bool defenseModeInactive() const;
    bool legacyPathsBlockedOnOfficialNetworks() const;
    bool treasuryReportConsistencyVerified() const;
    bool evidenceCaptureHealthy() const;
    bool chainAuditCompleted() const;
    bool genesisRegistered() const;
    const std::string& networkClass() const;
    const std::string& registeredGenesisId() const;
    const std::string& manifestGenesisId() const;
    bool finalityVerificationActive() const;
    bool peerCompatibilityPassed() const;
    bool latestImportSucceeded() const;
    const std::string& latestImportRejectionReason() const;
    bool defenseRestrictionsActive() const;

private:
    std::size_t m_connectedPeers;
    bool m_genesisVerified;
    std::uint64_t m_finalizedHeight;
    bool m_governanceLifecycleVerifierWired;
    bool m_defenseModeInactive;
    bool m_legacyPathsBlockedOnOfficialNetworks;
    bool m_treasuryReportConsistencyVerified;
    bool m_evidenceCaptureHealthy;
    bool m_chainAuditCompleted;
    bool m_genesisRegistered;
    std::string m_networkClass;
    std::string m_registeredGenesisId;
    std::string m_manifestGenesisId;
    bool m_finalityVerificationActive;
    bool m_peerCompatibilityPassed;
    bool m_latestImportSucceeded;
    std::string m_latestImportRejectionReason;
    bool m_defenseRestrictionsActive;
};

class TestnetReadinessChecker {
public:
    // Base checks only: network parameters, key policy, genesis, peers, finalized height.
    static std::vector<ReadinessDiagnostic> check(
        const config::NetworkParameters& params,
        const crypto::StoredKeyMetadata& validatorKey,
        std::size_t connectedPeers,
        bool genesisVerified,
        std::uint64_t finalizedHeight
    );

    // Full protocol safety checks: base checks plus defense mode, governance,
    // legacy path enforcement, and treasury report consistency.
    static std::vector<ReadinessDiagnostic> checkWithProtocolSafetyGates(
        const config::NetworkParameters& params,
        const crypto::StoredKeyMetadata& validatorKey,
        const TestnetReadinessCheckerConfig& config
    );

    static ReadinessStatus summarize(
        const std::vector<ReadinessDiagnostic>& checks
    );
};

} // namespace nodo::node

#endif
