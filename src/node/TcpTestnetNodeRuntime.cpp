#include "node/TcpTestnetNodeRuntime.hpp"

#include "node/ChainStatusGossipCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <limits>
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

std::int64_t parseInt64(const std::string& value, const char* fieldName) {
    std::size_t consumed = 0;
    try {
        const long long parsed = std::stoll(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::int64_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            std::string("Peer file ") + fieldName + " is malformed."
        );
    }
}

std::int32_t parseInt32(const std::string& value, const char* fieldName) {
    const std::int64_t parsed = parseInt64(value, fieldName);
    if (parsed < std::numeric_limits<std::int32_t>::min() ||
        parsed > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument(
            std::string("Peer file ") + fieldName + " is outside valid range."
        );
    }
    return static_cast<std::int32_t>(parsed);
}

std::size_t parseSize(const std::string& value, const char* fieldName) {
    if (value.empty() || value.front() == '-') {
        throw std::invalid_argument(
            std::string("Peer file ") + fieldName + " is malformed."
        );
    }
    std::size_t consumed = 0;
    try {
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size() ||
            parsed > std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument("outside valid range");
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(
            std::string("Peer file ") + fieldName + " is malformed."
        );
    }
}

bool parseBool(const std::string& value) {
    if (value == "0") return false;
    if (value == "1") return true;
    throw std::invalid_argument("Peer file quarantine flag is malformed.");
}

} // namespace

TcpTestnetNodeRuntimeConfig::TcpTestnetNodeRuntimeConfig()
    : m_nodeId(),
      m_host(),
      m_port(0),
      m_networkId(),
      m_chainId(),
      m_protocolVersion(),
      m_genesisId(),
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
    std::string genesisId,
    std::filesystem::path dataDirectory,
    std::uint32_t defaultTtlSeconds,
    std::size_t invalidMessageQuarantineThreshold
) : m_nodeId(std::move(nodeId)),
    m_host(std::move(host)),
    m_port(port),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_genesisId(std::move(genesisId)),
    m_dataDirectory(std::move(dataDirectory)),
    m_defaultTtlSeconds(defaultTtlSeconds),
    m_invalidMessageQuarantineThreshold(invalidMessageQuarantineThreshold) {}

const std::string& TcpTestnetNodeRuntimeConfig::nodeId() const { return m_nodeId; }
const std::string& TcpTestnetNodeRuntimeConfig::host() const { return m_host; }
std::uint16_t TcpTestnetNodeRuntimeConfig::port() const { return m_port; }
const std::string& TcpTestnetNodeRuntimeConfig::networkId() const { return m_networkId; }
const std::string& TcpTestnetNodeRuntimeConfig::chainId() const { return m_chainId; }
const std::string& TcpTestnetNodeRuntimeConfig::protocolVersion() const { return m_protocolVersion; }
const std::string& TcpTestnetNodeRuntimeConfig::genesisId() const { return m_genesisId; }
const std::filesystem::path& TcpTestnetNodeRuntimeConfig::dataDirectory() const { return m_dataDirectory; }
std::uint32_t TcpTestnetNodeRuntimeConfig::defaultTtlSeconds() const { return m_defaultTtlSeconds; }
std::size_t TcpTestnetNodeRuntimeConfig::invalidMessageQuarantineThreshold() const { return m_invalidMessageQuarantineThreshold; }

std::filesystem::path TcpTestnetNodeRuntimeConfig::peersFilePath() const {
    return m_dataDirectory / "peers.conf";
}

bool TcpTestnetNodeRuntimeConfig::isValid() const {
    return isSafeScalar(m_nodeId) &&
           isSafeScalar(m_host) &&
           isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
           !m_genesisId.empty() &&
           !m_dataDirectory.empty() &&
           m_defaultTtlSeconds > 0 &&
           m_invalidMessageQuarantineThreshold > 0;
}

TcpTestnetPeerFileEntry::TcpTestnetPeerFileEntry()
    : m_nodeId(),
      m_endpoint(),
      m_publicKeyFingerprint(),
      m_hasPersistentState(false),
      m_firstSeenAt(0),
      m_lastSeenAt(0),
      m_score(0),
      m_quarantined(false),
      m_invalidMessageCount(0) {}

