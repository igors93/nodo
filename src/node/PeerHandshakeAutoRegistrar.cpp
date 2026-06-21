#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"
#include "p2p/PeerRegistry.hpp"

#include <sstream>
#include <string>

namespace nodo::node {

namespace {

// ---------------------------------------------------------------------------
// Minimal payload parser for PeerHelloMessage serialization.
//
// Serialized form (from PeerHandshakeManager.cpp):
//   PeerHelloMessage{peer=PeerMetadata{nodeId=<id>;endpoint=PeerEndpoint{host=<h>;port=<p>};publicKeyFingerprint=<fp>;firstSeenAt=<t>;lastSeenAt=<t>;score=<s>;quarantined=<b>};networkId=...;...}
//
// We extract only the fields needed to reconstruct a PeerMetadata for
// registration. Nested brace groups are handled depth-aware so that ';'
// inside nested blocks is not treated as a field delimiter.
// ---------------------------------------------------------------------------

// Extracts the value of `key=<value>` from `text`.
// Brace-depth tracking means the value can itself contain nested {…} blocks.
std::string extractNestedField(const std::string& text, const std::string& key) {
    const std::string prefix = key + "=";
    const auto pos = text.find(prefix);
    if (pos == std::string::npos) {
        return "";
    }
    const auto valueStart = pos + prefix.size();
    std::size_t depth = 0;
    std::size_t i = valueStart;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            if (depth == 0) break;
            --depth;
        } else if (c == ';' && depth == 0) {
            break;
        }
        ++i;
    }
    return text.substr(valueStart, i - valueStart);
}

std::uint16_t safeParsePort(const std::string& value) {
    if (value.empty()) return 0;
    try {
        const unsigned long p = std::stoul(value);
        if (p == 0 || p > 65535) return 0;
        return static_cast<std::uint16_t>(p);
    } catch (...) {
        return 0;
    }
}

std::int64_t safeParseInt64(const std::string& value, std::int64_t fallback = 0) {
    if (value.empty()) return fallback;
    try {
        return static_cast<std::int64_t>(std::stoll(value));
    } catch (...) {
        return fallback;
    }
}

// Attempts to parse a PeerMetadata from a PeerHelloMessage payload string.
// Returns a default-constructed (invalid) PeerMetadata on parse failure.
p2p::PeerMetadata parsePeerFromHelloPayload(
    const std::string& payload,
    std::int64_t       now
) {
    // Extract the peer=PeerMetadata{...} block.
    const std::string peerBlock = extractNestedField(payload, "peer");
    if (peerBlock.rfind("PeerMetadata{", 0) != 0) {
        return {};
    }

    const std::string nodeId              = extractNestedField(peerBlock, "nodeId");
    const std::string endpointBlock       = extractNestedField(peerBlock, "endpoint");
    const std::string publicKeyFingerprint = extractNestedField(peerBlock, "publicKeyFingerprint");

    std::string host;
    std::uint16_t port = 0;

    if (endpointBlock.rfind("PeerEndpoint{", 0) == 0) {
        host = extractNestedField(endpointBlock, "host");
        port = safeParsePort(extractNestedField(endpointBlock, "port"));
    }

    std::int64_t firstSeenAt = safeParseInt64(
        extractNestedField(peerBlock, "firstSeenAt"), now
    );
    std::int64_t lastSeenAt = safeParseInt64(
        extractNestedField(peerBlock, "lastSeenAt"), now
    );

    // Clamp: reject obviously invalid or future timestamps.
    // Only overwrite when out of range; valid timestamps pass through unchanged.
    static constexpr std::int64_t TIMESTAMP_TOLERANCE_SECONDS = 300; // 5 minutes
    if (firstSeenAt <= 0 || firstSeenAt > now + TIMESTAMP_TOLERANCE_SECONDS) {
        firstSeenAt = now;
    }
    if (lastSeenAt <= 0 || lastSeenAt > now + TIMESTAMP_TOLERANCE_SECONDS) {
        lastSeenAt = now;
    }
    if (lastSeenAt < firstSeenAt) lastSeenAt = firstSeenAt;

    return p2p::PeerMetadata(
        nodeId,
        p2p::PeerEndpoint(host, port),
        publicKeyFingerprint,
        firstSeenAt,
        lastSeenAt,
        0,     // score starts neutral
        false  // not quarantined
    );
}

} // namespace

