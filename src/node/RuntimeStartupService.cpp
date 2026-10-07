#include "node/RuntimeStartupService.hpp"

#include "config/GenesisDocumentCodec.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "core/GenesisVerifier.hpp"
#include "storage/StorageSchemaVersion.hpp"

#include <filesystem>
#include <system_error>
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

config::GenesisLookupResult RuntimeStartupService::resolveGenesis(
    const std::string& networkName,
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& genesisFile
) {
    if (!config::GenesisRegistry::requiresOperatorGenesis(networkName)) {
        if (!genesisFile.empty()) {
            return config::GenesisLookupResult::missing(
                "Network '" + networkName + "' uses its built-in genesis; "
                "--genesis-file applies only to networks that require an "
                "operator genesis document."
            );
        }

        return resolveGenesis(networkName);
    }

    // A data directory that belongs to another network must be reported as
    // such, before its pinned genesis document is read for this network.
    const NodeDataDirectoryReadResult existing =
        NodeDataDirectory::loadManifest(NodeDataDirectoryConfig(dataDirectory));
    if (existing.loaded()) {
        const StartupValidationResult networkCheck =
            validateDataDirectoryNetwork(
                existing.manifest(),
                config::NetworkProfileRegistry::get(networkName)
            );
        if (!networkCheck.valid()) {
            return config::GenesisLookupResult::missing(networkCheck.reason());
        }
    }

    const bool fromDataDirectory = genesisFile.empty();
    const std::filesystem::path path = fromDataDirectory
        ? NodeDataDirectoryConfig(dataDirectory).genesisConfigPath()
        : genesisFile;

    std::error_code existsError;
    if (!std::filesystem::is_regular_file(path, existsError)) {
        if (fromDataDirectory) {
            return config::GenesisLookupResult::missing(
                "Network '" + networkName + "' requires an operator genesis "
                "document, but data directory '" + dataDirectory.string() +
                "' has none. Initialize it with 'nodo init --network " +
                networkName + " --genesis-file PATH'."
            );
        }

        return config::GenesisLookupResult::missing(
            "Genesis file '" + path.string() + "' does not exist."
        );
    }

    try {
        config::GenesisConfig genesis =
            config::GenesisDocumentCodec::loadFile(path);

        if (genesis.networkParameters().networkName() != networkName) {
            return config::GenesisLookupResult::missing(
                "Genesis document '" + path.string() + "' declares network '" +
                genesis.networkParameters().networkName() +
                "', but the command selected network '" + networkName + "'."
            );
        }

        return config::GenesisLookupResult::found(std::move(genesis));
    } catch (const std::exception& error) {
        return config::GenesisLookupResult::missing(
            "Genesis document '" + path.string() + "' is invalid: " +
            error.what()
        );
    }
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
    const StartupValidationResult networkCheck =
        validateDataDirectoryNetwork(manifest, genesis.networkParameters());
    if (!networkCheck.valid()) {
        return networkCheck;
    }

    const config::NetworkParameters& params = genesis.networkParameters();
    const std::string registeredGenesisId = genesis.deterministicId();

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

StartupValidationResult RuntimeStartupService::validateDataDirectoryNetwork(
    const NodeRuntimeManifest& manifest,
    const config::NetworkParameters& params
) {
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

    return StartupValidationResult::passed();
}

namespace {

config::GenesisLookupResult verifyResolvedGenesis(
    const config::GenesisLookupResult& lookup
) {
    if (!lookup.found()) {
        return lookup;
    }

    const config::NetworkParameters& params = lookup.genesis().networkParameters();
    const StartupValidationResult profileCheck =
        RuntimeStartupService::validateNetworkProfile(params);
    if (!profileCheck.valid()) {
        return config::GenesisLookupResult::missing(profileCheck.reason());
    }

    const StartupValidationResult genesisCheck =
        RuntimeStartupService::verifyGenesis(lookup.genesis());
    if (!genesisCheck.valid()) {
        return config::GenesisLookupResult::missing(genesisCheck.reason());
    }

    return lookup;
}

} // namespace

config::GenesisLookupResult RuntimeStartupService::resolveAndVerify(
    const std::string& networkName,
    const std::filesystem::path& dataDirectory,
    const std::filesystem::path& genesisFile
) {
    if (!config::NetworkProfileRegistry::isKnown(networkName)) {
        return resolveGenesis(networkName);
    }

    return verifyResolvedGenesis(
        resolveGenesis(networkName, dataDirectory, genesisFile)
    );
}

config::GenesisLookupResult RuntimeStartupService::resolveAndVerify(
    const std::string& networkName
) {
    return verifyResolvedGenesis(resolveGenesis(networkName));
}

} // namespace nodo::node