TcpTestnetPeerFileEntry::TcpTestnetPeerFileEntry(
    std::string nodeId,
    p2p::PeerEndpoint endpoint,
    std::string publicKeyFingerprint
) : m_nodeId(std::move(nodeId)),
    m_endpoint(std::move(endpoint)),
    m_publicKeyFingerprint(std::move(publicKeyFingerprint)),
    m_hasPersistentState(false),
    m_firstSeenAt(0),
    m_lastSeenAt(0),
    m_score(0),
    m_quarantined(false),
    m_invalidMessageCount(0) {}

TcpTestnetPeerFileEntry::TcpTestnetPeerFileEntry(
    std::string nodeId,
    p2p::PeerEndpoint endpoint,
    std::string publicKeyFingerprint,
    std::int64_t firstSeenAt,
    std::int64_t lastSeenAt,
    std::int32_t score,
    bool quarantined,
    std::size_t invalidMessageCount
) : m_nodeId(std::move(nodeId)),
    m_endpoint(std::move(endpoint)),
    m_publicKeyFingerprint(std::move(publicKeyFingerprint)),
    m_hasPersistentState(true),
    m_firstSeenAt(firstSeenAt),
    m_lastSeenAt(lastSeenAt),
    m_score(score),
    m_quarantined(quarantined),
    m_invalidMessageCount(invalidMessageCount) {}

const std::string& TcpTestnetPeerFileEntry::nodeId() const { return m_nodeId; }
const p2p::PeerEndpoint& TcpTestnetPeerFileEntry::endpoint() const { return m_endpoint; }
const std::string& TcpTestnetPeerFileEntry::publicKeyFingerprint() const { return m_publicKeyFingerprint; }
bool TcpTestnetPeerFileEntry::hasPersistentState() const { return m_hasPersistentState; }
std::int64_t TcpTestnetPeerFileEntry::firstSeenAt() const { return m_firstSeenAt; }
std::int64_t TcpTestnetPeerFileEntry::lastSeenAt() const { return m_lastSeenAt; }
std::int32_t TcpTestnetPeerFileEntry::score() const { return m_score; }
bool TcpTestnetPeerFileEntry::quarantined() const { return m_quarantined; }
std::size_t TcpTestnetPeerFileEntry::invalidMessageCount() const { return m_invalidMessageCount; }

bool TcpTestnetPeerFileEntry::isValid() const {
    const bool identityValid = isSafeScalar(m_nodeId) &&
           m_endpoint.isValid() &&
           isSafeScalar(m_publicKeyFingerprint);
    return identityValid &&
           (!m_hasPersistentState ||
            (m_firstSeenAt > 0 && m_lastSeenAt >= m_firstSeenAt));
}

std::string TcpTestnetPeerFileEntry::serialize() const {
    std::ostringstream output;
    output << m_nodeId << " "
           << m_endpoint.host() << " "
           << m_endpoint.port() << " "
           << m_publicKeyFingerprint;
    if (m_hasPersistentState) {
        output << " " << m_firstSeenAt
               << " " << m_lastSeenAt
               << " " << m_score
               << " " << (m_quarantined ? 1 : 0)
               << " " << m_invalidMessageCount;
    }
    return output.str();
}

