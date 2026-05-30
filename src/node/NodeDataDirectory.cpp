#include "node/NodeDataDirectory.hpp"

#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr const char* MANIFEST_VERSION =
    "NODO_RUNTIME_MANIFEST_V1";

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/' ||
            character == '#';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

bool isSafePath(
    const std::filesystem::path& path
) {
    if (path.empty()) {
        return false;
    }

    for (const auto& part : path) {
        const std::string text =
            part.string();

        if (text == "..") {
            return false;
        }
    }

    return true;
}

std::uint64_t parseU64(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Manifest field missing: " + key);
    }

    return static_cast<std::uint64_t>(
        std::stoull(found->second)
    );
}

std::int64_t parseI64(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Manifest field missing: " + key);
    }

    return static_cast<std::int64_t>(
        std::stoll(found->second)
    );
}

std::string parseString(
    const std::map<std::string, std::string>& fields,
    const std::string& key
) {
    const auto found =
        fields.find(key);

    if (found == fields.end()) {
        throw std::invalid_argument("Manifest field missing: " + key);
    }

    return found->second;
}

} // namespace

NodeDataDirectoryConfig::NodeDataDirectoryConfig()
    : m_rootPath(".nodo") {}

NodeDataDirectoryConfig::NodeDataDirectoryConfig(
    std::filesystem::path rootPath
)
    : m_rootPath(std::move(rootPath)) {}

const std::filesystem::path& NodeDataDirectoryConfig::rootPath() const {
    return m_rootPath;
}

std::filesystem::path NodeDataDirectoryConfig::manifestPath() const {
    return m_rootPath / "manifest.nodo";
}

std::filesystem::path NodeDataDirectoryConfig::genesisConfigPath() const {
    return m_rootPath / "genesis.nodo";
}

std::filesystem::path NodeDataDirectoryConfig::localPeerPath() const {
    return m_rootPath / "peers" / "local_peer.nodo";
}

std::filesystem::path NodeDataDirectoryConfig::runtimeSnapshotPath() const {
    return m_rootPath / "runtime" / "runtime_snapshot.nodo";
}

std::filesystem::path NodeDataDirectoryConfig::blocksDirectoryPath() const {
    return m_rootPath / "blocks";
}

std::filesystem::path NodeDataDirectoryConfig::peersDirectoryPath() const {
    return m_rootPath / "peers";
}

std::filesystem::path NodeDataDirectoryConfig::mempoolDirectoryPath() const {
    return m_rootPath / "mempool";
}

std::filesystem::path NodeDataDirectoryConfig::runtimeDirectoryPath() const {
    return m_rootPath / "runtime";
}

bool NodeDataDirectoryConfig::isValid() const {
    return isSafePath(m_rootPath);
}

std::string NodeDataDirectoryConfig::serialize() const {
    std::ostringstream oss;

    oss << "NodeDataDirectoryConfig{"
        << "rootPath=" << m_rootPath.string()
        << "}";

    return oss.str();
}

NodeRuntimeManifest::NodeRuntimeManifest()
    : m_chainId(""),
      m_networkName(""),
      m_protocolVersion(""),
      m_genesisConfigId(""),
      m_latestBlockHeight(0),
      m_latestBlockHash(""),
      m_validatorCount(0),
      m_peerCount(0),
      m_createdAt(0),
      m_updatedAt(0) {}

NodeRuntimeManifest::NodeRuntimeManifest(
    std::string chainId,
    std::string networkName,
    std::string protocolVersion,
    std::string genesisConfigId,
    std::uint64_t latestBlockHeight,
    std::string latestBlockHash,
    std::size_t validatorCount,
    std::size_t peerCount,
    std::int64_t createdAt,
    std::int64_t updatedAt
)
    : m_chainId(std::move(chainId)),
      m_networkName(std::move(networkName)),
      m_protocolVersion(std::move(protocolVersion)),
      m_genesisConfigId(std::move(genesisConfigId)),
      m_latestBlockHeight(latestBlockHeight),
      m_latestBlockHash(std::move(latestBlockHash)),
      m_validatorCount(validatorCount),
      m_peerCount(peerCount),
      m_createdAt(createdAt),
      m_updatedAt(updatedAt) {}

const std::string& NodeRuntimeManifest::chainId() const {
    return m_chainId;
}

