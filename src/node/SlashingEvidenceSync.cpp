#include "node/SlashingEvidenceSync.hpp"

#include "core/ProtocolLimits.hpp"
#include "node/SlashingEvidenceMessages.hpp"
#include "node/VerifiedSlashingEvidenceAdmission.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <vector>

namespace nodo::node {

namespace {

bool hasFreshBinding(
    const p2p::NetworkEnvelope& envelope,
    p2p::NetworkMessageType expectedType,
    const std::string& expectedNetworkId,
    const std::string& expectedChainId,
    const std::string& messageNetworkId,
    const std::string& messageChainId,
    const std::string& messageNodeId,
    std::int64_t messageTimestamp,
    std::int64_t now
) {
    if (now <= 0 || messageTimestamp <= 0) return false;
    const std::int64_t timestampDelta = now >= messageTimestamp
        ? now - messageTimestamp
        : messageTimestamp - now;
    return
           envelope.messageType() == expectedType &&
           envelope.isStructurallyValid(
               core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES
           ) &&
           envelope.payload().size() <=
               core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES &&
           envelope.networkId() == expectedNetworkId &&
           envelope.chainId() == expectedChainId &&
           messageNetworkId == expectedNetworkId &&
           messageChainId == expectedChainId &&
           envelope.senderNodeId() == messageNodeId &&
           envelope.createdAt() == messageTimestamp &&
           timestampDelta <=
               core::ProtocolLimits::MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS;
}

std::int64_t saturatingAdd(std::int64_t value, std::int64_t increment) {
    return value > std::numeric_limits<std::int64_t>::max() - increment
        ? std::numeric_limits<std::int64_t>::max()
        : value + increment;
}

} // namespace

SlashingEvidenceSync::SlashingEvidenceSync()
    : m_lastInventoryAt(0),
      m_inventoryOffset(0),
      m_pendingRequests(),
      m_peerWindows() {}

SlashingEvidenceSyncResult SlashingEvidenceSync::tick(
    p2p::GossipMesh& gossip,
    consensus::EvidencePool& evidencePool,
    std::uint64_t currentConsensusHeight,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    std::int64_t now
) {
    SlashingEvidenceSyncResult result;
    if (now <= 0 || currentConsensusHeight == 0) {
        return result;
    }

    pruneExpiredState(now);
    processInventories(gossip, evidencePool, now, result);
    processRequests(gossip, evidencePool, now, result);
    processResponses(
        gossip,
        evidencePool,
        currentConsensusHeight,
        validatorSetHistory,
        policy,
        provider,
        now,
        result
    );
    broadcastInventory(gossip, evidencePool, now, result);
    return result;
}

std::size_t SlashingEvidenceSync::pendingRequestCount() const {
    return m_pendingRequests.size();
}

void SlashingEvidenceSync::processInventories(
    p2p::GossipMesh& gossip,
    const consensus::EvidencePool& evidencePool,
    std::int64_t now,
    SlashingEvidenceSyncResult& result
) {
    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_INVENTORY
    );
    for (const p2p::NetworkEnvelope& envelope : messages) {
        if (!consumePeerBudget(envelope.senderNodeId(), now)) {
            ++result.rateLimitedMessages;
            continue;
        }
        if (envelope.payload().empty() || envelope.payload().size() >
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
            ++result.rejectedMessages;
            continue;
        }
        try {
            const SlashingEvidenceInventory inventory =
                SlashingEvidenceInventory::deserialize(envelope.payload());
            if (!hasFreshBinding(
                    envelope,
                    p2p::NetworkMessageType::SLASHING_EVIDENCE_INVENTORY,
                    gossip.config().networkId(),
                    gossip.config().chainId(),
                    inventory.networkId(),
                    inventory.chainId(),
                    inventory.announcerNodeId(),
                    inventory.generatedAt(),
                    now)) {
                ++result.rejectedMessages;
                continue;
            }

            std::size_t sentForInventory = 0;
            for (const std::string& evidenceId : inventory.evidenceIds()) {
                if (evidencePool.contains(evidenceId) ||
                    m_pendingRequests.find(evidenceId) !=
                        m_pendingRequests.end()) {
                    continue;
                }
                if (m_pendingRequests.size() >=
                        core::ProtocolLimits::
                            MAX_PENDING_SLASHING_EVIDENCE_REQUESTS ||
                    sentForInventory >=
                        core::ProtocolLimits::
                            MAX_SLASHING_EVIDENCE_REQUESTS_PER_INVENTORY) {
                    break;
                }

                const SlashingEvidenceRequest request(
                    gossip.config().networkId(),
                    gossip.config().chainId(),
                    gossip.config().localNodeId(),
                    evidenceId,
                    now
                );
                const p2p::GossipDeliveryReport delivery = gossip.sendTo(
                    envelope.senderNodeId(),
                    p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST,
                    request.serialize(),
                    now
                );
                if (delivery.acceptedCount() == 0) {
                    continue;
                }
                m_pendingRequests.emplace(
                    evidenceId,
                    PendingRequest{
                        envelope.senderNodeId(),
                        saturatingAdd(
                            now,
                            core::ProtocolLimits::
                                SLASHING_EVIDENCE_REQUEST_TIMEOUT_SECONDS
                        )
                    }
                );
                ++sentForInventory;
                ++result.requestsSent;
            }
        } catch (const std::exception&) {
            ++result.rejectedMessages;
        }
    }
}

