#include "p2p/PeerReputation.hpp"

#include <array>
#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

namespace nodo::p2p {

PeerReputation::PeerReputation(
    std::int32_t threshold,
    std::size_t maxPeersPerSubnet,
    std::int64_t temporaryBanSeconds,
    std::int32_t scoreRecoveryPerHour,
    std::size_t maxTrackedPeers
) : m_banThreshold(threshold),
    m_maxPeersPerSubnet(maxPeersPerSubnet),
    m_temporaryBanSeconds(temporaryBanSeconds > 0 ? temporaryBanSeconds : 3600),
    m_scoreRecoveryPerHour(scoreRecoveryPerHour),
    m_maxTrackedPeers(maxTrackedPeers > 0 ? maxTrackedPeers : 4096) {}

void PeerReputation::reportBehavior(
    const std::string& nodeId,
    const std::string& ipAddress,
    std::int32_t delta
) {
    reportBehavior(nodeId, ipAddress, delta, 0, "score-only");
}

void PeerReputation::reportBehavior(
    const std::string& nodeId,
    const std::string& ipAddress,
    std::int32_t delta,
    std::int64_t now,
    const std::string& reason
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (nodeId.empty()) {
        return;
    }

    touchLru(nodeId);
    recoverScore(nodeId, now);

    if (m_peerScores.find(nodeId) == m_peerScores.end()) {
        m_peerScores[nodeId] = 100;
    }
    const std::int64_t adjusted =
        static_cast<std::int64_t>(m_peerScores[nodeId]) + delta;
    if (adjusted > std::numeric_limits<std::int32_t>::max()) {
        m_peerScores[nodeId] = std::numeric_limits<std::int32_t>::max();
    } else if (adjusted < std::numeric_limits<std::int32_t>::min()) {
        m_peerScores[nodeId] = std::numeric_limits<std::int32_t>::min();
    } else {
        m_peerScores[nodeId] = static_cast<std::int32_t>(adjusted);
    }
    m_peerIps[nodeId] = ipAddress;
    m_lastScoreUpdate[nodeId] = now > 0 ? now : 0;

    if (now > 0 && m_peerScores[nodeId] < m_banThreshold) {
        m_bannedUntil[nodeId] = now + m_temporaryBanSeconds;
        m_banReasons[nodeId] = reason.empty() ? "peer.reputation.threshold" : reason;
    }

    evictLru();
}

std::int32_t PeerReputation::getScore(const std::string& nodeId, std::int64_t now) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!nodeId.empty()) {
        touchLru(nodeId);
        recoverScore(nodeId, now);
    }
    auto it = m_peerScores.find(nodeId);
    return (it != m_peerScores.end()) ? it->second : 100;
}

bool PeerReputation::isBanned(const std::string& nodeId, std::int64_t now) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!nodeId.empty()) {
        touchLru(nodeId);
        recoverScore(nodeId, now);
    }
    auto it = m_peerScores.find(nodeId);
    return it != m_peerScores.end() && it->second < m_banThreshold;
}

bool PeerReputation::isTemporarilyBanned(
    const std::string& nodeId,
    std::int64_t now
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!nodeId.empty()) {
        touchLru(nodeId);
    }
    const auto it = m_bannedUntil.find(nodeId);
    return now > 0 && it != m_bannedUntil.end() && it->second > now;
}

std::int64_t PeerReputation::bannedUntil(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_bannedUntil.find(nodeId);
    return it == m_bannedUntil.end() ? 0 : it->second;
}