const std::string& NodeRuntimeManifest::networkName() const {
    return m_networkName;
}

const std::string& NodeRuntimeManifest::protocolVersion() const {
    return m_protocolVersion;
}

const std::string& NodeRuntimeManifest::genesisConfigId() const {
    return m_genesisConfigId;
}

std::uint64_t NodeRuntimeManifest::latestBlockHeight() const {
    return m_latestBlockHeight;
}

const std::string& NodeRuntimeManifest::latestBlockHash() const {
    return m_latestBlockHash;
}

std::size_t NodeRuntimeManifest::validatorCount() const {
    return m_validatorCount;
}

std::size_t NodeRuntimeManifest::peerCount() const {
    return m_peerCount;
}

std::int64_t NodeRuntimeManifest::createdAt() const {
    return m_createdAt;
}

std::int64_t NodeRuntimeManifest::updatedAt() const {
    return m_updatedAt;
}

bool NodeRuntimeManifest::isValid() const {
    return isSafeScalar(m_chainId) &&
           isSafeScalar(m_networkName) &&
           isSafeScalar(m_protocolVersion) &&
           isSafeScalar(m_genesisConfigId) &&
           isSafeScalar(m_latestBlockHash) &&
           m_validatorCount > 0 &&
           m_createdAt > 0 &&
           m_updatedAt >= m_createdAt;
}

std::string NodeRuntimeManifest::serialize() const {
    std::ostringstream oss;

    oss << "NodeRuntimeManifest{"
        << "chainId=" << m_chainId
        << ";networkName=" << m_networkName
        << ";protocolVersion=" << m_protocolVersion
        << ";genesisConfigId=" << m_genesisConfigId
        << ";latestBlockHeight=" << m_latestBlockHeight
        << ";latestBlockHash=" << m_latestBlockHash
        << ";validatorCount=" << m_validatorCount
        << ";peerCount=" << m_peerCount
        << ";createdAt=" << m_createdAt
        << ";updatedAt=" << m_updatedAt
        << "}";

    return oss.str();
}

std::string NodeRuntimeManifest::toFileContents() const {
    return serialization::KeyValueFileCodec::serialize(
        MANIFEST_VERSION,
        {
            {"chainId", m_chainId},
            {"networkName", m_networkName},
            {"protocolVersion", m_protocolVersion},
            {"genesisConfigId", m_genesisConfigId},
            {"latestBlockHeight", std::to_string(m_latestBlockHeight)},
            {"latestBlockHash", m_latestBlockHash},
            {"validatorCount", std::to_string(m_validatorCount)},
            {"peerCount", std::to_string(m_peerCount)},
            {"createdAt", std::to_string(m_createdAt)},
            {"updatedAt", std::to_string(m_updatedAt)}
        }
    );
}

NodeRuntimeManifest NodeRuntimeManifest::fromFileContents(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            MANIFEST_VERSION
        );

    document.requireOnlyFields(
        {
            "chainId",
            "networkName",
            "protocolVersion",
            "genesisConfigId",
            "latestBlockHeight",
            "latestBlockHash",
            "validatorCount",
            "peerCount",
            "createdAt",
            "updatedAt"
        }
    );

    const std::map<std::string, std::string> fields =
        document.fields();

    NodeRuntimeManifest manifest(
        parseString(fields, "chainId"),
        parseString(fields, "networkName"),
        parseString(fields, "protocolVersion"),
        parseString(fields, "genesisConfigId"),
        parseU64(fields, "latestBlockHeight"),
        parseString(fields, "latestBlockHash"),
        static_cast<std::size_t>(parseU64(fields, "validatorCount")),
        static_cast<std::size_t>(parseU64(fields, "peerCount")),
        parseI64(fields, "createdAt"),
        parseI64(fields, "updatedAt")
    );

    if (!manifest.isValid()) {
        throw std::invalid_argument("Parsed manifest is invalid.");
    }

    return manifest;
}

NodeRuntimeManifest NodeRuntimeManifest::fromRuntime(
    const NodeRuntime& runtime,
    std::int64_t createdAt,
    std::int64_t updatedAt
) {
    const config::GenesisConfig& genesisConfig =
        runtime.config().genesisConfig();

    return NodeRuntimeManifest(
        genesisConfig.networkParameters().chainId(),
        genesisConfig.networkParameters().networkName(),
        genesisConfig.networkParameters().protocolVersion(),
        genesisConfig.deterministicId(),
        runtime.blockchain().latestBlock().index(),
        runtime.blockchain().latestBlock().hash(),
        runtime.validatorRegistry().activeCount(),
        runtime.peerManager().size(),
        createdAt,
        updatedAt
    );
}

