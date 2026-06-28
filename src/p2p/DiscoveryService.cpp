#include "p2p/DiscoveryService.hpp"
#include "utils/Time.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <sstream>

namespace nodo::p2p {

namespace {

bool isSafeDiscoveryScalar(
    const std::string& value,
    std::size_t maxSize
) {
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

bool parseDiscoveryPort(const std::string& value, std::uint16_t& out) {
    if (value.empty()) {
        return false;
    }
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed);
        if (consumed != value.size() || parsed == 0 || parsed > 65535) {
            return false;
        }
        out = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseDiscoveryFields(
    const std::string& body,
    std::map<std::string, std::string>& outFields
) {
    if (body.empty() || body.front() == ';' || body.back() == ';') {
        return false;
    }

    std::stringstream stream(body);
    std::string part;
    while (std::getline(stream, part, ';')) {
        const std::size_t separator = part.find('=');
        if (part.empty() || separator == std::string::npos || separator == 0 ||
            part.find('=', separator + 1) != std::string::npos) {
            return false;
        }

        const bool inserted = outFields.emplace(
            part.substr(0, separator),
            part.substr(separator + 1)
        ).second;
        if (!inserted) {
            return false;
        }
    }

    return !outFields.empty();
}

bool hasExactDiscoveryFields(
    const std::map<std::string, std::string>& fields,
    std::initializer_list<const char*> requiredFields
) {
    if (fields.size() != requiredFields.size()) {
        return false;
    }
    for (const char* field : requiredFields) {
        if (fields.find(field) == fields.end()) {
            return false;
        }
    }
    return true;
}

} // namespace

// DiscoveryPeerInfo Implementation
bool DiscoveryPeerInfo::isValid() const {
    return isSafeDiscoveryScalar(peerId, 160) &&
           isSafeDiscoveryScalar(host, 255) &&
           tcpPort > 0 && udpPort > 0;
}

std::string DiscoveryPeerInfo::serialize() const {
    return peerId + "|" + host + "|" + std::to_string(tcpPort) + "|" + std::to_string(udpPort);
}

DiscoveryPeerInfo DiscoveryPeerInfo::deserialize(const std::string& serialized) {
    std::vector<std::string> parts;
    std::stringstream ss(serialized);
    std::string item;
    while (std::getline(ss, item, '|')) {
        parts.push_back(item);
    }
    if (parts.size() != 4) {
        return DiscoveryPeerInfo{};
    }

    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;
    if (!parseDiscoveryPort(parts[2], tcpPort) ||
        !parseDiscoveryPort(parts[3], udpPort)) {
        return DiscoveryPeerInfo{};
    }

    const DiscoveryPeerInfo peer{
        parts[0], parts[1], tcpPort, udpPort, 0
    };
    if (!peer.isValid() || peer.serialize() != serialized) {
        return DiscoveryPeerInfo{};
    }
    return peer;
}

// DiscoveryService Implementation
DiscoveryService::DiscoveryService(
    const std::string& localNodeId,
    std::uint16_t udpPort,
    std::uint16_t tcpPort
) : m_localNodeId(localNodeId),
    m_localUdpPort(udpPort),
    m_localTcpPort(tcpPort),
    m_localIdHash(idHash(localNodeId)) {}

DiscoveryService::~DiscoveryService() {
    stop();
}

void DiscoveryService::start() {
    // Initializes the UDP socket and starts the asynchronous receive loop on the IO thread.
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_socket) return;

    try {
        m_ioContext.restart();
        m_workGuard.emplace(asio::make_work_guard(m_ioContext));
        m_socket = std::make_unique<asio::ip::udp::socket>(
            m_ioContext,
            asio::ip::udp::endpoint(asio::ip::udp::v4(), m_localUdpPort)
        );

        // Learn actual bound port
        m_localUdpPort = m_socket->local_endpoint().port();

        startReceiveLocked();

        m_ioThread = std::thread([this]() {
            m_ioContext.run();
        });
    } catch (const std::exception& e) {
        m_socket.reset();
    }
}

void DiscoveryService::stop() {
    // Gracefully shuts down the UDP socket, cancels pending operations, and joins the IO thread.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket) {
            asio::error_code ec;
            m_socket->close(ec);
        }
    }
    m_workGuard.reset();
    m_ioContext.stop();
    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_socket.reset();
}

