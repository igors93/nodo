#include "p2p/DiscoveryService.hpp"
#include "utils/Time.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

namespace nodo::p2p {

// DiscoveryPeerInfo Implementation
bool DiscoveryPeerInfo::isValid() const {
    return !peerId.empty() && !host.empty() && tcpPort > 0 && udpPort > 0;
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
    if (parts.size() < 4) {
        return DiscoveryPeerInfo{};
    }
    try {
        const unsigned long tcp = std::stoul(parts[2]);
        const unsigned long udp = std::stoul(parts[3]);
        if (tcp == 0 || tcp > 65535 || udp == 0 || udp > 65535) {
            return DiscoveryPeerInfo{};
        }
        return DiscoveryPeerInfo{
            parts[0],
            parts[1],
            static_cast<std::uint16_t>(tcp),
            static_cast<std::uint16_t>(udp),
            0
        };
    } catch (...) {
        return DiscoveryPeerInfo{};
    }
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

        startReceive();

        m_ioThread = std::thread([this]() {
            m_ioContext.run();
        });
    } catch (const std::exception& e) {
        m_socket.reset();
    }
}

void DiscoveryService::stop() {
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
    if (peerId == m_localNodeId || peerId.empty()) {
        return;
    }

    std::size_t bucketIndex = getBucketIndex(peerId);
    bool isNewPeer = false;

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
    }

    if (isNewPeer && m_discoveredCallback) {
        m_discoveredCallback(peerId, host, tcpPort);
    }
}

std::vector<DiscoveryPeerInfo> DiscoveryService::findClosestPeers(
    const std::string& targetId,
    std::size_t count
) {
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
    if (!m_socket) return;

    for (const auto& seed : seedPeers) {
        const std::string& seedHost = seed.second.first;
        std::uint16_t seedUdpPort = seed.second.second;

        try {
            asio::ip::udp::endpoint ep(asio::ip::make_address(seedHost), seedUdpPort);
            sendPing(ep);
            sendFindNode(ep, m_localNodeId);
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
    m_socket->async_receive_from(
        asio::buffer(m_recvBuffer),
        m_recvEndpoint,
        [this](const asio::error_code& ec, std::size_t bytesTransferred) {
            handleReceive(ec, bytesTransferred);
        });
}

void DiscoveryService::handleReceive(const asio::error_code& ec, std::size_t bytesTransferred) {
    if (ec) {
        return;
    }

    try {
        std::string message(m_recvBuffer.data(), bytesTransferred);

        // Parse message envelope
        if (message.rfind("KADEMLIA_MSG{", 0) == 0 && message.back() == '}') {
            std::string body = message.substr(13, message.size() - 14);
            std::map<std::string, std::string> fields;
            std::stringstream ss(body);
            std::string part;
            while (std::getline(ss, part, ';')) {
                auto eq = part.find('=');
                if (eq != std::string::npos) {
                    fields[part.substr(0, eq)] = part.substr(eq + 1);
                }
            }

            std::string type = fields["type"];
            std::string senderId = fields["senderId"];
            auto parsePort = [](const std::string& s) -> std::uint16_t {
                try {
                    const unsigned long v = std::stoul(s);
                    return (v > 0 && v <= 65535) ? static_cast<std::uint16_t>(v) : 0;
                } catch (...) { return 0; }
            };
            std::uint16_t senderUdpPort = fields.count("senderUdpPort") ? parsePort(fields["senderUdpPort"]) : 0;
            std::uint16_t senderTcpPort = fields.count("senderTcpPort") ? parsePort(fields["senderTcpPort"]) : 0;

            if (!senderId.empty() && senderId != m_localNodeId && senderUdpPort > 0) {
                // Update routing table with sender info
                addPeer(senderId, m_recvEndpoint.address().to_string(), senderTcpPort, senderUdpPort);

                if (type == "PING") {
                    sendPong(m_recvEndpoint);
                } else if (type == "PONG") {
                    // Already updated peer in addPeer
                } else if (type == "FIND_NODE") {
                    std::string targetId = fields["targetId"];
                    auto closest = findClosestPeers(targetId, 8);
                    sendNeighbors(m_recvEndpoint, closest);
                } else if (type == "NEIGHBORS") {
                    std::string neighborsStr = fields["neighbors"];
                    std::stringstream nss(neighborsStr);
                    std::string peerSer;
                    while (std::getline(nss, peerSer, ',')) {
                        DiscoveryPeerInfo peer = DiscoveryPeerInfo::deserialize(peerSer);
                        if (peer.isValid()) {
                            addPeer(peer.peerId, peer.host, peer.tcpPort, peer.udpPort);
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Ignore malformed messages safely
    }

    startReceive();
}

void DiscoveryService::sendPing(const asio::ip::udp::endpoint& endpoint) {
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
