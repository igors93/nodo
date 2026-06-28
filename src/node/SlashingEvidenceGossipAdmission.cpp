#include "node/SlashingEvidenceGossipAdmission.hpp"

#include "core/ProtocolLimits.hpp"
#include "node/SlashingEvidenceMessages.hpp"

#include <cstdlib>
#include <exception>
#include <utility>

namespace nodo::node {

SlashingEvidenceGossipResult::SlashingEvidenceGossipResult(
    SlashingEvidenceGossipStatus status,
    std::string reason,
    std::string evidenceId
) : m_status(status),
    m_reason(std::move(reason)),
    m_evidenceId(std::move(evidenceId)) {}

SlashingEvidenceGossipStatus SlashingEvidenceGossipResult::status() const {
    return m_status;
}

const std::string& SlashingEvidenceGossipResult::reason() const {
    return m_reason;
}

const std::string& SlashingEvidenceGossipResult::evidenceId() const {
    return m_evidenceId;
}

bool SlashingEvidenceGossipResult::accepted() const {
    return m_status == SlashingEvidenceGossipStatus::ACCEPTED;
}

bool SlashingEvidenceGossipResult::duplicate() const {
    return m_status == SlashingEvidenceGossipStatus::DUPLICATE;
}

bool SlashingEvidenceGossipResult::rateLimited() const {
    return m_status == SlashingEvidenceGossipStatus::RATE_LIMITED;
}

SlashingEvidenceGossipResult SlashingEvidenceGossipAdmission::admit(
    const p2p::NetworkEnvelope& envelope,
    const std::string& expectedNetworkId,
    const std::string& expectedChainId,
    std::uint64_t currentConsensusHeight,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::EvidencePool& evidencePool,
    std::int64_t now
) {
    using namespace core::ProtocolLimits;

    if (now <= 0 || envelope.messageType() !=
            p2p::NetworkMessageType::SLASHING_EVIDENCE_ANNOUNCE ||
        !envelope.isStructurallyValid(MAX_NETWORK_PAYLOAD_BYTES) ||
        envelope.networkId() != expectedNetworkId ||
        envelope.chainId() != expectedChainId) {
        return {
            SlashingEvidenceGossipStatus::REJECTED,
            "Slashing evidence envelope is invalid for this chain."
        };
    }

    if (!consumePeerBudget(envelope.senderNodeId(), now)) {
        return {
            SlashingEvidenceGossipStatus::RATE_LIMITED,
            "Peer exceeded the slashing evidence gossip window."
        };
    }

    if (envelope.payload().empty() ||
        envelope.payload().size() > MAX_SLASHING_EVIDENCE_GOSSIP_BYTES) {
        return {
            SlashingEvidenceGossipStatus::REJECTED,
            "Slashing evidence payload exceeds its protocol limit."
        };
    }

    try {
        const SlashingEvidenceAnnouncement announcement =
            SlashingEvidenceAnnouncement::deserialize(envelope.payload());
        if (announcement.networkId() != expectedNetworkId ||
            announcement.chainId() != expectedChainId ||
            announcement.announcerNodeId() != envelope.senderNodeId() ||
            announcement.announcedAt() != envelope.createdAt() ||
            std::llabs(now - announcement.announcedAt()) >
                MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                "Slashing evidence announcement is stale or not bound to its envelope."
            };
        }

        const consensus::DoubleVoteEvidence& evidence =
            announcement.evidence();
        const std::string evidenceId = evidence.evidenceId();
        const std::uint64_t offenseHeight =
            evidence.firstVote().blockIndex();

        if (evidence.detectedAt() > announcement.announcedAt() ||
            evidence.firstVote().createdAt() >
                announcement.announcedAt() +
                    MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS ||
            evidence.secondVote().createdAt() >
                announcement.announcedAt() +
                    MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS ||
            offenseHeight == 0 || offenseHeight > currentConsensusHeight) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                "Slashing evidence has an invalid height or timestamp.",
                evidenceId
            };
        }

        if (evidencePool.contains(evidenceId)) {
            return {
                SlashingEvidenceGossipStatus::DUPLICATE,
                "Slashing evidence is already known.",
                evidenceId
            };
        }

        if (!validatorSetHistory.hasSet(offenseHeight)) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                "Historical validator set is unavailable for the evidence.",
                evidenceId
            };
        }

        const core::ValidatorRegistry& historicalValidators =
            validatorSetHistory.setAt(offenseHeight);
        if (!historicalValidators.verifyValidatorIdentity(
                evidence.validatorAddress(),
                evidence.firstVote().validatorPublicKey()) ||
            !historicalValidators.verifyValidatorIdentity(
                evidence.validatorAddress(),
                evidence.secondVote().validatorPublicKey())) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                "Evidence signer was not active at the offense height.",
                evidenceId
            };
        }

        const consensus::SlashingEvidenceValidationResult verified =
            consensus::SlashingEvidenceVerifier::verifyDoubleVoteEvidence(
                evidence, policy, provider
            );
        if (!verified.accepted()) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                "Slashing evidence signature verification failed: " +
                    verified.reason(),
                evidenceId
            };
        }

        const consensus::DoubleVoteEvidence locallyTimestampedEvidence(
            evidence.firstVote(), evidence.secondVote(), now
        );
        const consensus::SlashingEvidenceValidationResult stored =
            evidencePool.submitDoubleVoteEvidence(locallyTimestampedEvidence);
        if (stored.duplicate()) {
            return {
                SlashingEvidenceGossipStatus::DUPLICATE,
                "Slashing evidence is already known.",
                evidenceId
            };
        }
        if (!stored.accepted()) {
            return {
                SlashingEvidenceGossipStatus::REJECTED,
                stored.reason(),
                evidenceId
            };
        }

        return {
            SlashingEvidenceGossipStatus::ACCEPTED,
            "Verified slashing evidence accepted from gossip.",
            evidenceId
        };
    } catch (const std::exception& error) {
        return {
            SlashingEvidenceGossipStatus::REJECTED,
            std::string("Malformed slashing evidence announcement: ") +
                error.what()
        };
    }
}

std::size_t SlashingEvidenceGossipAdmission::trackedPeerCount() const {
    return m_peerWindows.size();
}

bool SlashingEvidenceGossipAdmission::consumePeerBudget(
    const std::string& peerId,
    std::int64_t now
) {
    using namespace core::ProtocolLimits;
    pruneExpiredPeerWindows(now);

    auto found = m_peerWindows.find(peerId);
    if (found == m_peerWindows.end()) {
        if (m_peerWindows.size() >= MAX_TRACKED_SLASHING_EVIDENCE_PEERS) {
            return false;
        }
        found = m_peerWindows.emplace(
            peerId, PeerWindow{now, 0}
        ).first;
    }

    PeerWindow& window = found->second;
    if (now < window.startedAt ||
        now - window.startedAt >=
            static_cast<std::int64_t>(
                SLASHING_EVIDENCE_PEER_WINDOW_SECONDS)) {
        window = PeerWindow{now, 0};
    }
    if (window.count >= MAX_SLASHING_EVIDENCE_PER_PEER_WINDOW) {
        return false;
    }
    ++window.count;
    return true;
}

void SlashingEvidenceGossipAdmission::pruneExpiredPeerWindows(
    std::int64_t now
) {
    const std::int64_t windowSeconds = static_cast<std::int64_t>(
        core::ProtocolLimits::SLASHING_EVIDENCE_PEER_WINDOW_SECONDS
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
