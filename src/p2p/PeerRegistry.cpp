#include "p2p/PeerRegistry.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::p2p {

std::string peerRegistryStatusToString(PeerRegistryStatus status) {
    switch (status) {
        case PeerRegistryStatus::REGISTERED: return "REGISTERED";
        case PeerRegistryStatus::UPDATED: return "UPDATED";
        case PeerRegistryStatus::REJECTED: return "REJECTED";
        case PeerRegistryStatus::NOT_FOUND: return "NOT_FOUND";
        default: return "REJECTED";
    }
}

PeerRegistryResult::PeerRegistryResult()
    : m_status(PeerRegistryStatus::REJECTED),
      m_reason("Uninitialized peer registry result.") {}

PeerRegistryResult::PeerRegistryResult(PeerRegistryStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

PeerRegistryStatus PeerRegistryResult::status() const { return m_status; }
const std::string& PeerRegistryResult::reason() const { return m_reason; }
bool PeerRegistryResult::success() const {
    return m_status == PeerRegistryStatus::REGISTERED ||
           m_status == PeerRegistryStatus::UPDATED;
}

PeerRegistry::PeerRegistry()
    : m_peersByNodeId() {}

PeerRegistryResult PeerRegistry::registerPeer(PeerMetadata peerMetadata) {
    if (!peerMetadata.isValid()) {
        return PeerRegistryResult(
            PeerRegistryStatus::REJECTED,
            "Peer metadata is invalid."
        );
    }

    const auto existing =
        m_peersByNodeId.find(peerMetadata.nodeId());

    const PeerRegistryStatus status =
        existing == m_peersByNodeId.end()
            ? PeerRegistryStatus::REGISTERED
            : PeerRegistryStatus::UPDATED;

    // A fresh handshake may update endpoint and liveness metadata, but it must
    // never erase a reputation penalty already assigned to this node id.
    if (existing != m_peersByNodeId.end()) {
        peerMetadata = PeerMetadata(
            peerMetadata.nodeId(),
            peerMetadata.endpoint(),
            peerMetadata.publicKeyFingerprint(),
            std::min(
                existing->second.firstSeenAt(),
                peerMetadata.firstSeenAt()
            ),
            std::max(
                existing->second.lastSeenAt(),
                peerMetadata.lastSeenAt()
            ),
            existing->second.score(),
            existing->second.quarantined()
        );
    }

    m_peersByNodeId[peerMetadata.nodeId()] = std::move(peerMetadata);

    return PeerRegistryResult(status, "Peer registry accepted peer metadata.");
}

PeerRegistryResult PeerRegistry::updateHeartbeat(
    const std::string& nodeId,
    std::int64_t seenAt
) {
    const auto found = m_peersByNodeId.find(nodeId);
    if (found == m_peersByNodeId.end()) {
        return PeerRegistryResult(PeerRegistryStatus::NOT_FOUND, "Peer was not found.");
    }

    if (seenAt < found->second.lastSeenAt()) {
        return PeerRegistryResult(PeerRegistryStatus::REJECTED, "Heartbeat timestamp moved backwards.");
    }

    found->second = found->second.withHeartbeat(seenAt);
    return PeerRegistryResult(PeerRegistryStatus::UPDATED, "Peer heartbeat updated.");
}

PeerRegistryResult PeerRegistry::adjustScore(
    const std::string& nodeId,
    std::int32_t delta,
    std::string reason
) {
    const auto found = m_peersByNodeId.find(nodeId);
    if (found == m_peersByNodeId.end()) {
        return PeerRegistryResult(PeerRegistryStatus::NOT_FOUND, "Peer was not found.");
    }

    found->second = found->second.withScoreDelta(delta);
    return PeerRegistryResult(PeerRegistryStatus::UPDATED, std::move(reason));
}

PeerRegistryResult PeerRegistry::quarantinePeer(
    const std::string& nodeId,
    std::string reason
) {
    const auto found = m_peersByNodeId.find(nodeId);
    if (found == m_peersByNodeId.end()) {
        return PeerRegistryResult(PeerRegistryStatus::NOT_FOUND, "Peer was not found.");
    }

    found->second = found->second.quarantinedCopy();
    return PeerRegistryResult(PeerRegistryStatus::UPDATED, std::move(reason));
}

bool PeerRegistry::contains(const std::string& nodeId) const {
    return m_peersByNodeId.find(nodeId) != m_peersByNodeId.end();
}

const PeerMetadata* PeerRegistry::peer(const std::string& nodeId) const {
    const auto found = m_peersByNodeId.find(nodeId);
    return found == m_peersByNodeId.end() ? nullptr : &found->second;
}

std::vector<PeerMetadata> PeerRegistry::activePeers() const {
    std::vector<PeerMetadata> peers;
    peers.reserve(m_peersByNodeId.size());

    for (const auto& [_, peerMetadata] : m_peersByNodeId) {
        if (!peerMetadata.quarantined()) {
            peers.push_back(peerMetadata);
        }
    }
    return peers;
}

std::vector<PeerMetadata> PeerRegistry::allPeers() const {
    std::vector<PeerMetadata> peers;
    peers.reserve(m_peersByNodeId.size());

    for (const auto& [_, peerMetadata] : m_peersByNodeId) {
        peers.push_back(peerMetadata);
    }
    return peers;
}

std::size_t PeerRegistry::size() const { return m_peersByNodeId.size(); }

bool PeerRegistry::isValid() const {
    for (const auto& [nodeId, peerMetadata] : m_peersByNodeId) {
        if (nodeId != peerMetadata.nodeId() || !peerMetadata.isValid()) {
            return false;
        }
    }
    return true;
}

std::string PeerRegistry::serialize() const {
    std::ostringstream output;
    output << "PeerRegistry{size=" << m_peersByNodeId.size() << ";peers=[";
    bool first = true;
    for (const auto& [_, peerMetadata] : m_peersByNodeId) {
        if (!first) {
            output << ",";
        }
        output << peerMetadata.serialize();
        first = false;
    }
    output << "]}";
    return output.str();
}

} // namespace nodo::p2p
