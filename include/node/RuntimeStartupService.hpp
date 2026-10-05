#ifndef NODO_NODE_RUNTIME_STARTUP_SERVICE_HPP
#define NODO_NODE_RUNTIME_STARTUP_SERVICE_HPP

#include "config/GenesisRegistry.hpp"
#include "config/NetworkParameters.hpp"
#include "node/NodeDataDirectory.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace nodo::node {

/*
 * StartupValidationResult carries the outcome of runtime startup validation.
 *
 * A failed result includes the precise rejection reason. The CLI formats and
 * displays it; it does not re-interpret it.
 */
class StartupValidationResult {
public:
    StartupValidationResult();

    static StartupValidationResult passed();
    static StartupValidationResult failed(std::string reason);

    bool valid() const;
    const std::string& reason() const;

private:
    bool m_valid;
    std::string m_reason;
};

/*
 * RuntimeStartupService is the single authority for startup validation.
 *
 * The CLI delegates startup decisions here. This service owns:
 *  - genesis resolution (via GenesisRegistry);
 *  - network profile validation;
 *  - genesis verification;
 *  - data directory compatibility checks.
 *
 * Separation of concerns:
 * The CLI calls this service and formats output. It does not duplicate
 * network, genesis, or data-directory compatibility logic.
 */
class RuntimeStartupService {
public:
    // Resolve the registered genesis for a network name.
    // Returns missing() if the network has no registered genesis, which
    // includes networks that require an operator genesis document.
    static config::GenesisLookupResult resolveGenesis(
        const std::string& networkName
    );

    // Resolve the genesis a command should run against.
    // Networks with a built-in genesis resolve through GenesisRegistry and
    // reject an explicit genesis file. Networks that require an operator
    // genesis read genesisFile when it is given (as on init), otherwise the
    // genesis document pinned in dataDirectory at init time.
    static config::GenesisLookupResult resolveGenesis(
        const std::string& networkName,
        const std::filesystem::path& dataDirectory,
        const std::filesystem::path& genesisFile
    );

    // Validate network profile parameters for runtime startup.
    // Returns invalid() with a reason if any parameter is unacceptable.
    static StartupValidationResult validateNetworkProfile(
        const config::NetworkParameters& params
    );

    // Verify the genesis configuration is structurally valid.
    // Returns invalid() with a reason if genesis fails verification.
    static StartupValidationResult verifyGenesis(
        const config::GenesisConfig& genesisConfig
    );

    // Validate that the data directory's stored network identity is compatible
    // with the selected genesis. Checks network name, chain id, protocol version,
    // and genesis id. Returns invalid() on any mismatch.
    static StartupValidationResult validateDataDirectoryCompatibility(
        const NodeRuntimeManifest& manifest,
        const config::GenesisConfig& genesis
    );

    // The network identity part of validateDataDirectoryCompatibility():
    // network name, chain id, and protocol version only.
    static StartupValidationResult validateDataDirectoryNetwork(
        const NodeRuntimeManifest& manifest,
        const config::NetworkParameters& params
    );

    // Combined check: resolves genesis, validates profile, and verifies genesis.
    // Returns the resolved genesis on success or invalid() on first failure.
    // This is the single call most CLI commands make before starting a runtime.
    static config::GenesisLookupResult resolveAndVerify(
        const std::string& networkName
    );

    // resolveAndVerify() for a command bound to a data directory and an
    // optional operator genesis file. See resolveGenesis() above.
    static config::GenesisLookupResult resolveAndVerify(
        const std::string& networkName,
        const std::filesystem::path& dataDirectory,
        const std::filesystem::path& genesisFile
    );
};

} // namespace nodo::node

#endif
