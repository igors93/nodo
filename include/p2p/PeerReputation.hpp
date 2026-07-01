#ifndef NODO_P2P_PEER_REPUTATION_HPP
#define NODO_P2P_PEER_REPUTATION_HPP

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include <list>

namespace nodo::p2p {

class PeerReputation {
public:
    explicit PeerReputation(
        std::int32_t threshold = 50,
        std::size_t maxPeersPerSubnet = 3,
        std::int64_t temporaryBanSeconds = 3600,
        std::int32_t scoreRecoveryPerHour = 10,
        std::size_t maxTrackedPeers = 4096
    );

    void reportBehavior(
        const std::string& nodeId,
        const std::string& ipAddress,
        std::int32_t delta
    );

    void reportBehavior(
        const std::string& nodeId,
        const std::string& ipAddress,
        std::int32_t delta,
        std::int64_t now,
        const std::string& reason
    );

    std::int32_t getScore(const std::string& nodeId, std::int64_t now = 0);
    bool isBanned(const std::string& nodeId, std::int64_t now = 0);
    bool isTemporarilyBanned(const std::string& nodeId, std::int64_t now = 0);
    std::int64_t bannedUntil(const std::string& nodeId) const;
    std::size_t liftExpiredBans(std::int64_t now);

    bool allowConnection(const std::string& ipAddress);
    void releaseConnection(const std::string& ipAddress);

    std::string serialize() const;

private:
    std::int32_t m_banThreshold;
    std::size_t m_maxPeersPerSubnet;
    std::int64_t m_temporaryBanSeconds;
    std::int32_t m_scoreRecoveryPerHour;
    std::size_t m_maxTrackedPeers;
    mutable std::mutex m_mutex;

    std::map<std::string, std::int32_t> m_peerScores;
    std::map<std::string, std::string> m_peerIps;
    std::map<std::string, std::size_t> m_subnetCounts;
    std::map<std::string, std::int64_t> m_bannedUntil;
    std::map<std::string, std::string> m_banReasons;
    std::map<std::string, std::int64_t> m_lastScoreUpdate;

    std::list<std::string> m_lruPeers;
    std::map<std::string, std::list<std::string>::iterator> m_lruMap;

    static std::string getSubnet(const std::string& ipAddress);
    
    // Helper to recover score and update LRU
    void recoverScore(const std::string& nodeId, std::int64_t now);
    void touchLru(const std::string& nodeId);
    void evictLru();
};

} // namespace nodo::p2p

#endif