// ---------------------------------------------------------------------------
// processInbox
// ---------------------------------------------------------------------------

std::vector<HandshakeRegistrationResult> PeerHandshakeAutoRegistrar::processInbox(
    p2p::GossipMesh&          gossip,
    const ChainStatusMessage& localChainStatus,
    std::int64_t              now
) {
    std::vector<HandshakeRegistrationResult> results;

    const auto messages = gossip.inbox().messagesForType(
        p2p::NetworkMessageType::PEER_HELLO
    );

    for (const auto& envelope : messages) {
        HandshakeRegistrationResult result;
        result.peerId = envelope.senderNodeId();

        // Step 1: validate the hello envelope (network, chain, genesis, expiry).
        const p2p::PeerHandshakeResult validation =
            p2p::PeerHandshakeManager::validateHello(gossip.config(), envelope, now);

        if (!validation.accepted()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Handshake validation rejected: " + validation.reason();
            results.push_back(result);
            continue;
        }

        // Step 2: check if peer is already known.
        if (gossip.peerRegistry().contains(envelope.senderNodeId())) {
            // Heartbeat update only — treat as already known.
            gossip.peerRegistry().updateHeartbeat(envelope.senderNodeId(), now);
            result.registered   = false;
            result.alreadyKnown = true;
            result.reason       = "Peer already registered; heartbeat updated.";
            results.push_back(result);
            // Still reply with our chain status so the peer can sync if needed.
            gossip.broadcast(
                p2p::NetworkMessageType::CHAIN_STATUS,
                localChainStatus.serialize(),
                now
            );
            continue;
        }

        // Step 3: parse PeerMetadata from the hello payload.
        p2p::PeerMetadata peer = parsePeerFromHelloPayload(envelope.payload(), now);

        // If parsing yielded an invalid PeerMetadata, fall back to a minimal
        // registration using only the sender node id from the envelope.
        // The endpoint/fingerprint will be empty but we still record the peer.
        if (!peer.isValid()) {
            // Best-effort minimal peer: we know the nodeId from the envelope.
            // Endpoint and fingerprint are unknown — register without them.
            // The peer will be invalid by PeerMetadata::isValid() standards,
            // so skip registration and report the failure.
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Could not parse PeerMetadata from PEER_HELLO payload.";
            results.push_back(result);
            continue;
        }

        // Step 4: register peer.
        const p2p::PeerRegistryResult regResult = gossip.registerPeer(peer);

        if (!regResult.success()) {
            result.registered   = false;
            result.alreadyKnown = false;
            result.reason       = "Peer registration rejected: " + regResult.reason();
            results.push_back(result);
            continue;
        }

        // Step 5: broadcast our CHAIN_STATUS so the peer knows our height.
        gossip.broadcast(
            p2p::NetworkMessageType::CHAIN_STATUS,
            localChainStatus.serialize(),
            now
        );

        result.registered   = true;
        result.alreadyKnown = false;
        result.reason       = "Peer registered and CHAIN_STATUS sent.";
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
    std::int64_t              now
) {
    // Build the PEER_HELLO envelope using the canonical factory.
    const p2p::NetworkEnvelope helloEnvelope =
        p2p::PeerHandshakeManager::createHelloEnvelope(
            gossip.config(),
            localPeer,
            localChainStatus,
            now
        );

    // Broadcast its payload to all connected peers.
    return gossip.broadcast(
        p2p::NetworkMessageType::PEER_HELLO,
        helloEnvelope.payload(),
        now
    );
}

} // namespace nodo::node
