#include "p2p/EclipseGuard.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace nodo::p2p {

// ---------------------------------------------------------------------------
// PeerSubnetInfo
// ---------------------------------------------------------------------------

std::string PeerSubnetInfo::extractSubnetPrefix(const std::string& ip) {
    // Parse an IPv4 address and return the first three octets (the /24 prefix).
    // For an address like "192.168.1.42", returns "192.168.1".
    // Returns empty string for invalid or non-IPv4 addresses.
    if (ip.empty()) {
        return "";
    }

    std::vector<std::string> parts;
    std::string current;
    for (const char c : ip) {
        if (c == '.') {
            if (current.empty()) return ""; // Empty octet like "192..1.1"
            parts.push_back(current);
            current.clear();
        } else if (c >= '0' && c <= '9') {
            current.push_back(c);
        } else if (c == ':') {
            // Port separator, we stop parsing here
            break;
        } else {
            return ""; // Invalid character
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    if (parts.size() != 4) {
        return ""; // IPv4 must have exactly 4 octets
    }

    for (int i = 0; i < 4; ++i) {
        if (parts[i].size() > 3) {
            return ""; // An octet can have at most 3 digits
        }
        int val = std::stoi(parts[i]);
        if (val < 0 || val > 255) {
            return "";
        }
    }

    return parts[0] + "." + parts[1] + "." + parts[2];
}

bool PeerSubnetInfo::isValid() const {
    return !peerId.empty() && !ipAddress.empty() && !subnetPrefix.empty();
}

std::string PeerSubnetInfo::serialize() const {
    std::ostringstream oss;
    oss << "PeerSubnetInfo{"
        << "peerId=" << peerId
        << ";ipAddress=" << ipAddress
        << ";subnetPrefix=" << subnetPrefix
        << ";port=" << port
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// EclipseGuardConfig
// ---------------------------------------------------------------------------

EclipseGuardConfig EclipseGuardConfig::defaults() {
    return EclipseGuardConfig{
        2,      // maxPeersPerSubnet
        50,     // maxTotalPeers
        8,      // minSubnetDiversity
        0.25    // maxSingleSubnetFraction
    };
}

bool EclipseGuardConfig::isValid() const {
    return maxPeersPerSubnet >= 1 &&
           maxTotalPeers >= maxPeersPerSubnet &&
           minSubnetDiversity >= 1 &&
           maxSingleSubnetFraction > 0.0 &&
           maxSingleSubnetFraction <= 1.0;
}

std::string EclipseGuardConfig::serialize() const {
    std::ostringstream oss;
    oss << "EclipseGuardConfig{"
        << "maxPeersPerSubnet=" << maxPeersPerSubnet
        << ";maxTotalPeers=" << maxTotalPeers
        << ";minSubnetDiversity=" << minSubnetDiversity
        << ";maxSingleSubnetFraction=" << maxSingleSubnetFraction
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// eclipseCheckOutcomeToString
// ---------------------------------------------------------------------------

std::string eclipseCheckOutcomeToString(EclipseCheckOutcome outcome) {
    switch (outcome) {
        case EclipseCheckOutcome::ALLOWED:                       return "ALLOWED";
        case EclipseCheckOutcome::REJECTED_SUBNET_SATURATED:     return "REJECTED_SUBNET_SATURATED";
        case EclipseCheckOutcome::REJECTED_DIVERSITY_INSUFFICIENT: return "REJECTED_DIVERSITY_INSUFFICIENT";
        case EclipseCheckOutcome::REJECTED_CAPACITY_FULL:        return "REJECTED_CAPACITY_FULL";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// EclipseCheckResult
// ---------------------------------------------------------------------------

EclipseCheckResult::EclipseCheckResult(
    EclipseCheckOutcome outcome,
    std::string detail,
    bool allowed
)
    : m_outcome(outcome)
    , m_detail(std::move(detail))
    , m_allowed(allowed)
{}

EclipseCheckResult EclipseCheckResult::allowed() {
    return EclipseCheckResult(EclipseCheckOutcome::ALLOWED, "", true);
}

EclipseCheckResult EclipseCheckResult::rejected(
    EclipseCheckOutcome reason,
    std::string detail
) {
    return EclipseCheckResult(reason, std::move(detail), false);
}

bool EclipseCheckResult::isAllowed() const { return m_allowed; }
EclipseCheckOutcome EclipseCheckResult::outcome() const { return m_outcome; }
const std::string& EclipseCheckResult::detail() const { return m_detail; }

std::string EclipseCheckResult::serialize() const {
    std::ostringstream oss;
    oss << "EclipseCheckResult{"
        << "outcome=" << eclipseCheckOutcomeToString(m_outcome)
        << ";allowed=" << (m_allowed ? "true" : "false")
        << ";detail=" << m_detail
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// EclipseGuard
// ---------------------------------------------------------------------------

EclipseGuard::EclipseGuard(EclipseGuardConfig config)
    : m_config(std::move(config))
{}

const EclipseGuardConfig& EclipseGuard::config() const { return m_config; }

std::map<std::string, std::size_t> EclipseGuard::subnetCounts(
    const std::vector<PeerSubnetInfo>& peers
) const {
    std::map<std::string, std::size_t> counts;
    for (const auto& peer : peers) {
        ++counts[peer.subnetPrefix];
    }
    return counts;
}

EclipseCheckResult EclipseGuard::checkAdmission(
    const PeerSubnetInfo& candidate,
    const std::vector<PeerSubnetInfo>& currentPeers
) const {
    // 1. Capacity check
    if (currentPeers.size() >= m_config.maxTotalPeers) {
        return EclipseCheckResult::rejected(
            EclipseCheckOutcome::REJECTED_CAPACITY_FULL,
            "Total peer capacity reached: " + std::to_string(m_config.maxTotalPeers)
        );
    }

    // 2. Per-subnet check
    const auto counts = subnetCounts(currentPeers);
    const auto it = counts.find(candidate.subnetPrefix);
    if (it != counts.end() && it->second >= m_config.maxPeersPerSubnet) {
        return EclipseCheckResult::rejected(
            EclipseCheckOutcome::REJECTED_SUBNET_SATURATED,
            "Subnet " + candidate.subnetPrefix + " already has " +
            std::to_string(it->second) + " peers (max " +
            std::to_string(m_config.maxPeersPerSubnet) + ")"
        );
    }

    // 3. Fraction check: adding this peer must not push one subnet over the limit.
    // We only enforce this when the peer set is large enough for the fraction
    // constraint to be satisfiable (at least 1/maxSingleSubnetFraction peers).
    const std::size_t minPeersForFractionCheck =
        m_config.maxSingleSubnetFraction > 0.0
        ? static_cast<std::size_t>(1.0 / m_config.maxSingleSubnetFraction)
        : m_config.maxTotalPeers;

    if (currentPeers.size() + 1 >= minPeersForFractionCheck) {
        const std::size_t candidateSubnetCount =
            (it != counts.end() ? it->second : 0) + 1;
        const double fraction = static_cast<double>(candidateSubnetCount) /
                                static_cast<double>(currentPeers.size() + 1);

        if (fraction > m_config.maxSingleSubnetFraction) {
            return EclipseCheckResult::rejected(
                EclipseCheckOutcome::REJECTED_DIVERSITY_INSUFFICIENT,
                "Adding peer from subnet " + candidate.subnetPrefix +
                " would exceed max fraction " +
                std::to_string(m_config.maxSingleSubnetFraction)
            );
        }
    }

    return EclipseCheckResult::allowed();
}

std::vector<std::string> EclipseGuard::recommendEvictions(
    const std::vector<PeerSubnetInfo>& currentPeers,
    std::size_t targetCount
) const {
    std::vector<std::string> toEvict;

    if (currentPeers.size() <= targetCount) {
        return toEvict;
    }

    const std::size_t toRemove = currentPeers.size() - targetCount;

    // Build subnet counts and find most over-represented subnet
    auto counts = subnetCounts(currentPeers);

    // Build a map from subnetPrefix → list of peer IDs
    std::map<std::string, std::vector<std::string>> subnetPeers;
    for (const auto& peer : currentPeers) {
        subnetPeers[peer.subnetPrefix].push_back(peer.peerId);
    }

    // Repeatedly evict from the most over-represented subnet
    std::size_t evicted = 0;
    while (evicted < toRemove) {
        // Find subnet with most peers
        std::string maxSubnet;
        std::size_t maxCount = 0;
        for (const auto& [subnet, cnt] : counts) {
            if (cnt > maxCount) {
                maxCount = cnt;
                maxSubnet = subnet;
            }
        }

        if (maxSubnet.empty() || maxCount == 0) {
            break;
        }

        auto& peers = subnetPeers[maxSubnet];
        if (peers.empty()) {
            break;
        }

        toEvict.push_back(peers.back());
        peers.pop_back();
        --counts[maxSubnet];

        ++evicted;
    }

    return toEvict;
}

double EclipseGuard::diversityScore(const std::vector<PeerSubnetInfo>& peers) const {
    if (peers.empty()) {
        return 1.0;
    }

    const auto counts = subnetCounts(peers);
    const std::size_t distinctSubnets = counts.size();
    const std::size_t totalPeers = peers.size();

    if (distinctSubnets >= totalPeers) {
        return 1.0;  // every peer is in a unique subnet
    }

    // Score: ratio of distinct subnets to total peers, capped at 1.0
    const double raw = static_cast<double>(distinctSubnets) /
                       static_cast<double>(totalPeers);

    return raw < 0.0 ? 0.0 : (raw > 1.0 ? 1.0 : raw);
}

} // namespace nodo::p2p