std::size_t PeerReputation::liftExpiredBans(std::int64_t now) {
    if (now <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t lifted = 0;
    auto it = m_bannedUntil.begin();
    while (it != m_bannedUntil.end()) {
        if (it->second <= now) {
            m_banReasons.erase(it->first);
            it = m_bannedUntil.erase(it);
            ++lifted;
        } else {
            ++it;
        }
    }
    return lifted;
}

bool PeerReputation::allowConnection(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string subnet = getSubnet(ipAddress);
    if (subnet.empty() || m_maxPeersPerSubnet == 0) {
        return false;
    }
    if (m_subnetCounts[subnet] >= m_maxPeersPerSubnet) {
        return false;
    }
    m_subnetCounts[subnet]++;
    return true;
}

void PeerReputation::releaseConnection(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string subnet = getSubnet(ipAddress);
    if (subnet.empty()) {
        return;
    }
    auto it = m_subnetCounts.find(subnet);
    if (it != m_subnetCounts.end() && it->second > 0) {
        it->second--;
        if (it->second == 0) {
            m_subnetCounts.erase(it);
        }
    }
}

std::string PeerReputation::serialize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream output;
    output << "PeerReputation{peers=" << m_peerScores.size()
           << ";bans=" << m_bannedUntil.size()
           << ";scores=[";
    bool first = true;
    for (const auto& [nodeId, score] : m_peerScores) {
        if (!first) output << ",";
        output << nodeId << ":" << score;
        first = false;
    }
    output << "]}";
    return output.str();
}

void PeerReputation::recoverScore(const std::string& nodeId, std::int64_t now) {
    if (now <= 0 || m_scoreRecoveryPerHour <= 0) return;
    
    auto scoreIt = m_peerScores.find(nodeId);
    if (scoreIt == m_peerScores.end() || scoreIt->second >= 100) {
        return;
    }

    auto lastUpdateIt = m_lastScoreUpdate.find(nodeId);
    if (lastUpdateIt == m_lastScoreUpdate.end() || lastUpdateIt->second <= 0) {
        m_lastScoreUpdate[nodeId] = now;
        return;
    }

    std::int64_t elapsed = now - lastUpdateIt->second;
    if (elapsed >= 3600) {
        std::int64_t hours = elapsed / 3600;
        std::int32_t recovered = static_cast<std::int32_t>(hours * m_scoreRecoveryPerHour);
        
        scoreIt->second = std::min(100, scoreIt->second + recovered);
        m_lastScoreUpdate[nodeId] = lastUpdateIt->second + (hours * 3600);
    }
}

void PeerReputation::touchLru(const std::string& nodeId) {
    auto it = m_lruMap.find(nodeId);
    if (it != m_lruMap.end()) {
        m_lruPeers.erase(it->second);
    }
    m_lruPeers.push_front(nodeId);
    m_lruMap[nodeId] = m_lruPeers.begin();
}

void PeerReputation::evictLru() {
    while (m_lruPeers.size() > m_maxTrackedPeers) {
        const std::string evicted = m_lruPeers.back();
        m_lruPeers.pop_back();
        m_lruMap.erase(evicted);

        m_peerScores.erase(evicted);
        m_peerIps.erase(evicted);
        m_bannedUntil.erase(evicted);
        m_banReasons.erase(evicted);
        m_lastScoreUpdate.erase(evicted);
    }
}

std::string PeerReputation::getSubnet(const std::string& ipAddress) {
    if (ipAddress.empty() || ipAddress.back() == '.') {
        return {};
    }

    std::stringstream ss(ipAddress);
    std::string segment;
    std::vector<std::string> parts;
    while (std::getline(ss, segment, '.')) {
        parts.push_back(segment);
    }

    if (parts.size() != 4) {
        return {};
    }

    std::array<unsigned int, 4> octets{};
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (parts[index].empty() || parts[index].size() > 3) {
            return {};
        }
        for (const char character : parts[index]) {
            if (character < '0' || character > '9') {
                return {};
            }
        }
        try {
            const unsigned long parsed = std::stoul(parts[index]);
            if (parsed > 255) {
                return {};
            }
            octets[index] = static_cast<unsigned int>(parsed);
        } catch (...) {
            return {};
        }
    }

    return std::to_string(octets[0]) + "." +
           std::to_string(octets[1]) + "." +
           std::to_string(octets[2]);
}

} // namespace nodo::p2p
