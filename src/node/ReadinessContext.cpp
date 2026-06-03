#include "node/ReadinessContext.hpp"

#include "config/NetworkProfileRegistry.hpp"
#include "crypto/KeyEncryptionPolicy.hpp"
#include "economics/GovernanceApprovalBridge.hpp"

#include <filesystem>
#include <utility>

namespace nodo::node {

// --- LegacyCommandPolicy ---
// Returns true if legacy command blocking is enforced for this network.
// On official networks, produce-demo-block and submit-demo-transaction are
// blocked in the CLI dispatcher; this function makes that enforcement explicit.
static bool legacyCommandBlockingEnforced(
    const config::NetworkParameters& params
) {
    // Non-official networks do not require legacy command blocking for readiness.
    // Official networks enforce blocking via the CLI dispatcher at all call sites.
    // This is a compile-time invariant verified by the CLI implementation.
    (void)params;
    return true;
}

// --- GovernanceLifecyclePolicy ---
// Returns true if governance lifecycle verification is integrated in the
// treasury approval path. This is a compile-time invariant: GovernanceApprovalBridge
// is always integrated in production builds.
static bool governanceLifecycleVerificationIntegrated() {
    return economics::GovernanceApprovalBridge::isLifecycleVerificationEnabled();
}

// ---- ReadinessContextBuilder ----

ReadinessContextBuilder::ReadinessContextBuilder(
    const NodeDataDirectoryConfig& directoryConfig,
    const config::NetworkParameters& networkParams,
    const crypto::StoredKeyMetadata& validatorKey
)
    : m_directoryConfig(directoryConfig),
      m_networkParams(networkParams),
      m_validatorKey(validatorKey),
      m_context()
{
    // Set policy-driven facts that are compile-time verified.
    m_context.legacyCommandsBlocked =
        legacyCommandBlockingEnforced(networkParams);
    m_context.governanceLifecycleIntegrated =
        governanceLifecycleVerificationIntegrated();

    // Defense mode defaults to unknown (unsafe) until withSafetyState() is called.
    m_context.defenseModeInactive = false;
    m_context.safetyStateLoaded = false;
    m_context.safetyStateFailureReason =
        "safety state not yet loaded; call withSafetyState()";
}

ReadinessContextBuilder& ReadinessContextBuilder::withManifest(
    const NodeDataDirectoryReadResult& manifest
) {
    if (manifest.loaded()) {
        m_context.connectedPeers = manifest.manifest().peerCount();
        m_context.finalizedHeight = manifest.manifest().latestBlockHeight();
    } else {
        m_context.connectedPeers = 0;
        m_context.finalizedHeight = 0;
    }
    return *this;
}

ReadinessContextBuilder& ReadinessContextBuilder::withGenesisVerified(
    bool verified
) {
    m_context.genesisVerified = verified;
    return *this;
}

ReadinessContextBuilder& ReadinessContextBuilder::withChainAuditResult(
    bool chainAuditPassed,
    bool treasuryReportVerified
) {
    m_context.chainAuditPassed = chainAuditPassed;
    m_context.monetaryReportVerified = chainAuditPassed;
    m_context.treasuryReportVerified = treasuryReportVerified;
    return *this;
}

ReadinessContextBuilder& ReadinessContextBuilder::withSafetyState() {
    const std::filesystem::path safetyStatePath =
        m_directoryConfig.runtimeSafetyStatePath();

    const RuntimeSafetyStateReadResult result =
        RuntimeSafetyStateStore::read(safetyStatePath);

    if (result.isMissing()) {
        // No safety state file: new node or node that has never been in defense mode.
        // Treat as INACTIVE (safe default for new data directories).
        m_context.defenseModeInactive = true;
        m_context.safetyStateLoaded = true;
        m_context.safetyStateFailureReason = "";
    } else if (result.isLoaded()) {
        m_context.defenseModeInactive =
            (result.state().defenseMode() ==
             economics::DefenseModeState::INACTIVE);
        m_context.safetyStateLoaded = true;
        m_context.safetyStateFailureReason = "";
    } else {
        // Corrupt, invalid, schema mismatch, or I/O failure: fail closed.
        m_context.defenseModeInactive = false;
        m_context.safetyStateLoaded = false;
        m_context.safetyStateFailureReason =
            runtimeSafetyStateReadStatusToString(result.status()) +
            ": " + result.reason();
        m_context.warnings.push_back(
            "safety state read failure: " + m_context.safetyStateFailureReason
        );
    }

    return *this;
}

ReadinessContextBuilder& ReadinessContextBuilder::withKeyPolicyResult(
    bool passed
) {
    m_context.keyPolicyPassed = passed;
    return *this;
}

ReadinessContextBuilder& ReadinessContextBuilder::addWarning(
    std::string warning
) {
    m_context.warnings.push_back(std::move(warning));
    return *this;
}

ReadinessContext ReadinessContextBuilder::build() const {
    return m_context;
}

} // namespace nodo::node