TcpTestnetPeerFileEntry TcpTestnetPeerFileEntry::parseLine(
    const std::string& line
) {
    std::istringstream input(line);
    std::vector<std::string> fields;
    std::string field;
    while (input >> field) {
        fields.push_back(std::move(field));
    }

    if (fields.size() != 4 && fields.size() != 9) {
        throw std::invalid_argument("Peer file line is malformed.");
    }

    TcpTestnetPeerFileEntry entry = fields.size() == 4
        ? TcpTestnetPeerFileEntry(
            fields[0],
            p2p::PeerEndpoint(fields[1], parsePort(fields[2])),
            fields[3]
        )
        : TcpTestnetPeerFileEntry(
            fields[0],
            p2p::PeerEndpoint(fields[1], parsePort(fields[2])),
            fields[3],
            parseInt64(fields[4], "firstSeenAt"),
            parseInt64(fields[5], "lastSeenAt"),
            parseInt32(fields[6], "score"),
            parseBool(fields[7]),
            parseSize(fields[8], "invalidMessageCount")
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
    output << "# nodeId host port publicKeyFingerprint firstSeenAt "
              "lastSeenAt score quarantined invalidMessageCount\n";

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
    m_authenticatedTransport(m_transport),
    m_gossipMesh(makeGossipConfig(), m_authenticatedTransport),
    m_running(false) {
    m_gossipMesh.setPeerPenaltyPersistenceHandler(
        [this]() { savePeersToDisk(); }
    );
}

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

bool TcpTestnetNodeRuntime::hasAuthenticatedSession(
    const std::string& remoteNodeId
) const {
    return m_transport.connected(m_config.nodeId(), remoteNodeId) &&
        m_authenticatedTransport.hasSession(
            m_config.nodeId(),
            remoteNodeId
        );
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
    m_authenticatedTransport.clearSessions();
    m_transport.closeAll();
    m_running = false;
}

p2p::PeerRegistryResult TcpTestnetNodeRuntime::addPeer(
    const p2p::PeerMetadata& peer
) {
    const p2p::PeerRegistryResult registered =
        m_gossipMesh.registerPeer(peer);
    if (!registered.success()) {
        return registered;
    }

    m_transport.registerPeerEndpoint(
        peer.nodeId(),
        peer.endpoint()
    );
    return registered;
}

std::size_t TcpTestnetNodeRuntime::loadPeersFromDisk(
    std::int64_t now
) {
    const std::vector<TcpTestnetPeerFileEntry> entries =
        TcpTestnetPeerStore::load(m_config.peersFilePath());

    std::size_t loaded = 0;

    for (const auto& entry : entries) {
        const std::int64_t firstSeenAt =
            entry.hasPersistentState() ? entry.firstSeenAt() : now;
        const std::int64_t lastSeenAt =
            entry.hasPersistentState() ? entry.lastSeenAt() : now;
        p2p::PeerMetadata peer(
            entry.nodeId(),
            entry.endpoint(),
            entry.publicKeyFingerprint(),
            firstSeenAt,
            lastSeenAt,
            entry.hasPersistentState() ? entry.score() : 0,
            entry.hasPersistentState() && entry.quarantined()
        );

        if (addPeer(peer).success()) {
            if (entry.hasPersistentState()) {
                m_gossipMesh.restorePeerPenaltyState(
                    entry.nodeId(),
                    entry.invalidMessageCount()
                );
            }
            ++loaded;
        }
    }

    return loaded;
}

void TcpTestnetNodeRuntime::savePeersToDisk() const {
    std::vector<TcpTestnetPeerFileEntry> entries;

    for (const auto& peer : m_gossipMesh.peerRegistry().allPeers()) {
        entries.emplace_back(
            peer.nodeId(),
            peer.endpoint(),
            peer.publicKeyFingerprint(),
            peer.firstSeenAt(),
            peer.lastSeenAt(),
            peer.score(),
            peer.quarantined(),
            m_gossipMesh.invalidMessageCountForPeer(peer.nodeId())
        );
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

p2p::TransportResult TcpTestnetNodeRuntime::connectUnverifiedPeer(
    const std::string& remoteNodeId,
    const p2p::PeerEndpoint& endpoint
) {
    if (remoteNodeId.empty() || !endpoint.isValid()) {
        return p2p::TransportResult(
            p2p::TransportStatus::REJECTED,
            "Unverified peer endpoint is invalid."
        );
    }
    const p2p::PeerMetadata* knownPeer =
        m_gossipMesh.peerRegistry().peer(remoteNodeId);
    if (knownPeer != nullptr && knownPeer->quarantined()) {
        return p2p::TransportResult(
            p2p::TransportStatus::REJECTED,
            "Cannot connect quarantined peer for handshake."
        );
    }
    m_transport.registerPeerEndpoint(remoteNodeId, endpoint);
    if (!m_transport.hasPeerEndpoint(remoteNodeId)) {
        return p2p::TransportResult(
            p2p::TransportStatus::REJECTED,
            "Unverified peer node id is invalid."
        );
    }
    return m_transport.connect(m_config.nodeId(), remoteNodeId);
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
        ChainStatusGossipCodec::encode(chainStatus),
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
    p2p::EclipseGuardConfig eclipseConfig = p2p::EclipseGuardConfig::defaults();
    // Testnet/localnet nodes commonly run several peers on loopback during
    // integration tests. Keep the guard mandatory while allowing enough peers
    // from one /24 for local multi-node clusters; public deployments can tighten
    // this through a future config file without changing the admission path.
    eclipseConfig.maxPeersPerSubnet = 8;
    eclipseConfig.maxSingleSubnetFraction = 1.0;

    return p2p::GossipMeshConfig(
        m_config.nodeId(),
        m_config.networkId(),
        m_config.chainId(),
        m_config.protocolVersion(),
        m_config.genesisId(),
        m_config.defaultTtlSeconds(),
        m_config.invalidMessageQuarantineThreshold(),
        true,
        true,
        eclipseConfig
    );
}

} // namespace nodo::node