void DiscoveryService::addPeer(
    const std::string& peerId,
    const std::string& host,
    std::uint16_t tcpPort,
    std::uint16_t udpPort
) {
    // Inserts or updates a peer in the Kademlia routing table based on distance to the local node.
    const DiscoveryPeerInfo candidate{peerId, host, tcpPort, udpPort, 0};
    if (peerId == m_localNodeId || !candidate.isValid()) {
        return;
    }

    std::size_t bucketIndex = getBucketIndex(peerId);
    bool isNewPeer = false;
    std::function<void(const std::string&, const std::string&, std::uint16_t)>
        discoveredCallback;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& bucket = m_buckets[bucketIndex];
        auto it = std::find_if(bucket.begin(), bucket.end(), [&](const DiscoveryPeerInfo& p) {
            return p.peerId == peerId;
        });

        std::int64_t now = utils::currentUnixTimestamp();

        if (it != bucket.end()) {
            it->host = host;
            it->tcpPort = tcpPort;
            it->udpPort = udpPort;
            it->lastSeenAt = now;
            // Move to most recently seen (back)
            std::rotate(it, it + 1, bucket.end());
        } else {
            DiscoveryPeerInfo newPeer{peerId, host, tcpPort, udpPort, now};
            if (bucket.size() < 20) {
                bucket.push_back(newPeer);
                isNewPeer = true;
            } else {
                // If full, evict oldest if it's inactive (no ping for last 60 seconds)
                if (now - bucket.front().lastSeenAt > 60) {
                    bucket.erase(bucket.begin());
                    bucket.push_back(newPeer);
                    isNewPeer = true;
                }
            }
        }

        if (isNewPeer) {
            discoveredCallback = m_discoveredCallback;
        }
    }

    if (discoveredCallback) {
        discoveredCallback(peerId, host, tcpPort);
    }
}

std::vector<DiscoveryPeerInfo> DiscoveryService::findClosestPeers(
    const std::string& targetId,
    std::size_t count
) {
    // Searches the routing table buckets to find peers with IDs closest to the target hash.
    std::array<unsigned char, 32> targetHash = idHash(targetId);
    std::vector<std::pair<std::array<unsigned char, 32>, DiscoveryPeerInfo>> peersWithDistance;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& bucket : m_buckets) {
            for (const auto& peer : bucket) {
                std::array<unsigned char, 32> peerHash = idHash(peer.peerId);
                peersWithDistance.push_back({xorDist(peerHash, targetHash), peer});
            }
        }
    }

    std::sort(peersWithDistance.begin(), peersWithDistance.end(),
        [](const auto& a, const auto& b) {
            for (std::size_t i = 0; i < 32; ++i) {
                if (a.first[i] < b.first[i]) return true;
                if (a.first[i] > b.first[i]) return false;
            }
            return false;
        });

    std::vector<DiscoveryPeerInfo> results;
    for (std::size_t i = 0; i < std::min(count, peersWithDistance.size()); ++i) {
        results.push_back(peersWithDistance[i].second);
    }
    return results;
}

void DiscoveryService::bootstrap(
    const std::vector<std::pair<std::string, std::pair<std::string, std::uint16_t>>>& seedPeers
) {
    // Initiates discovery by sending initial PING and FIND_NODE requests to known seed peers.
    if (!m_socket) return;

    for (const auto& seed : seedPeers) {
        const std::string& seedHost = seed.second.first;
        std::uint16_t seedUdpPort = seed.second.second;

        try {
            asio::ip::udp::resolver resolver(m_ioContext);
            auto endpoints = resolver.resolve(seedHost, std::to_string(seedUdpPort));
            for (auto ep : endpoints) {
                sendPing(ep.endpoint());
                sendFindNode(ep.endpoint(), m_localNodeId);
                break; // Use the first resolved address
            }
        } catch (...) {
            // Ignore resolution/endpoint parsing exceptions during bootstrap
        }
    }
}

void DiscoveryService::registerPeerDiscoveredCallback(
    std::function<void(const std::string&, const std::string&, std::uint16_t)> callback
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_discoveredCallback = std::move(callback);
}

std::uint16_t DiscoveryService::localUdpPort() const {
    return m_localUdpPort;
}

void DiscoveryService::startReceive() {
    std::lock_guard<std::mutex> lock(m_mutex);
    startReceiveLocked();
}

