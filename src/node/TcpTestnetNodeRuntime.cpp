#include "node/TcpTestnetNodeRuntime.hpp"

#include "storage/AtomicFile.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 200) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::uint16_t parsePort(const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument("Peer port is empty.");
    }

    for (const char character : value) {
        if (character < '0' || character > '9') {
            throw std::invalid_argument("Peer port is malformed.");
        }
    }

    const unsigned long parsed = std::stoul(value);
    if (parsed == 0 || parsed > 65535) {
        throw std::invalid_argument("Peer port is outside valid TCP range.");
    }

    return static_cast<std::uint16_t>(parsed);
}

} // namespace

TcpTestnetNodeRuntimeConfig::TcpTestnetNodeRuntimeConfig()
    : m_nodeId(),
      m_host(),
      m_port(0),
      m_networkId(),
      m_chainId(),
      m_protocolVersion(),
      m_dataDirectory(),
      m_defaultTtlSeconds(30),
      m_invalidMessageQuarantineThreshold(3) {}

TcpTestnetNodeRuntimeConfig::TcpTestnetNodeRuntimeConfig(
    std::string nodeId,
    std::string host,
    std::uint16_t port,
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::filesystem::path dataDirectory,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold
) : m_nodeId(std::move(nodeId)),
    m_host(std::move(host)),
    m_port(port),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_dataDirectory(std::move(dataDirectory)),
    m_defaultTtlSeconds(defaultTtlSeconds),
    m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold) {}

const std::string& TcpTestnetNodeRuntimeConfig::nodeId() const { return m_nodeId; }
const std::string& TcpTestnetNodeRuntimeConfig::host() const { return m_host; }
std::uint16_t TcpTestnetNodeRuntimeConfig::port() const { return m_port; }
const std::string& TcpTestnetNodeRuntimeConfig::networkId() const { return m_networkId; }
const std::string& TcpTestnetNodeRuntimeConfig::chainId() const { return m_chainId; }
const std::string& TcpTestnetNodeRuntimeConfig::protocolVersion() const { return m_protocolVersion; }
const std::filesystem::path& TcpTestnetNodeRuntimeConfig::dataDirectory() const { return m_dataDirectory; }
std::uint32_t TcpTestnetNodeRuntimeConfig::defaultTtlSeconds() const { return m_defaultTtlSeconds; }
std::size_t TcpTestnetNodeRuntimeConfig::invalidMessageQuarantineThreshold() const { return m_invalidMessageQuarantineThreshold; }

std::filesystem::path TcpTestnetNodeRuntimeConfig::peersFilePath() const {
    return m_dataDirectory / "peers.conf";
}

bool TcpTestnetNodeRuntimeConfig::isValid() const {
    return isSafeScalar(m_nodeId) &&
           isSafeScalar(m_host) &&
           m_port <= 65535 &&
           isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
           !m_dataDirectory.empty() &&
           m_defaultTtlSeconds > 0 &&
           m_invalidMessageQuarantineThreshold > 0;
}

TcpTestnetPeerFileEntry::TcpTestnetPeerFileEntry()
    : m_nodeId(),
      m_endpoint(),
      m_publicKeyFingerprint() {}

TcpTestnetPeerFileEntry::TcpTestnetPeerFileEntry(
    std::string nodeId,
    p2p::PeerEndpoint endpoint,
    std::string publicKeyFingerprint
) : m_nodeId(std::move(nodeId)),
    m_endpoint(std::move(endpoint)),
    m_publicKeyFingerprint(std::move(publicKeyFingerprint)) {}

const std::string& TcpTestnetPeerFileEntry::nodeId() const { return m_nodeId; }
const p2p::PeerEndpoint& TcpTestnetPeerFileEntry::endpoint() const { return m_endpoint; }
const std::string& TcpTestnetPeerFileEntry::publicKeyFingerprint() const { return m_publicKeyFingerprint; }

bool TcpTestnetPeerFileEntry::isValid() const {
    return isSafeScalar(m_nodeId) &&
           m_endpoint.isValid() &&
           isSafeScalar(m_publicKeyFingerprint);
}

std::string TcpTestnetPeerFileEntry::serialize() const {
    std::ostringstream output;
    output << m_nodeId << " "
           << m_endpoint.host() << " "
           << m_endpoint.port() << " "
           << m_publicKeyFingerprint;
    return output.str();
}

TcpTestnetPeerFileEntry TcpTestnetPeerFileEntry::parseLine(
    const std::string& line
) {
    std::istringstream input(line);
    std::string nodeId;
    std::string host;
    std::string port;
    std::string fingerprint;

    input >> nodeId >> host >> port >> fingerprint;

    if (nodeId.empty() || host.empty() || port.empty() || fingerprint.empty()) {
        throw std::invalid_argument("Peer file line is malformed.");
    }

    TcpTestnetPeerFileEntry entry(
        nodeId,
        p2p::PeerEndpoint(host, parsePort(port)),
        fingerprint
    );

    if (!entry.isValid()) {
        throw std::invalid_argument("Peer file entry is invalid.");
    }

    return entry;
}

std::vector<TcpTestnetPeerFileEntry> TcpTestnetPeerStore::load(
    const std::filesystem::path& peersFile
) {
    std::vector<TcpTestnetPeerFileEntry> peers;

    if (!std::filesystem::exists(peersFile)) {
        return peers;
    }

    std::istringstream input(storage::AtomicFile::readTextFile(peersFile));

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        peers.push_back(TcpTestnetPeerFileEntry::parseLine(line));
    }

    return peers;
}

