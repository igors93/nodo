#include "p2p/PeerReputation.hpp"

#include <array>
#include <limits>
#include <sstream>
#include <vector>

namespace nodo::p2p {

PeerReputation::PeerReputation(int32_t threshold, size_t maxPeersPerSubnet)
    : m_banThreshold(threshold), m_maxPeersPerSubnet(maxPeersPerSubnet) {}

void PeerReputation::reportBehavior(const std::string& nodeId, const std::string& ipAddress, int32_t delta) {
    std::lock_guard<std::mutex> lock(m_mutex);
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
}

int32_t PeerReputation::getScore(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_peerScores.find(nodeId);
    return (it != m_peerScores.end()) ? it->second : 100;
}

bool PeerReputation::isBanned(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_peerScores.find(nodeId);
    if (it != m_peerScores.end()) {
        return it->second < m_banThreshold;
    }
    return false;
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
