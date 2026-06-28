#ifndef NODO_NODE_SLASHING_EVIDENCE_GOSSIP_ADMISSION_HPP
#define NODO_NODE_SLASHING_EVIDENCE_GOSSIP_ADMISSION_HPP

#include "consensus/EvidencePool.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"
#include "p2p/NetworkEnvelope.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace nodo::node {

enum class SlashingEvidenceGossipStatus {
    ACCEPTED,
    DUPLICATE,
    RATE_LIMITED,
    REJECTED
};

class SlashingEvidenceGossipResult {
public:
    SlashingEvidenceGossipResult(
        SlashingEvidenceGossipStatus status,
        std::string reason,
        std::string evidenceId = ""
    );

    SlashingEvidenceGossipStatus status() const;
    const std::string& reason() const;
    const std::string& evidenceId() const;

    bool accepted() const;
    bool duplicate() const;
    bool rateLimited() const;

private:
    SlashingEvidenceGossipStatus m_status;
    std::string m_reason;
    std::string m_evidenceId;
};

class SlashingEvidenceGossipAdmission {
public:
    SlashingEvidenceGossipResult admit(
        const p2p::NetworkEnvelope& envelope,
        const std::string& expectedNetworkId,
        const std::string& expectedChainId,
        std::uint64_t currentConsensusHeight,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        consensus::EvidencePool& evidencePool,
        std::int64_t now
    );

    std::size_t trackedPeerCount() const;

private:
    struct PeerWindow {
        std::int64_t startedAt = 0;
        std::size_t count = 0;
    };

    std::map<std::string, PeerWindow> m_peerWindows;

    bool consumePeerBudget(
        const std::string& peerId,
        std::int64_t now
    );

    void pruneExpiredPeerWindows(std::int64_t now);
};

} // namespace nodo::node

#endif