void TcpTestnetPeerStore::save(
    const std::filesystem::path& peersFile,
    const std::vector<TcpTestnetPeerFileEntry>& peers
) {
    std::filesystem::create_directories(peersFile.parent_path());

    std::ostringstream output;
    output << "# nodeId host port publicKeyFingerprint\n";

    for (const auto& peer : peers) {
        if (peer.isValid()) {
            output << peer.serialize() << "\n";
        }
    }

    storage::AtomicFile::writeTextFile(peersFile, output.str());
}

TcpTestnetNodeRuntime::TcpTestnetNodeRuntime(
    TcpTestnetNodeRuntimeConfig config
) : m_config(std::move(config)),
    m_transport(),
    m_gossipMesh(makeGossipConfig(), m_transport),
    m_running(false) {}

const TcpTestnetNodeRuntimeConfig& TcpTestnetNodeRuntime::config() const {
    return m_config;
}

p2p::TcpTransport& TcpTestnetNodeRuntime::transport() {
    return m_transport;
}

const p2p::TcpTransport& TcpTestnetNodeRuntime::transport() const {
    return m_transport;
}

p2p::GossipMesh& TcpTestnetNodeRuntime::gossipMesh() {
    return m_gossipMesh;
}

const p2p::GossipMesh& TcpTestnetNodeRuntime::gossipMesh() const {
    return m_gossipMesh;
}

bool TcpTestnetNodeRuntime::running() const {
    return m_running;
}

p2p::TransportResult TcpTestnetNodeRuntime::start() {
    if (!m_config.isValid()) {
        return p2p::TransportResult(
            p2p::TransportStatus::REJECTED,
            "TCP testnet runtime config is invalid."
        );
    }

    std::filesystem::create_directories(m_config.dataDirectory());

    p2p::TransportResult result =
        m_transport.bind(
            m_config.nodeId(),
            m_config.host(),
            m_config.port()
        );

    m_running = result.success();
    return result;
}

void TcpTestnetNodeRuntime::stop() {
    m_transport.closeAll();
    m_running = false;
}

p2p::PeerRegistryResult TcpTestnetNodeRuntime::addPeer(
    const p2p::PeerMetadata& peer
) {
    if (peer.isValid()) {
        m_transport.registerPeerEndpoint(
            peer.nodeId(),
            peer.endpoint()
        );
    }

    return m_gossipMesh.registerPeer(peer);
}

std::size_t TcpTestnetNodeRuntime::loadPeersFromDisk(
    std::int64_t now
) {
    const std::vector<TcpTestnetPeerFileEntry> entries =
        TcpTestnetPeerStore::load(m_config.peersFilePath());

    std::size_t loaded = 0;

    for (const auto& entry : entries) {
        p2p::PeerMetadata peer(
            entry.nodeId(),
            entry.endpoint(),
            entry.publicKeyFingerprint(),
            now,
            now,
            0,
            false
        );

        if (addPeer(peer).success()) {
            ++loaded;
        }
    }

    return loaded;
}

void TcpTestnetNodeRuntime::savePeersToDisk() const {
    std::vector<TcpTestnetPeerFileEntry> entries;

    for (const auto& peer : m_gossipMesh.peerRegistry().allPeers()) {
        if (!peer.quarantined()) {
            entries.emplace_back(
                peer.nodeId(),
                peer.endpoint(),
                peer.publicKeyFingerprint()
            );
        }
    }

    TcpTestnetPeerStore::save(
        m_config.peersFilePath(),
        entries
    );
}

p2p::TransportResult TcpTestnetNodeRuntime::connectPeer(
    const std::string& remoteNodeId
) {
    return m_gossipMesh.connectPeer(remoteNodeId);
}

p2p::GossipDeliveryReport TcpTestnetNodeRuntime::broadcast(
    p2p::NetworkMessageType type,
    const std::string& payload,
    std::int64_t now
) {
    return m_gossipMesh.broadcast(type, payload, now);
}

p2p::GossipDeliveryReport TcpTestnetNodeRuntime::broadcastChainStatus(
    const ChainStatusMessage& chainStatus,
    std::int64_t now
) {
    return broadcast(
        p2p::NetworkMessageType::CHAIN_STATUS,
        chainStatus.serialize(),
        now
    );
}

p2p::GossipDeliveryReport TcpTestnetNodeRuntime::tick(
    std::int64_t now
) {
    const p2p::GossipDeliveryReport outbound =
        m_gossipMesh.flushOutbound(now);

    const p2p::GossipDeliveryReport inbound =
        m_gossipMesh.receiveAvailable(now);

    return p2p::GossipDeliveryReport(
        outbound.acceptedCount() + inbound.acceptedCount(),
        outbound.rejectedCount() + inbound.rejectedCount()
    );
}

p2p::GossipMeshConfig TcpTestnetNodeRuntime::makeGossipConfig() const {
    return p2p::GossipMeshConfig(
        m_config.nodeId(),
        m_config.networkId(),
        m_config.chainId(),
        m_config.protocolVersion(),
        m_config.defaultTtlSeconds(),
        m_config.invalidMessageQuarantineThreshold()
    );
}

} // namespace nodo::node
