#ifndef NODO_P2P_PEER_REPUTATION_HPP
#define NODO_P2P_PEER_REPUTATION_HPP

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>

namespace nodo::p2p {

class PeerReputation {
public:
    explicit PeerReputation(int32_t threshold = 50, size_t maxPeersPerSubnet = 3);

    void reportBehavior(const std::string& nodeId, const std::string& ipAddress, int32_t delta);
    int32_t getScore(const std::string& nodeId) const;
    bool isBanned(const std::string& nodeId) const;

    bool allowConnection(const std::string& ipAddress);
    void releaseConnection(const std::string& ipAddress);

private:
    int32_t m_banThreshold;
    size_t m_maxPeersPerSubnet;
    mutable std::mutex m_mutex;

    std::map<std::string, int32_t> m_peerScores;
    std::map<std::string, std::string> m_peerIps;
    std::map<std::string, size_t> m_subnetCounts;

    static std::string getSubnet(const std::string& ipAddress);
};

} // namespace nodo::p2p

#endif
