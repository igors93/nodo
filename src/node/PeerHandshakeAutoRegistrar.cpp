#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"
#include "p2p/PeerRegistry.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace nodo::node {

// ---------------------------------------------------------------------------
// processInbox
// ---------------------------------------------------------------------------

std::vector<HandshakeRegistrationResult> PeerHandshakeAutoRegistrar::processInbox(
    p2p::GossipMesh&          gossip,
    const p2p::PeerMetadata&  localPeer,
    const ChainStatusMessage& localChainStatus,
    const crypto::KeyPair&    nodeIdentityKey,
    std::int64_t              now
) {
    std::vector<HandshakeRegistrationResult> results;
    std::unordered_set<std::string> authenticatedThisTick;
    auto& replayGuard = gossip.handshakeReplayGuard();
    replayGuard.prune(now);

    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::PEER_HELLO
    );

    for (const auto& envelope : messages) {
        HandshakeRegistrationResult result;
        result.peerId = envelope.senderNodeId();
        const std::string answeredChallenge =
            p2p::PeerHandshakeManager::challengeNonceFromHello(envelope);

        const std::optional<std::string> expectedChallenge =
            replayGuard.outstandingChallenge(result.peerId, now);
        if (!expectedChallenge.has_value()) {
            result.reason = replayGuard.wasChallengeConsumed(
                result.peerId,
                answeredChallenge,
                now
            )
                ? "Handshake validation rejected: challenge nonce was already consumed."
                : "Handshake validation rejected: no outstanding challenge for peer.";
            results.push_back(result);
            continue;
        }

        const p2p::PeerHandshakeResult validation =
            p2p::PeerHandshakeManager::validateHello(
                gossip.config(),
                envelope,
                *expectedChallenge,
                now
            );

        if (!validation.accepted()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Handshake validation rejected: " + validation.reason();
            results.push_back(result);
            continue;
        }

        if (!replayGuard.consumeChallenge(
                result.peerId,
                answeredChallenge,
                now)) {
            result.reason =
                "Handshake validation rejected: challenge was already consumed.";
            results.push_back(result);
            continue;
        }

        const std::optional<p2p::PeerMetadata> parsedPeer =
            p2p::PeerHandshakeManager::peerMetadataFromHello(envelope, now);
        if (!parsedPeer.has_value()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Could not parse PeerMetadata from PEER_HELLO payload.";
            results.push_back(result);
            continue;
        }

        const bool wasAlreadyKnown =
            gossip.peerRegistry().contains(envelope.senderNodeId());
        const p2p::PeerRegistryResult regResult =
            gossip.registerPeer(*parsedPeer);

        if (!regResult.success()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Peer registration rejected: " + regResult.reason();
            results.push_back(result);
            continue;
        }

        if (wasAlreadyKnown) {
            gossip.peerRegistry().updateHeartbeat(
                envelope.senderNodeId(),
                now
            );
        }

        // Broadcast our CHAIN_STATUS so the peer knows our height.
        gossip.broadcast(
            p2p::NetworkMessageType::CHAIN_STATUS,
            localChainStatus.serialize(),
            now
        );

        result.registered   = !wasAlreadyKnown;
        result.alreadyKnown = wasAlreadyKnown;
        result.reason       = wasAlreadyKnown
            ? "Peer cryptographic identity revalidated; heartbeat updated."
            : "Peer registered and CHAIN_STATUS sent.";
        authenticatedThisTick.insert(result.peerId);
        results.push_back(result);
    }

    const auto challenges = gossip.drainInbox(
        p2p::NetworkMessageType::PEER_CHALLENGE
    );
    for (const auto& envelope : challenges) {
        const std::optional<p2p::PeerChallengeMessage> challenge =
            p2p::PeerHandshakeManager::challengeFromEnvelope(
                gossip.config(),
                envelope,
                now
            );
        if (!challenge.has_value()) {
            continue;
        }

        const p2p::NetworkEnvelope hello =
            p2p::PeerHandshakeManager::createHelloEnvelope(
                gossip.config(),
                localPeer,
                localChainStatus,
                challenge->challengerNodeId(),
                challenge->nonce(),
                nodeIdentityKey,
                now
            );
        const p2p::GossipDeliveryReport response = gossip.sendHandshakeTo(
            challenge->challengerNodeId(),
            p2p::NetworkMessageType::PEER_HELLO,
            hello.payload(),
            now
        );
        if (!response.allAccepted()) continue;

        if (authenticatedThisTick.find(challenge->challengerNodeId()) ==
                authenticatedThisTick.end() &&
            !replayGuard.outstandingChallenge(
                challenge->challengerNodeId(), now).has_value()) {
            (void)initiateHandshake(
                gossip,
                challenge->challengerNodeId(),
                now
            );
        }
    }

    return results;
}

p2p::GossipDeliveryReport PeerHandshakeAutoRegistrar::initiateHandshake(
    p2p::GossipMesh&   gossip,
    const std::string& targetNodeId,
    std::int64_t       now
) {
    const std::optional<std::string> nonce =
        gossip.handshakeReplayGuard().issueChallenge(
            targetNodeId,
            now,
            gossip.config().defaultTtlSeconds()
        );
    if (!nonce.has_value()) {
        return p2p::GossipDeliveryReport(0, 1);
    }

    try {
        const p2p::NetworkEnvelope challenge =
            p2p::PeerHandshakeManager::createChallengeEnvelope(
            gossip.config(),
            targetNodeId,
            *nonce,
            now
        );
        const p2p::GossipDeliveryReport sent = gossip.sendHandshakeTo(
            targetNodeId,
            p2p::NetworkMessageType::PEER_CHALLENGE,
            challenge.payload(),
            now
        );
        if (!sent.allAccepted()) {
            (void)gossip.handshakeReplayGuard().discardChallenge(
                targetNodeId,
                *nonce
            );
        }
        return sent;
    } catch (const std::exception&) {
        (void)gossip.handshakeReplayGuard().discardChallenge(
            targetNodeId,
            *nonce
        );
        return p2p::GossipDeliveryReport(0, 1);
    }
}

} // namespace nodo::node