void SlashingEvidenceSync::processRequests(
    p2p::GossipMesh& gossip,
    const consensus::EvidencePool& evidencePool,
    std::int64_t now,
    SlashingEvidenceSyncResult& result
) {
    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST
    );
    for (const p2p::NetworkEnvelope& envelope : messages) {
        if (!consumePeerBudget(envelope.senderNodeId(), now, true)) {
            ++result.rateLimitedMessages;
            continue;
        }
        if (envelope.payload().empty() || envelope.payload().size() >
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
            ++result.rejectedMessages;
            continue;
        }
        try {
            const SlashingEvidenceRequest request =
                SlashingEvidenceRequest::deserialize(envelope.payload());
            if (!hasFreshBinding(
                    envelope,
                    p2p::NetworkMessageType::SLASHING_EVIDENCE_REQUEST,
                    gossip.config().networkId(),
                    gossip.config().chainId(),
                    request.networkId(),
                    request.chainId(),
                    request.requesterNodeId(),
                    request.requestedAt(),
                    now)) {
                ++result.rejectedMessages;
                continue;
            }

            const consensus::DoubleVoteEvidence* evidence =
                evidencePool.doubleVoteEvidenceById(request.evidenceId());
            if (evidence == nullptr) {
                continue;
            }
            const SlashingEvidenceResponse response(
                gossip.config().networkId(),
                gossip.config().chainId(),
                gossip.config().localNodeId(),
                *evidence,
                now
            );
            const p2p::GossipDeliveryReport delivery = gossip.sendTo(
                envelope.senderNodeId(),
                p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE,
                response.serialize(),
                now
            );
            if (delivery.acceptedCount() > 0) {
                ++result.responsesSent;
            }
        } catch (const std::exception&) {
            ++result.rejectedMessages;
        }
    }
}

void SlashingEvidenceSync::processResponses(
    p2p::GossipMesh& gossip,
    consensus::EvidencePool& evidencePool,
    std::uint64_t currentConsensusHeight,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    std::int64_t now,
    SlashingEvidenceSyncResult& result
) {
    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE
    );
    for (const p2p::NetworkEnvelope& envelope : messages) {
        if (!consumePeerBudget(envelope.senderNodeId(), now)) {
            ++result.rateLimitedMessages;
            continue;
        }
        if (envelope.payload().empty() || envelope.payload().size() >
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
            ++result.rejectedMessages;
            continue;
        }
        try {
            const SlashingEvidenceResponse response =
                SlashingEvidenceResponse::deserialize(envelope.payload());
            if (!hasFreshBinding(
                    envelope,
                    p2p::NetworkMessageType::SLASHING_EVIDENCE_RESPONSE,
                    gossip.config().networkId(),
                    gossip.config().chainId(),
                    response.networkId(),
                    response.chainId(),
                    response.responderNodeId(),
                    response.respondedAt(),
                    now)) {
                ++result.rejectedMessages;
                continue;
            }

            const std::string evidenceId = response.evidence().evidenceId();
            const auto pending = m_pendingRequests.find(evidenceId);
            if (pending == m_pendingRequests.end() ||
                pending->second.peerId != envelope.senderNodeId()) {
                ++result.rejectedMessages;
                continue;
            }

            const consensus::SlashingEvidenceValidationResult admitted =
                VerifiedSlashingEvidenceAdmission::admit(
                    response.evidence(),
                    currentConsensusHeight,
                    now,
                    validatorSetHistory,
                    policy,
                    provider,
                    evidencePool
                );
            m_pendingRequests.erase(pending);
            if (admitted.accepted()) {
                ++result.evidenceAccepted;
            } else if (admitted.duplicate()) {
                ++result.duplicatesSkipped;
            } else {
                ++result.rejectedMessages;
            }
        } catch (const std::exception&) {
            ++result.rejectedMessages;
        }
    }
}

