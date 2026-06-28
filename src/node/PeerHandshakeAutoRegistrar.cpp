#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"
#include "p2p/PeerRegistry.hpp"

#include <optional>
#include <string>

namespace nodo::node {

// ---------------------------------------------------------------------------
// processInbox
// ---------------------------------------------------------------------------

std::vector<HandshakeRegistrationResult> PeerHandshakeAutoRegistrar::processInbox(
    p2p::GossipMesh&          gossip,
    const ChainStatusMessage& localChainStatus,
    std::int64_t              now
) {
    std::vector<HandshakeRegistrationResult> results;

    const auto messages = gossip.drainInbox(
        p2p::NetworkMessageType::PEER_HELLO
    );

    for (const auto& envelope : messages) {
        HandshakeRegistrationResult result;
        result.peerId = envelope.senderNodeId();

        // Validate the hello envelope (network, chain, genesis, expiry).
        const p2p::PeerHandshakeResult validation =
            p2p::PeerHandshakeManager::validateHello(gossip.config(), envelope, now);

        if (!validation.accepted()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Handshake validation rejected: " + validation.reason();
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
        results.push_back(result);
    }

    return results;
}

// ---------------------------------------------------------------------------
// sendHello
// ---------------------------------------------------------------------------

p2p::GossipDeliveryReport PeerHandshakeAutoRegistrar::sendHello(
    p2p::GossipMesh&          gossip,
    const p2p::PeerMetadata&  localPeer,
    const ChainStatusMessage& localChainStatus,
    const crypto::KeyPair&    nodeIdentityKey,
    std::int64_t              now
) {
    // Build the PEER_HELLO envelope using the canonical factory.
    const p2p::NetworkEnvelope helloEnvelope =
        p2p::PeerHandshakeManager::createHelloEnvelope(
            gossip.config(),
            localPeer,
            localChainStatus,
            nodeIdentityKey,
            now
        );

    // Broadcast its payload to all connected peers.
    return gossip.broadcast(
        p2p::NetworkMessageType::PEER_HELLO,
        helloEnvelope.payload(),
        now
    );
}

p2p::GossipDeliveryReport PeerHandshakeAutoRegistrar::sendHelloTo(
    p2p::GossipMesh&          gossip,
    const std::string&        targetNodeId,
    const p2p::PeerMetadata&  localPeer,
    const ChainStatusMessage& localChainStatus,
    const crypto::KeyPair&    nodeIdentityKey,
    std::int64_t              now
) {
    const p2p::NetworkEnvelope helloEnvelope =
        p2p::PeerHandshakeManager::createHelloEnvelope(
            gossip.config(),
            localPeer,
            localChainStatus,
            nodeIdentityKey,
            now
        );
    return gossip.sendHandshakeTo(
        targetNodeId,
        helloEnvelope.payload(),
        now
    );
}

} // namespace nodo::node