std::string nodeDataDirectoryInitStatusToString(
    NodeDataDirectoryInitStatus status
) {
    switch (status) {
        case NodeDataDirectoryInitStatus::INITIALIZED:
            return "INITIALIZED";
        case NodeDataDirectoryInitStatus::ALREADY_INITIALIZED:
            return "ALREADY_INITIALIZED";
        case NodeDataDirectoryInitStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case NodeDataDirectoryInitStatus::INVALID_GENESIS_CONFIG:
            return "INVALID_GENESIS_CONFIG";
        case NodeDataDirectoryInitStatus::INVALID_LOCAL_PEER:
            return "INVALID_LOCAL_PEER";
        case NodeDataDirectoryInitStatus::GENESIS_BUILD_FAILED:
            return "GENESIS_BUILD_FAILED";
        case NodeDataDirectoryInitStatus::EXISTS_WITH_DIFFERENT_GENESIS:
            return "EXISTS_WITH_DIFFERENT_GENESIS";
        case NodeDataDirectoryInitStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

NodeDataDirectoryInitResult::NodeDataDirectoryInitResult()
    : m_status(NodeDataDirectoryInitStatus::IO_ERROR),
      m_reason("Uninitialized node data directory init result."),
      m_manifest() {}

NodeDataDirectoryInitResult NodeDataDirectoryInitResult::initialized(
    NodeRuntimeManifest manifest
) {
    NodeDataDirectoryInitResult result;
    result.m_status = NodeDataDirectoryInitStatus::INITIALIZED;
    result.m_reason = "";
    result.m_manifest = std::move(manifest);
    return result;
}

NodeDataDirectoryInitResult NodeDataDirectoryInitResult::alreadyInitialized(
    NodeRuntimeManifest manifest
) {
    NodeDataDirectoryInitResult result;
    result.m_status = NodeDataDirectoryInitStatus::ALREADY_INITIALIZED;
    result.m_reason = "Node data directory is already initialized for this genesis.";
    result.m_manifest = std::move(manifest);
    return result;
}

NodeDataDirectoryInitResult NodeDataDirectoryInitResult::rejected(
    NodeDataDirectoryInitStatus status,
    std::string reason
) {
    NodeDataDirectoryInitResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

NodeDataDirectoryInitStatus NodeDataDirectoryInitResult::status() const {
    return m_status;
}

const std::string& NodeDataDirectoryInitResult::reason() const {
    return m_reason;
}

const NodeRuntimeManifest& NodeDataDirectoryInitResult::manifest() const {
    return m_manifest;
}

bool NodeDataDirectoryInitResult::success() const {
    return initialized() || alreadyInitialized();
}

bool NodeDataDirectoryInitResult::initialized() const {
    return m_status == NodeDataDirectoryInitStatus::INITIALIZED &&
           m_manifest.isValid();
}

bool NodeDataDirectoryInitResult::alreadyInitialized() const {
    return m_status == NodeDataDirectoryInitStatus::ALREADY_INITIALIZED &&
           m_manifest.isValid();
}

std::string NodeDataDirectoryInitResult::serialize() const {
    std::ostringstream oss;

    oss << "NodeDataDirectoryInitResult{"
        << "status=" << nodeDataDirectoryInitStatusToString(m_status)
        << ";reason=" << m_reason
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

std::string nodeDataDirectoryReadStatusToString(
    NodeDataDirectoryReadStatus status
) {
    switch (status) {
        case NodeDataDirectoryReadStatus::LOADED:
            return "LOADED";
        case NodeDataDirectoryReadStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case NodeDataDirectoryReadStatus::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case NodeDataDirectoryReadStatus::INVALID_MANIFEST:
            return "INVALID_MANIFEST";
        case NodeDataDirectoryReadStatus::IO_ERROR:
            return "IO_ERROR";
        default:
            return "IO_ERROR";
    }
}

NodeDataDirectoryReadResult::NodeDataDirectoryReadResult()
    : m_status(NodeDataDirectoryReadStatus::IO_ERROR),
      m_reason("Uninitialized node data directory read result."),
      m_manifest() {}

NodeDataDirectoryReadResult NodeDataDirectoryReadResult::loaded(
    NodeRuntimeManifest manifest
) {
    NodeDataDirectoryReadResult result;
    result.m_status = NodeDataDirectoryReadStatus::LOADED;
    result.m_reason = "";
    result.m_manifest = std::move(manifest);
    return result;
}

NodeDataDirectoryReadResult NodeDataDirectoryReadResult::rejected(
    NodeDataDirectoryReadStatus status,
    std::string reason
) {
    NodeDataDirectoryReadResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

NodeDataDirectoryReadStatus NodeDataDirectoryReadResult::status() const {
    return m_status;
}

const std::string& NodeDataDirectoryReadResult::reason() const {
    return m_reason;
}

const NodeRuntimeManifest& NodeDataDirectoryReadResult::manifest() const {
    return m_manifest;
}

bool NodeDataDirectoryReadResult::loaded() const {
    return m_status == NodeDataDirectoryReadStatus::LOADED &&
           m_manifest.isValid();
}

std::string NodeDataDirectoryReadResult::serialize() const {
    std::ostringstream oss;

    oss << "NodeDataDirectoryReadResult{"
        << "status=" << nodeDataDirectoryReadStatusToString(m_status)
        << ";reason=" << m_reason
        << ";manifest=" << (m_manifest.isValid() ? m_manifest.serialize() : "NONE")
        << "}";

    return oss.str();
}

NodeDataDirectoryInitResult NodeDataDirectory::initialize(
    const NodeDataDirectoryConfig& directoryConfig,
    const config::GenesisConfig& genesisConfig,
    const p2p::PeerInfo& localPeer,
    std::int64_t createdAt
) {
    if (!directoryConfig.isValid()) {
        return NodeDataDirectoryInitResult::rejected(
            NodeDataDirectoryInitStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!genesisConfig.isValid()) {
        return NodeDataDirectoryInitResult::rejected(
            NodeDataDirectoryInitStatus::INVALID_GENESIS_CONFIG,
            "Genesis config is invalid."
        );
    }

    if (!localPeer.isValid()) {
        return NodeDataDirectoryInitResult::rejected(
            NodeDataDirectoryInitStatus::INVALID_LOCAL_PEER,
            "Local peer metadata is invalid."
        );
    }

    if (createdAt <= 0) {
        return NodeDataDirectoryInitResult::rejected(
            NodeDataDirectoryInitStatus::INVALID_CONFIG,
            "Initialization timestamp must be positive."
        );
    }

    if (std::filesystem::exists(directoryConfig.manifestPath())) {
        const NodeDataDirectoryReadResult loaded =
            loadManifest(directoryConfig);

        if (!loaded.loaded()) {
            return NodeDataDirectoryInitResult::rejected(
                NodeDataDirectoryInitStatus::IO_ERROR,
                loaded.reason()
            );
        }

        if (loaded.manifest().genesisConfigId() !=
            genesisConfig.deterministicId()) {
            return NodeDataDirectoryInitResult::rejected(
                NodeDataDirectoryInitStatus::EXISTS_WITH_DIFFERENT_GENESIS,
                "Existing manifest belongs to a different genesis config."
            );
        }

        return NodeDataDirectoryInitResult::alreadyInitialized(
            loaded.manifest()
        );
    }

    try {
        ensureDirectoryTree(directoryConfig);

        const NodeRuntimeConfig runtimeConfig(
            genesisConfig,
            localPeer,
            genesisConfig.networkParameters().maxPeerCount()
        );

        const NodeRuntimeStartResult runtimeStart =
            NodeRuntimeFactory::startFromGenesis(runtimeConfig);

        if (!runtimeStart.started()) {
            return NodeDataDirectoryInitResult::rejected(
                NodeDataDirectoryInitStatus::GENESIS_BUILD_FAILED,
                runtimeStart.reason()
            );
        }

        const NodeRuntimeManifest manifest =
            NodeRuntimeManifest::fromRuntime(
                runtimeStart.runtime(),
                createdAt,
                createdAt
            );

        if (!manifest.isValid()) {
            return NodeDataDirectoryInitResult::rejected(
                NodeDataDirectoryInitStatus::GENESIS_BUILD_FAILED,
                "Generated runtime manifest is invalid."
            );
        }

        writeTextFile(
            directoryConfig.genesisConfigPath(),
            genesisConfig.serialize() + "\n"
        );

        writeTextFile(
            directoryConfig.localPeerPath(),
            localPeer.serialize() + "\n"
        );

        writeTextFile(
            directoryConfig.runtimeSnapshotPath(),
            runtimeStart.runtime().serialize() + "\n"
        );

        writeTextFile(
            directoryConfig.manifestPath(),
            manifest.toFileContents()
        );

        return NodeDataDirectoryInitResult::initialized(
            manifest
        );
    } catch (const std::exception& error) {
        return NodeDataDirectoryInitResult::rejected(
            NodeDataDirectoryInitStatus::IO_ERROR,
            error.what()
        );
    }
}

NodeDataDirectoryReadResult NodeDataDirectory::loadManifest(
    const NodeDataDirectoryConfig& directoryConfig
) {
    if (!directoryConfig.isValid()) {
        return NodeDataDirectoryReadResult::rejected(
            NodeDataDirectoryReadStatus::INVALID_CONFIG,
            "Node data directory config is invalid."
        );
    }

    if (!std::filesystem::exists(directoryConfig.manifestPath())) {
        return NodeDataDirectoryReadResult::rejected(
            NodeDataDirectoryReadStatus::NOT_INITIALIZED,
            "Node data directory is not initialized."
        );
    }

    try {
        const NodeRuntimeManifest manifest =
            NodeRuntimeManifest::fromFileContents(
                readTextFile(directoryConfig.manifestPath())
            );

        return NodeDataDirectoryReadResult::loaded(manifest);
    } catch (const std::exception& error) {
        return NodeDataDirectoryReadResult::rejected(
            NodeDataDirectoryReadStatus::INVALID_MANIFEST,
            error.what()
        );
    }
}

NodeDataDirectoryReadResult NodeDataDirectory::writeRuntimeSnapshot(
    const NodeDataDirectoryConfig& directoryConfig,
    const NodeRuntime& runtime,
    std::int64_t updatedAt
) {
    if (!runtime.isValid()) {
        return NodeDataDirectoryReadResult::rejected(
            NodeDataDirectoryReadStatus::INVALID_MANIFEST,
            "Runtime is invalid."
        );
    }

    const NodeDataDirectoryReadResult loaded =
        loadManifest(directoryConfig);

    if (!loaded.loaded()) {
        return loaded;
    }

    try {
        const NodeRuntimeManifest manifest =
            NodeRuntimeManifest::fromRuntime(
                runtime,
                loaded.manifest().createdAt(),
                updatedAt
            );

        writeTextFile(
            directoryConfig.runtimeSnapshotPath(),
            runtime.serialize() + "\n"
        );

        writeTextFile(
            directoryConfig.manifestPath(),
            manifest.toFileContents()
        );

        return NodeDataDirectoryReadResult::loaded(manifest);
    } catch (const std::exception& error) {
        return NodeDataDirectoryReadResult::rejected(
            NodeDataDirectoryReadStatus::IO_ERROR,
            error.what()
        );
    }
}

bool NodeDataDirectory::isInitialized(
    const NodeDataDirectoryConfig& directoryConfig
) {
    return loadManifest(directoryConfig).loaded();
}

void NodeDataDirectory::ensureDirectoryTree(
    const NodeDataDirectoryConfig& directoryConfig
) {
    std::filesystem::create_directories(directoryConfig.rootPath());
    std::filesystem::create_directories(directoryConfig.blocksDirectoryPath());
    std::filesystem::create_directories(directoryConfig.peersDirectoryPath());
    std::filesystem::create_directories(directoryConfig.mempoolDirectoryPath());
    std::filesystem::create_directories(directoryConfig.runtimeDirectoryPath());
}

void NodeDataDirectory::writeTextFile(
    const std::filesystem::path& path,
    const std::string& contents
) {
    storage::AtomicFile::writeTextFile(
        path,
        contents
    );
}

std::string NodeDataDirectory::readTextFile(
    const std::filesystem::path& path
) {
    return storage::AtomicFile::readTextFile(path);
}

} // namespace nodo::node