void SlashingEvidenceSync::broadcastInventory(
    p2p::GossipMesh& gossip,
    const consensus::EvidencePool& evidencePool,
    std::int64_t now,
    SlashingEvidenceSyncResult& result
) {
    if (m_lastInventoryAt > 0 && now >= m_lastInventoryAt &&
        now - m_lastInventoryAt <
            core::ProtocolLimits::
                SLASHING_EVIDENCE_INVENTORY_INTERVAL_SECONDS) {
        return;
    }
    m_lastInventoryAt = now;

    const std::vector<consensus::DoubleVoteEvidence> allEvidence =
        evidencePool.allDoubleVoteEvidence();
    if (allEvidence.empty()) {
        m_inventoryOffset = 0;
        return;
    }
    if (m_inventoryOffset >= allEvidence.size()) {
        m_inventoryOffset = 0;
    }

    const std::size_t end = std::min(
        allEvidence.size(),
        m_inventoryOffset +
            core::ProtocolLimits::MAX_SLASHING_EVIDENCE_INVENTORY_IDS
    );
    std::vector<std::string> evidenceIds;
    evidenceIds.reserve(end - m_inventoryOffset);
    for (std::size_t index = m_inventoryOffset; index < end; ++index) {
        evidenceIds.push_back(allEvidence[index].evidenceId());
    }
    m_inventoryOffset = end == allEvidence.size() ? 0 : end;

    const SlashingEvidenceInventory inventory(
        gossip.config().networkId(),
        gossip.config().chainId(),
        gossip.config().localNodeId(),
        std::move(evidenceIds),
        now
    );
    if (!inventory.isValid()) return;

    gossip.broadcast(
        p2p::NetworkMessageType::SLASHING_EVIDENCE_INVENTORY,
        inventory.serialize(),
        now
    );
    ++result.inventoriesBroadcast;
}

bool SlashingEvidenceSync::consumePeerBudget(
    const std::string& peerId,
    std::int64_t now,
    bool consumesResponseBudget
) {
    pruneExpiredState(now);
    auto found = m_peerWindows.find(peerId);
    if (found == m_peerWindows.end()) {
        if (m_peerWindows.size() >=
            core::ProtocolLimits::MAX_TRACKED_SLASHING_EVIDENCE_PEERS) {
            return false;
        }
        found = m_peerWindows.emplace(
            peerId, PeerWindow{now, 0, 0}
        ).first;
    }

    PeerWindow& window = found->second;
    const std::int64_t windowSeconds = static_cast<std::int64_t>(
        core::ProtocolLimits::SLASHING_EVIDENCE_SYNC_WINDOW_SECONDS
    );
    if (now < window.startedAt ||
        now - window.startedAt >= windowSeconds) {
        window = PeerWindow{now, 0, 0};
    }
    if (window.count >=
        core::ProtocolLimits::
            MAX_SLASHING_EVIDENCE_SYNC_MESSAGES_PER_PEER_WINDOW) {
        return false;
    }
    if (consumesResponseBudget && window.responseCount >=
        core::ProtocolLimits::
            MAX_SLASHING_EVIDENCE_RESPONSES_PER_PEER_WINDOW) {
        return false;
    }
    ++window.count;
    if (consumesResponseBudget) {
        ++window.responseCount;
    }
    return true;
}

void SlashingEvidenceSync::pruneExpiredState(std::int64_t now) {
    for (auto iterator = m_pendingRequests.begin();
         iterator != m_pendingRequests.end();) {
        if (iterator->second.expiresAt <= now) {
            iterator = m_pendingRequests.erase(iterator);
        } else {
            ++iterator;
        }
    }

    const std::int64_t windowSeconds = static_cast<std::int64_t>(
        core::ProtocolLimits::SLASHING_EVIDENCE_SYNC_WINDOW_SECONDS
    );
    for (auto iterator = m_peerWindows.begin();
         iterator != m_peerWindows.end();) {
        if (now < iterator->second.startedAt ||
            now - iterator->second.startedAt >= windowSeconds) {
            iterator = m_peerWindows.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

} // namespace nodo::node
