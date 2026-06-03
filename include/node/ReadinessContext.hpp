#ifndef NODO_NODE_READINESS_CONTEXT_HPP
#define NODO_NODE_READINESS_CONTEXT_HPP

#include "config/NetworkParameters.hpp"
#include "crypto/KeyStore.hpp"
#include "node/EvidenceCaptureHealth.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/RuntimeSafetyStateStore.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * ReadinessContext carries all facts derived from real runtime state that are
 * needed to evaluate protocol safety readiness.
 *
 * Security principle:
 * Every field must be derived from actual runtime inspection, storage state,
 * or explicit policy checks — never from hardcoded constants. An unknown or
 * unverifiable critical fact must fail readiness, not default to safe.
 */
struct ReadinessContext {
    std::size_t connectedPeers = 0;
    bool genesisVerified = false;
    bool genesisRegistered = false;
    std::string genesisId;
    std::string networkClass;
    std::uint64_t finalizedHeight = 0;
    bool chainAuditPassed = false;
    bool monetaryReportVerified = false;
    bool treasuryReportVerified = false;
    bool defenseModeInactive = false;
    bool safetyStateLoaded = false;
    std::string safetyStateFailureReason;
    bool governanceLifecycleIntegrated = false;
    bool legacyCommandsBlocked = false;
    bool keyPolicyPassed = false;
    // Evidence capture health: true if healthy or intentionally disabled.
    // false means required capture store is unavailable or has recent failures.
    bool evidenceCaptureHealthy = true;
    bool evidenceCaptureDisabled = false;
    std::vector<std::string> warnings;
};

/*
 * ReadinessContextBuilder derives the ReadinessContext from actual runtime
 * artifacts, persisted state, and policy evaluation.
 */
class ReadinessContextBuilder {
public:
    ReadinessContextBuilder(
        const NodeDataDirectoryConfig& directoryConfig,
        const config::NetworkParameters& networkParams,
        const crypto::StoredKeyMetadata& validatorKey
    );

    // Derive peer and chain facts from the loaded manifest.
    ReadinessContextBuilder& withManifest(
        const NodeDataDirectoryReadResult& manifest
    );

    // Derive genesis facts: verified flag, registered flag, genesis ID, and
    // network class. Call after the genesis has been resolved and verified.
    ReadinessContextBuilder& withGenesisFacts(
        bool verified,
        bool registered,
        const std::string& genesisId,
        const std::string& networkClass
    );

    // Derive genesis verification from the result of GenesisVerifier.
    ReadinessContextBuilder& withGenesisVerified(bool verified);

    // Derive chain audit and report facts from a ChainAuditResult.
    // Pass chainAuditPassed and treasuryReportVerified explicitly so the
    // builder does not need to re-run the audit.
    ReadinessContextBuilder& withChainAuditResult(
        bool chainAuditPassed,
        bool treasuryReportVerified
    );

    // Derive defense mode from persistent RuntimeSafetyState.
    ReadinessContextBuilder& withSafetyState();

    // Derive key policy result.
    ReadinessContextBuilder& withKeyPolicyResult(bool passed);

    // Derive evidence capture health from a live GossipMesh or a health snapshot.
    // Call with DISABLED health when no gossip mesh is available (headless audits).
    ReadinessContextBuilder& withEvidenceCaptureHealth(
        const EvidenceCaptureHealth& health
    );

    // Add a warning message.
    ReadinessContextBuilder& addWarning(std::string warning);

    // Build and return the complete ReadinessContext.
    ReadinessContext build() const;

private:
    const NodeDataDirectoryConfig& m_directoryConfig;
    const config::NetworkParameters& m_networkParams;
    const crypto::StoredKeyMetadata& m_validatorKey;
    ReadinessContext m_context;
};

} // namespace nodo::node

#endif
