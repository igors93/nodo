#include "p2p/PeerReputation.hpp"
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
    m_peerScores[nodeId] += delta;
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
    if (m_subnetCounts[subnet] >= m_maxPeersPerSubnet) {
        return false;
    }
    m_subnetCounts[subnet]++;
    return true;
}

void PeerReputation::releaseConnection(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string subnet = getSubnet(ipAddress);
    auto it = m_subnetCounts.find(subnet);
    if (it != m_subnetCounts.end() && it->second > 0) {
        it->second--;
        if (it->second == 0) {
            m_subnetCounts.erase(it);
        }
    }
}

std::string PeerReputation::getSubnet(const std::string& ipAddress) {
    std::stringstream ss(ipAddress);
    std::string segment;
    std::vector<std::string> parts;
    while (std::getline(ss, segment, '.')) {
        parts.push_back(segment);
    }
    if (parts.size() >= 3) {
        return parts[0] + "." + parts[1] + "." + parts[2];
    }
    return ipAddress;
}

} // namespace nodo::p2p