void DiscoveryService::startReceiveLocked() {
    if (!m_socket || !m_socket->is_open()) {
        return;
    }
    m_socket->async_receive_from(
        asio::buffer(m_recvBuffer),
        m_recvEndpoint,
        [this](const asio::error_code& ec, std::size_t bytesTransferred) {
            handleReceive(ec, bytesTransferred);
        });
}

void DiscoveryService::handleReceive(const asio::error_code& ec, std::size_t bytesTransferred) {
    // Processes incoming UDP packets, updates the routing table, and responds to discovery requests.
    if (ec) {
        if (ec != asio::error::operation_aborted) {
            try {
                startReceive();
            } catch (...) {
                // A failed re-arm leaves discovery stopped but does not crash
                // the node's IO thread.
            }
        }
        return;
    }

    try {
        std::string message(m_recvBuffer.data(), bytesTransferred);

        const auto processMessage = [&]() {
            // Parse message envelope
            if (message.size() < 14 ||
                message.rfind("KADEMLIA_MSG{", 0) != 0 ||
                message.back() != '}') {
                return;
            }

            std::string body = message.substr(13, message.size() - 14);
            std::map<std::string, std::string> fields;
            if (!parseDiscoveryFields(body, fields)) {
                return;
            }

            const auto typeIt = fields.find("type");
            if (typeIt == fields.end()) {
                return;
            }
            const std::string& type = typeIt->second;
            const bool isBasicMessage = type == "PING" || type == "PONG";
            const bool isFindNode = type == "FIND_NODE";
            const bool isNeighbors = type == "NEIGHBORS";
            if ((!isBasicMessage && !isFindNode && !isNeighbors) ||
                (isBasicMessage && !hasExactDiscoveryFields(
                    fields,
                    {"type", "senderId", "senderUdpPort", "senderTcpPort"}
                )) ||
                (isFindNode && !hasExactDiscoveryFields(
                    fields,
                    {"type", "senderId", "senderUdpPort", "senderTcpPort", "targetId"}
                )) ||
                (isNeighbors && !hasExactDiscoveryFields(
                    fields,
                    {"type", "senderId", "senderUdpPort", "senderTcpPort", "neighbors"}
                ))) {
                return;
            }

            std::uint16_t senderUdpPort = 0;
            std::uint16_t senderTcpPort = 0;
            if (!parseDiscoveryPort(fields.at("senderUdpPort"), senderUdpPort) ||
                !parseDiscoveryPort(fields.at("senderTcpPort"), senderTcpPort)) {
                return;
            }

            const std::string& senderId = fields.at("senderId");
            const std::string senderHost = m_recvEndpoint.address().to_string();
            const DiscoveryPeerInfo sender{
                senderId, senderHost, senderTcpPort, senderUdpPort, 0
            };
            if (senderId == m_localNodeId || !sender.isValid()) {
                return;
            }

            std::vector<DiscoveryPeerInfo> neighbors;
            if (isFindNode && !isSafeDiscoveryScalar(fields.at("targetId"), 160)) {
                return;
            }
            if (isNeighbors && !fields.at("neighbors").empty()) {
                if (fields.at("neighbors").front() == ',' ||
                    fields.at("neighbors").back() == ',') {
                    return;
                }
                std::stringstream neighborStream(fields.at("neighbors"));
                std::string serializedPeer;
                while (std::getline(neighborStream, serializedPeer, ',')) {
                    if (neighbors.size() >= 8) {
                        return;
                    }
                    DiscoveryPeerInfo peer =
                        DiscoveryPeerInfo::deserialize(serializedPeer);
                    if (!peer.isValid()) {
                        return;
                    }
                    neighbors.push_back(std::move(peer));
                }
            }

            // Promote only senders of fully validated, supported messages.
            addPeer(senderId, senderHost, senderTcpPort, senderUdpPort);

            if (type == "PING") {
                sendPong(m_recvEndpoint);
            } else if (type == "FIND_NODE") {
                auto closest = findClosestPeers(fields.at("targetId"), 8);
                sendNeighbors(m_recvEndpoint, closest);
            } else if (type == "NEIGHBORS") {
                for (const DiscoveryPeerInfo& peer : neighbors) {
                    addPeer(peer.peerId, peer.host, peer.tcpPort, peer.udpPort);
                }
            }
        };

        processMessage();
    } catch (...) {
        // Ignore malformed messages safely
    }

    try {
        startReceive();
    } catch (...) {
        // Keep malformed or transient socket state from terminating the IO thread.
    }
}

