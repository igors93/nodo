#include "node/RuntimeStartupService.hpp"

#include "config/NetworkProfileRegistry.hpp"
#include "core/GenesisVerifier.hpp"
#include "storage/StorageSchemaVersion.hpp"

#include <utility>

namespace nodo::node {

// ---------------------------------------------------------------------------
// StartupValidationResult
// ---------------------------------------------------------------------------

StartupValidationResult::StartupValidationResult()
    : m_valid(false),
      m_reason("Uninitialized startup validation result.") {}

StartupValidationResult StartupValidationResult::passed() {
    StartupValidationResult r;
    r.m_valid = true;
    r.m_reason = "";
    return r;
}

StartupValidationResult StartupValidationResult::failed(std::string reason) {
    StartupValidationResult r;
    r.m_valid = false;
    r.m_reason = std::move(reason);
    return r;
}

bool StartupValidationResult::valid() const { return m_valid; }
const std::string& StartupValidationResult::reason() const { return m_reason; }

// ---------------------------------------------------------------------------
// RuntimeStartupService
// ---------------------------------------------------------------------------

config::GenesisLookupResult RuntimeStartupService::resolveGenesis(
    const std::string& networkName
) {
    if (!config::NetworkProfileRegistry::isKnown(networkName)) {
        return config::GenesisLookupResult::missing(
            "Unknown network profile '" + networkName + "'. "
            "Only registered network profiles can start a runtime."
        );
    }

    return config::GenesisRegistry::get(networkName);
}

StartupValidationResult RuntimeStartupService::validateNetworkProfile(
    const config::NetworkParameters& params
) {
    if (!params.isValid()) {
        return StartupValidationResult::failed(
            "Network profile parameters are invalid."
        );
    }

    if (config::NetworkProfileRegistry::isMainnetLocked(params.networkName())) {
        return StartupValidationResult::failed(
            "Network profile '" + params.networkName() +
            "' is blocked: mainnet is not ready for runtime startup."
        );
    }

    if (params.chainId().empty() ||
        params.networkName().empty() ||
        params.protocolVersion().empty()) {
        return StartupValidationResult::failed(
            "Network profile identity fields are incomplete."
        );
    }

    if (params.quorumThresholdNumerator() == 0 ||
        params.quorumThresholdDenominator() == 0 ||
        params.quorumThresholdNumerator() > params.quorumThresholdDenominator()) {
        return StartupValidationResult::failed(
            "Network profile quorum parameters are invalid."
        );
    }

    if (params.finalityDepth() == 0) {
        return StartupValidationResult::failed(
            "Network profile finality depth must be at least 1."
        );
    }

    if (config::NetworkProfileRegistry::isOfficialNetwork(params.networkName()) &&
        params.minimumFeeRawUnits() == 0) {
        return StartupValidationResult::failed(
            "Official network profile must define a non-zero minimum fee."
        );
    }

    if (params.storageFormatVersion() != "NODO_STORAGE_V2" ||
        !storage::StorageSchemaVersion::currentNodeDataDirectorySchema()
            .isSupportedNodeDataDirectoryVersion()) {
        return StartupValidationResult::failed(
            "Network profile storage schema is unsupported by this runtime."
        );
    }

    return StartupValidationResult::passed();
}

StartupValidationResult RuntimeStartupService::verifyGenesis(
    const config::GenesisConfig& genesisConfig
) {
    const core::GenesisVerificationResult verified =
        core::GenesisVerifier::verify(genesisConfig);

    if (!verified.isValid()) {
        return StartupValidationResult::failed(
            "Genesis verification failed: " +
            core::genesisVerificationStatusToString(verified.status()) +
            ": " + verified.reason()
        );
    }

    return StartupValidationResult::passed();
}

StartupValidationResult RuntimeStartupService::validateDataDirectoryCompatibility(
    const NodeRuntimeManifest& manifest,
    const config::GenesisConfig& genesis
) {
    const config::NetworkParameters& params = genesis.networkParameters();
    const std::string registeredGenesisId = genesis.deterministicId();

    if (manifest.networkName() != params.networkName() ||
        manifest.chainId() != params.chainId() ||
        manifest.protocolVersion() != params.protocolVersion()) {
        return StartupValidationResult::failed(
            "Data directory belongs to network '" +
            manifest.networkName() +
            "' (chain='" + manifest.chainId() +
            "', protocol='" + manifest.protocolVersion() +
            "'), but command selected network '" +
            params.networkName() +
            "' (chain='" + params.chainId() +
            "', protocol='" + params.protocolVersion() + "')."
        );
    }

    // Genesis identity must match. A directory initialized from a different genesis
    // cannot be reused for a different genesis on the same network name and chain id.
    if (!registeredGenesisId.empty() &&
        !manifest.genesisConfigId().empty() &&
        manifest.genesisConfigId() != registeredGenesisId) {
        return StartupValidationResult::failed(
            "Data directory genesis id '" + manifest.genesisConfigId() +
            "' does not match registered genesis id '" + registeredGenesisId +
            "' for network '" + params.networkName() +
            "'. Directory: cannot be used with a different genesis."
        );
    }

    if (registeredGenesisId.empty()) {
        return StartupValidationResult::failed(
            "Registered genesis id is empty for network '" +
            params.networkName() + "'. Cannot verify data directory genesis identity."
        );
    }

    if (manifest.genesisConfigId().empty()) {
        return StartupValidationResult::failed(
            "Data directory manifest has no genesis id stored. "
            "Directory may be corrupted or initialized by an incompatible version."
        );
    }

    return StartupValidationResult::passed();
}

config::GenesisLookupResult RuntimeStartupService::resolveAndVerify(
    const std::string& networkName
) {
    const config::GenesisLookupResult lookup = resolveGenesis(networkName);
    if (!lookup.found()) {
        return lookup;
    }

    const config::NetworkParameters& params = lookup.genesis().networkParameters();
    const StartupValidationResult profileCheck = validateNetworkProfile(params);
    if (!profileCheck.valid()) {
        return config::GenesisLookupResult::missing(profileCheck.reason());
    }

    const StartupValidationResult genesisCheck = verifyGenesis(lookup.genesis());
    if (!genesisCheck.valid()) {
        return config::GenesisLookupResult::missing(genesisCheck.reason());
    }

    return lookup;
}

} // namespace nodo::node
