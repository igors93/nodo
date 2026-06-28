#ifndef NODO_P2P_DISCOVERY_SERVICE_HPP
#define NODO_P2P_DISCOVERY_SERVICE_HPP

#include "crypto/hash.h"
#include <asio.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nodo::p2p {

struct DiscoveryPeerInfo {
    std::string peerId;
    std::string host;
    std::uint16_t tcpPort;
    std::uint16_t udpPort;
    std::int64_t lastSeenAt;

    bool isValid() const;
    std::string serialize() const;
    static DiscoveryPeerInfo deserialize(const std::string& serialized);
};

/*
 * DiscoveryService implements a Kademlia-based DHT service over UDP.
 * It manages peer routing tables using XOR distances and answers dynamic peer searches.
 */
class DiscoveryService {
public:
    DiscoveryService(
        const std::string& localNodeId,
        std::uint16_t udpPort,
        std::uint16_t tcpPort
    );
    ~DiscoveryService();

    void start();
    void stop();

    void addPeer(
        const std::string& peerId,
        const std::string& host,
        std::uint16_t tcpPort,
        std::uint16_t udpPort
    );

    std::vector<DiscoveryPeerInfo> findClosestPeers(
        const std::string& targetId,
        std::size_t count
    );

    void bootstrap(
        const std::vector<std::pair<std::string, std::pair<std::string, std::uint16_t>>>& seedPeers
    );

    void registerPeerDiscoveredCallback(
        std::function<void(const std::string&, const std::string&, std::uint16_t)> callback
    );

    std::uint16_t localUdpPort() const;

private:
    std::string m_localNodeId;
    std::uint16_t m_localUdpPort;
    std::uint16_t m_localTcpPort;
    std::array<unsigned char, 32> m_localIdHash;

    std::mutex m_mutex;
    std::array<std::vector<DiscoveryPeerInfo>, 256> m_buckets;
    std::function<void(const std::string&, const std::string&, std::uint16_t)> m_discoveredCallback;

    asio::io_context m_ioContext;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> m_workGuard;
    std::unique_ptr<asio::ip::udp::socket> m_socket;
    std::thread m_ioThread;

    std::array<char, 4096> m_recvBuffer;
    asio::ip::udp::endpoint m_recvEndpoint;

    void startReceive();
    void startReceiveLocked();
    void handleReceive(const asio::error_code& ec, std::size_t bytesTransferred);
    void sendPing(const asio::ip::udp::endpoint& endpoint);
    void sendPong(const asio::ip::udp::endpoint& endpoint);
    void sendFindNode(const asio::ip::udp::endpoint& endpoint, const std::string& targetId);
    void sendNeighbors(const asio::ip::udp::endpoint& endpoint, const std::vector<DiscoveryPeerInfo>& neighbors);

    std::size_t getBucketIndex(const std::string& peerId);
    static std::array<unsigned char, 32> idHash(const std::string& id);
    static std::array<unsigned char, 32> xorDist(const std::array<unsigned char, 32>& a, const std::array<unsigned char, 32>& b);
    static std::size_t prefixLength(const std::array<unsigned char, 32>& distance);
};

} // namespace nodo::p2p

#endif