void DiscoveryService::sendPing(const asio::ip::udp::endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_socket) return;
    std::ostringstream oss;
    oss << "KADEMLIA_MSG{"
        << "type=PING"
        << ";senderId=" << m_localNodeId
        << ";senderUdpPort=" << m_localUdpPort
        << ";senderTcpPort=" << m_localTcpPort
        << "}";
    auto payload = std::make_shared<std::string>(oss.str());
    m_socket->async_send_to(asio::buffer(*payload), endpoint, [payload](const asio::error_code&, std::size_t) {});
}

void DiscoveryService::sendPong(const asio::ip::udp::endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_socket) return;
    std::ostringstream oss;
    oss << "KADEMLIA_MSG{"
        << "type=PONG"
        << ";senderId=" << m_localNodeId
        << ";senderUdpPort=" << m_localUdpPort
        << ";senderTcpPort=" << m_localTcpPort
        << "}";
    auto payload = std::make_shared<std::string>(oss.str());
    m_socket->async_send_to(asio::buffer(*payload), endpoint, [payload](const asio::error_code&, std::size_t) {});
}

void DiscoveryService::sendFindNode(const asio::ip::udp::endpoint& endpoint, const std::string& targetId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_socket) return;
    std::ostringstream oss;
    oss << "KADEMLIA_MSG{"
        << "type=FIND_NODE"
        << ";senderId=" << m_localNodeId
        << ";senderUdpPort=" << m_localUdpPort
        << ";senderTcpPort=" << m_localTcpPort
        << ";targetId=" << targetId
        << "}";
    auto payload = std::make_shared<std::string>(oss.str());
    m_socket->async_send_to(asio::buffer(*payload), endpoint, [payload](const asio::error_code&, std::size_t) {});
}

void DiscoveryService::sendNeighbors(const asio::ip::udp::endpoint& endpoint, const std::vector<DiscoveryPeerInfo>& neighbors) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_socket) return;
    std::ostringstream oss;
    oss << "KADEMLIA_MSG{"
        << "type=NEIGHBORS"
        << ";senderId=" << m_localNodeId
        << ";senderUdpPort=" << m_localUdpPort
        << ";senderTcpPort=" << m_localTcpPort
        << ";neighbors=";

    for (std::size_t i = 0; i < neighbors.size(); ++i) {
        if (i != 0) oss << ",";
        oss << neighbors[i].serialize();
    }

    oss << "}";
    auto payload = std::make_shared<std::string>(oss.str());
    m_socket->async_send_to(asio::buffer(*payload), endpoint, [payload](const asio::error_code&, std::size_t) {});
}

std::size_t DiscoveryService::getBucketIndex(const std::string& peerId) {
    std::array<unsigned char, 32> peerHash = idHash(peerId);
    std::array<unsigned char, 32> distance = xorDist(m_localIdHash, peerHash);
    std::size_t length = prefixLength(distance);
    return (length >= 256) ? 255 : length;
}

std::array<unsigned char, 32> DiscoveryService::idHash(const std::string& id) {
    char hexOutput[NODO_HASH_BUFFER_SIZE] = {0};
    nodo_hash_string(id.c_str(), hexOutput, sizeof(hexOutput));

    std::array<unsigned char, 32> bytes = {0};
    // Parse hex characters back to bytes
    for (std::size_t i = 0; i < 32; ++i) {
        std::string byteString = std::string(hexOutput).substr(i * 2, 2);
        try {
            bytes[i] = static_cast<unsigned char>(std::stoul(byteString, nullptr, 16));
        } catch (...) {
            bytes[i] = 0;
        }
    }
    return bytes;
}

std::array<unsigned char, 32> DiscoveryService::xorDist(
    const std::array<unsigned char, 32>& a,
    const std::array<unsigned char, 32>& b
) {
    std::array<unsigned char, 32> dist;
    for (std::size_t i = 0; i < 32; ++i) {
        dist[i] = a[i] ^ b[i];
    }
    return dist;
}

std::size_t DiscoveryService::prefixLength(const std::array<unsigned char, 32>& distance) {
    std::size_t bits = 0;
    for (unsigned char byte : distance) {
        if (byte == 0) {
            bits += 8;
        } else {
            for (int bit = 7; bit >= 0; --bit) {
                if ((byte & (1 << bit)) == 0) {
                    bits++;
                } else {
                    break;
                }
            }
            break;
        }
    }
    return bits;
}

} // namespace nodo::p2p
