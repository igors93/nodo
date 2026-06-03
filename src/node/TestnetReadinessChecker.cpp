#include "node/TestnetReadinessChecker.hpp"

#include "crypto/KeyEncryptionPolicy.hpp"

#include <sstream>

namespace nodo::node {

std::string readinessStatusToString(ReadinessStatus status) {
    switch (status) {
        case ReadinessStatus::READY:     return "READY";
        case ReadinessStatus::NOT_READY: return "NOT_READY";
        default:                         return "UNKNOWN";
    }
}

ReadinessDiagnostic::ReadinessDiagnostic()
    : m_checkName(""),
      m_passed(false),
      m_detail("") {}

ReadinessDiagnostic::ReadinessDiagnostic(
    std::string checkName,
    bool passed,
    std::string detail
)
    : m_checkName(std::move(checkName)),
      m_passed(passed),
      m_detail(std::move(detail)) {}

const std::string& ReadinessDiagnostic::checkName() const { return m_checkName; }
bool ReadinessDiagnostic::passed() const { return m_passed; }
const std::string& ReadinessDiagnostic::detail() const { return m_detail; }

std::string ReadinessDiagnostic::serialize() const {
    return "[" + std::string(m_passed ? "PASS" : "FAIL") +
           "] " + m_checkName + ": " + m_detail;
}

TestnetReadinessCheckerConfig::TestnetReadinessCheckerConfig()
    : m_connectedPeers(0),
      m_genesisVerified(false),
      m_finalizedHeight(0),
      m_governanceLifecycleVerifierWired(false),
      m_defenseModeInactive(true),
      m_legacyPathsBlockedOnOfficialNetworks(false),
      m_treasuryReportConsistencyVerified(false) {}

TestnetReadinessCheckerConfig::TestnetReadinessCheckerConfig(
    std::size_t connectedPeers,
    bool genesisVerified,
    std::uint64_t finalizedHeight,
    bool governanceLifecycleVerifierWired,
    bool defenseModeInactive,
    bool legacyPathsBlockedOnOfficialNetworks,
    bool treasuryReportConsistencyVerified
)
    : m_connectedPeers(connectedPeers),
      m_genesisVerified(genesisVerified),
      m_finalizedHeight(finalizedHeight),
      m_governanceLifecycleVerifierWired(governanceLifecycleVerifierWired),
      m_defenseModeInactive(defenseModeInactive),
      m_legacyPathsBlockedOnOfficialNetworks(legacyPathsBlockedOnOfficialNetworks),
      m_treasuryReportConsistencyVerified(treasuryReportConsistencyVerified) {}

std::size_t TestnetReadinessCheckerConfig::connectedPeers() const { return m_connectedPeers; }
bool TestnetReadinessCheckerConfig::genesisVerified() const { return m_genesisVerified; }
std::uint64_t TestnetReadinessCheckerConfig::finalizedHeight() const { return m_finalizedHeight; }
bool TestnetReadinessCheckerConfig::governanceLifecycleVerifierWired() const { return m_governanceLifecycleVerifierWired; }
bool TestnetReadinessCheckerConfig::defenseModeInactive() const { return m_defenseModeInactive; }
bool TestnetReadinessCheckerConfig::legacyPathsBlockedOnOfficialNetworks() const { return m_legacyPathsBlockedOnOfficialNetworks; }
bool TestnetReadinessCheckerConfig::treasuryReportConsistencyVerified() const { return m_treasuryReportConsistencyVerified; }

namespace {

void addBaseChecks(
    std::vector<ReadinessDiagnostic>& checks,
    const config::NetworkParameters& params,
    const crypto::StoredKeyMetadata& validatorKey,
    std::size_t connectedPeers,
    bool genesisVerified,
    std::uint64_t finalizedHeight
) {
    {
        const bool ok = params.isValid();
        checks.emplace_back(
            "network_parameters_valid",
            ok,
            ok ? "Network parameters are valid for " + params.networkName()
               : "Network parameters are invalid. Check chainId and quorum settings."
        );
    }

    {
        const bool isOfficial = crypto::KeyEncryptionPolicy::isOfficialNetwork(params.networkName());
        const bool keyOk = !isOfficial || !validatorKey.isLocalnetOnly();
        checks.emplace_back(
            "key_policy",
            keyOk,
            keyOk ? "Key '" + validatorKey.keyId() + "' is acceptable for " + params.networkName()
                  : "Key '" + validatorKey.keyId() + "' is a localnet-only key and cannot "
                    "be used on official network '" + params.networkName() + "'."
        );
    }

    {
        const bool notMainnet = !crypto::KeyEncryptionPolicy::isMainnetBlocked(params.networkName());
        checks.emplace_back(
            "mainnet_not_blocked",
            notMainnet,
            notMainnet ? "Network is not mainnet."
                       : "mainnet is blocked until an audited key provider is available."
        );
    }

    {
        checks.emplace_back(
            "genesis_verified",
            genesisVerified,
            genesisVerified ? "Genesis has been verified."
                            : "Genesis has not been verified. Run GenesisVerifier before starting."
        );
    }

    {
        const bool hasPeers = connectedPeers > 0;
        std::ostringstream detail;
        detail << connectedPeers << " peer(s) connected. "
               << (hasPeers ? "Meets minimum requirement."
                             : "At least one peer is required.");
        checks.emplace_back("peers_connected", hasPeers, detail.str());
    }

    {
        std::ostringstream detail;
        detail << "Finalized height is " << finalizedHeight << ".";
        checks.emplace_back("chain_initialized", true, detail.str());
    }
}

void addProtocolSafetyGates(
    std::vector<ReadinessDiagnostic>& checks,
    const config::NetworkParameters& params,
    const TestnetReadinessCheckerConfig& config
) {
    // Governance lifecycle verifier must be integrated in critical paths.
    // This gate exists so that if the integration is ever removed, readiness fails.
    {
        const bool ok = config.governanceLifecycleVerifierWired();
        checks.emplace_back(
            "governance_lifecycle_verifier_wired",
            ok,
            ok ? "Governance lifecycle verifier is integrated in treasury approval path."
               : "Governance lifecycle verifier is NOT integrated. Treasury approvals "
                 "cannot be produced without verified transition history."
        );
    }

    // Defense mode must be INACTIVE to start on official networks.
    {
        const bool isOfficial = crypto::KeyEncryptionPolicy::isOfficialNetwork(params.networkName());
        const bool ok = !isOfficial || config.defenseModeInactive();
        checks.emplace_back(
            "defense_mode_inactive",
            ok,
            ok ? "Defense mode is INACTIVE."
               : "Defense mode is ACTIVE. A chain audit is required before this node "
                 "can participate in '" + params.networkName() + "'."
        );
    }

    // Legacy and demo paths must not produce blocks on official networks.
    {
        const bool isOfficial = crypto::KeyEncryptionPolicy::isOfficialNetwork(params.networkName());
        const bool ok = !isOfficial || config.legacyPathsBlockedOnOfficialNetworks();
        checks.emplace_back(
            "legacy_paths_blocked_on_official_network",
            ok,
            ok ? "Legacy and demo CLI paths are blocked on official networks."
               : "Legacy or demo paths can produce blocks on this official network. "
                 "Block the 'demo' command and any bypass routes before starting."
        );
    }

    // Treasury report consistency must be verified when finalized blocks exist.
    {
        const bool hasBlocks = config.finalizedHeight() > 0;
        const bool ok = !hasBlocks || config.treasuryReportConsistencyVerified();
        checks.emplace_back(
            "treasury_report_consistent",
            ok,
            ok ? (hasBlocks ? "Treasury report is consistent with finalized artifacts."
                            : "No finalized blocks; treasury report check skipped.")
               : "Treasury report has NOT been verified against finalized artifacts. "
                 "Run chain audit before starting."
        );
    }
}

} // namespace

std::vector<ReadinessDiagnostic> TestnetReadinessChecker::check(
    const config::NetworkParameters& params,
    const crypto::StoredKeyMetadata& validatorKey,
    std::size_t connectedPeers,
    bool genesisVerified,
    std::uint64_t finalizedHeight
) {
    std::vector<ReadinessDiagnostic> checks;
    addBaseChecks(checks, params, validatorKey, connectedPeers, genesisVerified, finalizedHeight);
    return checks;
}

std::vector<ReadinessDiagnostic> TestnetReadinessChecker::checkWithProtocolSafetyGates(
    const config::NetworkParameters& params,
    const crypto::StoredKeyMetadata& validatorKey,
    const TestnetReadinessCheckerConfig& config
) {
    std::vector<ReadinessDiagnostic> checks;
    addBaseChecks(
        checks,
        params,
        validatorKey,
        config.connectedPeers(),
        config.genesisVerified(),
        config.finalizedHeight()
    );
    addProtocolSafetyGates(checks, params, config);
    return checks;
}

ReadinessStatus TestnetReadinessChecker::summarize(
    const std::vector<ReadinessDiagnostic>& checks
) {
    for (const auto& check : checks) {
        if (!check.passed()) {
            return ReadinessStatus::NOT_READY;
        }
    }
    return ReadinessStatus::READY;
}

} // namespace nodo::node
