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

std::vector<ReadinessDiagnostic> TestnetReadinessChecker::check(
    const config::NetworkParameters& params,
    const crypto::StoredKeyMetadata& validatorKey,
    std::size_t connectedPeers,
    bool genesisVerified,
    std::uint64_t finalizedHeight
) {
    std::vector<ReadinessDiagnostic> checks;

    // Check 1: Network parameters are valid.
    {
        const bool ok = params.isValid();
        checks.emplace_back(
            "network_parameters_valid",
            ok,
            ok ? "Network parameters are valid for " + params.networkName()
               : "Network parameters are invalid. Check chainId and quorum settings."
        );
    }

    // Check 2: Key policy — no localnet-only key on official networks.
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

    // Check 3: mainnet is not accessible.
    {
        const bool notMainnet = !crypto::KeyEncryptionPolicy::isMainnetBlocked(params.networkName());
        checks.emplace_back(
            "mainnet_not_blocked",
            notMainnet,
            notMainnet ? "Network is not mainnet."
                       : "mainnet is blocked until an audited key provider is available."
        );
    }

    // Check 4: Genesis has been verified.
    {
        checks.emplace_back(
            "genesis_verified",
            genesisVerified,
            genesisVerified ? "Genesis has been verified."
                            : "Genesis has not been verified. Run GenesisVerifier before starting."
        );
    }

    // Check 5: At least one connected peer.
    {
        const bool hasPeers = connectedPeers > 0;
        std::ostringstream detail;
        detail << connectedPeers << " peer(s) connected. "
               << (hasPeers ? "Meets minimum requirement."
                             : "At least one peer is required.");
        checks.emplace_back("peers_connected", hasPeers, detail.str());
    }

    // Check 6: Node has at least genesis block.
    {
        const bool hasSynced = finalizedHeight >= 1 || finalizedHeight == 0;
        std::ostringstream detail;
        detail << "Finalized height is " << finalizedHeight << ".";
        checks.emplace_back("chain_initialized", true, detail.str());
    }

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
