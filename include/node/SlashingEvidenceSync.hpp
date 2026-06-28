#ifndef NODO_NODE_SLASHING_EVIDENCE_SYNC_HPP
#define NODO_NODE_SLASHING_EVIDENCE_SYNC_HPP

#include "consensus/EvidencePool.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "p2p/GossipMesh.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace nodo::node {

struct SlashingEvidenceSyncResult {
    std::uint32_t inventoriesBroadcast = 0;
    std::uint32_t requestsSent = 0;
    std::uint32_t responsesSent = 0;
    std::uint32_t evidenceAccepted = 0;
    std::uint32_t duplicatesSkipped = 0;
    std::uint32_t rejectedMessages = 0;
    std::uint32_t rateLimitedMessages = 0;
};

class SlashingEvidenceSync {
public:
    SlashingEvidenceSync();

    SlashingEvidenceSyncResult tick(
        p2p::GossipMesh& gossip,
        consensus::EvidencePool& evidencePool,
        std::uint64_t currentConsensusHeight,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::int64_t now
    );

    std::size_t pendingRequestCount() const;

private:
    struct PendingRequest {
        std::string peerId;
        std::int64_t expiresAt = 0;
    };

    struct PeerWindow {
        std::int64_t startedAt = 0;
        std::size_t count = 0;
        std::size_t responseCount = 0;
    };

    std::int64_t m_lastInventoryAt;
    std::size_t m_inventoryOffset;
    std::map<std::string, PendingRequest> m_pendingRequests;
    std::map<std::string, PeerWindow> m_peerWindows;

    void processInventories(
        p2p::GossipMesh& gossip,
        const consensus::EvidencePool& evidencePool,
        std::int64_t now,
        SlashingEvidenceSyncResult& result
    );

    void processRequests(
        p2p::GossipMesh& gossip,
        const consensus::EvidencePool& evidencePool,
        std::int64_t now,
        SlashingEvidenceSyncResult& result
    );

    void processResponses(
        p2p::GossipMesh& gossip,
        consensus::EvidencePool& evidencePool,
        std::uint64_t currentConsensusHeight,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::int64_t now,
        SlashingEvidenceSyncResult& result
    );

    void broadcastInventory(
        p2p::GossipMesh& gossip,
        const consensus::EvidencePool& evidencePool,
        std::int64_t now,
        SlashingEvidenceSyncResult& result
    );

    bool consumePeerBudget(
        const std::string& peerId,
        std::int64_t now,
        bool consumesResponseBudget = false
    );
    void pruneExpiredState(std::int64_t now);
};

} // namespace nodo::node

#endif
