#include "node/ProtocolCompletenessGate.hpp"

#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::KeyPair;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::NodeRuntime;
using nodo::node::NodeRuntimeConfig;
using nodo::node::NodeRuntimeFactory;
using nodo::node::ProtocolCompletenessGate;
using nodo::node::ProtocolCompletenessReport;
using nodo::p2p::PeerInfo;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& message
) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

BootstrapValidatorConfig validator() {
    return BootstrapValidatorConfig(
        KeyPair::createDeterministicBls12381KeyPair(
            "protocol-completeness-gate-validator"
        ).publicKey(),
        1,
        1,
        "protocol-completeness-gate-validator"
    );
}

GenesisConfig genesisConfig() {
    return GenesisConfig(
        NetworkParameters::developmentLocal(),
        kTimestamp,
        {
            validator()
        },
        "protocol-completeness-gate-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "protocol-completeness-gate-peer",
        "127.0.0.1:9750",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

NodeRuntime startRuntime() {
    const auto start =
        NodeRuntimeFactory::startFromGenesis(
            NodeRuntimeConfig(
                genesisConfig(),
                localPeer(),
                16
            )
        );

    requireCondition(
        start.started(),
        "Runtime should start from genesis."
    );

    return start.runtime();
}

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-protocol-completeness-gate-tests-" + suffix);
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

bool hasRequirement(
    const ProtocolCompletenessReport& report,
    const std::string& id
) {
    for (const auto& requirement : report.requirements()) {
        if (requirement.id() == id) {
            return true;
        }
    }

    return false;
}

void testRuntimeReportCoversCoreProtocolRequirements() {
    const NodeRuntime runtime =
        startRuntime();

    const ProtocolCompletenessReport report =
        ProtocolCompletenessGate::evaluateRuntime(runtime);

    requireCondition(
        report.complete() &&
        report.failedCount() == 0 &&
        hasRequirement(report, "consensus_fork_choice_finality") &&
        hasRequirement(report, "verifiable_state_root") &&
        hasRequirement(report, "deterministic_state_transition") &&
        hasRequirement(report, "economic_rules") &&
        hasRequirement(report, "sync_protocol") &&
        hasRequirement(report, "advanced_mempool_policy") &&
        hasRequirement(report, "p2p_identity_security") &&
        hasRequirement(report, "complete_block_validation"),
        "Runtime protocol completeness report should pass all core requirements."
    );
}

void testStorageReportFailsWhenManifestIsMissing() {
    const NodeRuntime runtime =
        startRuntime();

    const std::filesystem::path path =
        tempPath("missing-manifest");

    clean(path);

    const ProtocolCompletenessReport report =
        ProtocolCompletenessGate::evaluateRuntimeWithStorage(
            runtime,
            NodeDataDirectoryConfig(path)
        );

    requireCondition(
        !report.complete() &&
        report.failedCount() == 1 &&
        hasRequirement(report, "persistent_chain_state") &&
        report.firstFailure().find("persistent_chain_state") != std::string::npos,
        "Storage-aware protocol completeness report should fail when manifest is missing."
    );

    clean(path);
}

void testStorageReportPassesWithInitializedDirectory() {
    const NodeRuntime runtime =
        startRuntime();

    const std::filesystem::path path =
        tempPath("initialized");

    clean(path);

    const NodeDataDirectoryConfig directoryConfig(path);

    const auto init =
        NodeDataDirectory::initialize(
            directoryConfig,
            genesisConfig(),
            localPeer(),
            kTimestamp + 10
        );

    requireCondition(
        init.initialized(),
        "Node data directory should initialize before storage completeness audit."
    );

    const ProtocolCompletenessReport report =
        ProtocolCompletenessGate::evaluateRuntimeWithStorage(
            runtime,
            directoryConfig
        );

    requireCondition(
        report.complete() &&
        report.failedCount() == 0 &&
        hasRequirement(report, "persistent_chain_state"),
        "Storage-aware protocol completeness report should pass initialized manifest."
    );

    clean(path);
}

} // namespace

int main() {
    try {
        testRuntimeReportCoversCoreProtocolRequirements();
        testStorageReportFailsWhenManifestIsMissing();
        testStorageReportPassesWithInitializedDirectory();

        std::cout << "Nodo protocol completeness gate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo protocol completeness gate tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
