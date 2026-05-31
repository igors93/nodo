#include "node/NodeDataDirectory.hpp"
#include "config/NetworkParameters.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PublicKey.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::CryptoAlgorithm;
using nodo::crypto::PublicKey;
using nodo::node::NodeDataDirectory;
using nodo::node::NodeDataDirectoryConfig;
using nodo::node::NodeDataDirectoryInitStatus;
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

std::filesystem::path tempPath(
    const std::string& suffix
) {
    return std::filesystem::temp_directory_path()
        / ("nodo-node-data-directory-tests-" + suffix);
}

PublicKey publicKey(
    const std::string& suffix
) {
    return PublicKey(
        CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE,
        "node-data-directory-public-key-" + suffix
    );
}

BootstrapValidatorConfig validator(
    const std::string& suffix
) {
    return BootstrapValidatorConfig(
        publicKey(suffix),
        1,
        1,
        "node-data-directory-validator-" + suffix
    );
}

GenesisConfig genesisConfig(
    const std::string& chainId = "nodo-data-dir-test"
) {
    return GenesisConfig(
        NetworkParameters(
            chainId,
            "nodo-data-dir-network",
            "nodo/0.1",
            60,
            1,
            2,
            3,
            1000,
            32
        ),
        kTimestamp,
        {
            validator(chainId + "-a"),
            validator(chainId + "-b")
        },
        "nodo-data-dir-genesis"
    );
}

PeerInfo localPeer() {
    return PeerInfo(
        "local-test-peer",
        "127.0.0.1:9100",
        "nodo/0.1",
        0,
        kTimestamp
    );
}

void clean(
    const std::filesystem::path& path
) {
    std::error_code error;
    std::filesystem::remove_all(
        path,
        error
    );
}

void testInitializeCreatesDurableFiles() {
    const std::filesystem::path path =
        tempPath("init");

    clean(path);

    const NodeDataDirectoryConfig config(path);

    const auto result =
        NodeDataDirectory::initialize(
            config,
            genesisConfig(),
            localPeer(),
            kTimestamp + 10
        );

    requireCondition(
        result.initialized(),
        "Node data directory should initialize."
    );

    requireCondition(
        std::filesystem::exists(config.manifestPath()) &&
        std::filesystem::exists(config.genesisConfigPath()) &&
        std::filesystem::exists(config.localPeerPath()) &&
        std::filesystem::exists(config.runtimeSnapshotPath()),
        "Initialization should create durable files."
    );

    requireCondition(
        NodeDataDirectory::loadManifest(config).loaded(),
        "Manifest should load after initialization."
    );

    const auto loaded =
        NodeDataDirectory::loadManifest(config);

    requireCondition(
        loaded.loaded() &&
        !loaded.manifest().latestStateRoot().empty(),
        "Initialized manifest should include a non-empty latestStateRoot."
    );

    {
        std::ifstream manifestFile(config.manifestPath());
        const std::string manifestContents(
            (std::istreambuf_iterator<char>(manifestFile)),
            std::istreambuf_iterator<char>()
        );

        requireCondition(
            manifestContents.find("latestStateRoot=" + loaded.manifest().latestStateRoot()) != std::string::npos,
            "Manifest file should persist latestStateRoot."
        );
    }

    clean(path);
}

void testInitializeIsIdempotentForSameGenesis() {
    const std::filesystem::path path =
        tempPath("idempotent");

    clean(path);

    const NodeDataDirectoryConfig config(path);

    requireCondition(
        NodeDataDirectory::initialize(
            config,
            genesisConfig(),
            localPeer(),
            kTimestamp + 20
        ).initialized(),
        "First initialization should succeed."
    );

    const auto second =
        NodeDataDirectory::initialize(
            config,
            genesisConfig(),
            localPeer(),
            kTimestamp + 21
        );

    requireCondition(
        second.alreadyInitialized(),
        "Second initialization with same genesis should be idempotent."
    );

    clean(path);
}

void testRejectsDifferentGenesisInExistingDirectory() {
    const std::filesystem::path path =
        tempPath("different-genesis");

    clean(path);

    const NodeDataDirectoryConfig config(path);

    requireCondition(
        NodeDataDirectory::initialize(
            config,
            genesisConfig("nodo-data-dir-a"),
            localPeer(),
            kTimestamp + 30
        ).initialized(),
        "Initial genesis should initialize."
    );

    const auto second =
        NodeDataDirectory::initialize(
            config,
            genesisConfig("nodo-data-dir-b"),
            localPeer(),
            kTimestamp + 31
        );

    requireCondition(
        second.status() == NodeDataDirectoryInitStatus::EXISTS_WITH_DIFFERENT_GENESIS,
        "Different genesis should be rejected in existing directory."
    );

    clean(path);
}

void testMissingDirectoryStatusFailsSafely() {
    const std::filesystem::path path =
        tempPath("missing");

    clean(path);

    requireCondition(
        !NodeDataDirectory::loadManifest(
            NodeDataDirectoryConfig(path)
        ).loaded(),
        "Missing directory should not load manifest."
    );
}

} // namespace

int main() {
    try {
        testInitializeCreatesDurableFiles();
        testInitializeIsIdempotentForSameGenesis();
        testRejectsDifferentGenesisInExistingDirectory();
        testMissingDirectoryStatusFailsSafely();

        std::cout << "Nodo node data directory tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo node data directory tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
