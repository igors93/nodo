#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "node/ChainStatusGossipCodec.hpp"

#include "p2p/AuthenticatedSessionTransport.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/PeerHandshakeManager.hpp"
#include "p2p/PeerRegistry.hpp"
#include "p2p/PeerSessionKeyAgreement.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace nodo::node {

namespace {

std::string encodedChainStatus(const ChainStatusMessage &status) {
  return ChainStatusGossipCodec::encode(status);
}

} // namespace

// ---------------------------------------------------------------------------
// processInbox
// ---------------------------------------------------------------------------

std::vector<HandshakeRegistrationResult>
PeerHandshakeAutoRegistrar::processInbox(
    p2p::GossipMesh &gossip, const p2p::PeerMetadata &localPeer,
    const ChainStatusMessage &localChainStatus,
    const crypto::KeyPair &nodeIdentityKey, std::int64_t now) {
  std::vector<HandshakeRegistrationResult> results;
  std::unordered_set<std::string> authenticatedThisTick;
  std::unordered_set<std::string> reregisteredThisTick;
  auto *authenticatedTransport =
      dynamic_cast<p2p::AuthenticatedSessionTransport *>(&gossip.transport());
  const auto rejectPendingPeer = [&](const std::string &peerId) {
    if (authenticatedTransport != nullptr) {
      (void)authenticatedTransport->rejectPendingConnection(
          gossip.config().localNodeId(), peerId);
    }
  };
  auto &replayGuard = gossip.handshakeReplayGuard();
  replayGuard.prune(now);

  const auto messages = gossip.drainInbox(p2p::NetworkMessageType::PEER_HELLO);

  for (const auto &envelope : messages) {
    (void)gossip.takeConnectionIdForEnvelope(envelope);
    HandshakeRegistrationResult result;
    result.peerId = envelope.senderNodeId();
    const std::string answeredChallenge =
        p2p::PeerHandshakeManager::challengeNonceFromHello(envelope);

    const auto expectedChallenge =
        replayGuard.outstandingChallengeMaterial(result.peerId, now);
    if (!expectedChallenge.has_value()) {
      rejectPendingPeer(result.peerId);
      result.reason = replayGuard.wasChallengeConsumed(result.peerId,
                                                       answeredChallenge, now)
                          ? "Handshake validation rejected: challenge nonce "
                            "was already consumed."
                          : "Handshake validation rejected: no outstanding "
                            "challenge for peer.";
      gossip.reportPeerMisbehavior(envelope,
                                   p2p::PeerMisbehaviorType::INVALID_MESSAGE,
                                   result.reason, now);
      results.push_back(result);
      continue;
    }

    const p2p::PeerHandshakeResult validation =
        p2p::PeerHandshakeManager::validateHello(
            gossip.config(), envelope, expectedChallenge->nonce,
            expectedChallenge->ephemeralPublicKeyHex, now);

    if (!validation.accepted()) {
      rejectPendingPeer(result.peerId);
      result.registered = false;
      result.alreadyKnown = false;
      result.reason = "Handshake validation rejected: " + validation.reason();
      gossip.reportPeerMisbehavior(envelope,
                                   p2p::PeerMisbehaviorType::INVALID_MESSAGE,
                                   result.reason, now);
      results.push_back(result);
      continue;
    }

    std::optional<std::string> inboundSessionSecret;
    if (authenticatedTransport != nullptr) {
      const p2p::PeerSessionContext context{
          gossip.config().networkId(),
          gossip.config().chainId(),
          gossip.config().protocolVersion(),
          gossip.config().localNodeId(),
          result.peerId,
          expectedChallenge->nonce,
          expectedChallenge->ephemeralPublicKeyHex,
          p2p::PeerHandshakeManager::ephemeralPublicKeyFromHello(envelope)};
      inboundSessionSecret = p2p::PeerSessionKeyAgreement::deriveSessionSecret(
          expectedChallenge->ephemeralPrivateKeyHex,
          context.challengedEphemeralPublicKeyHex, context);
      if (!inboundSessionSecret.has_value()) {
        rejectPendingPeer(result.peerId);
        result.reason =
            "Handshake validation rejected: session key agreement failed.";
        results.push_back(result);
        continue;
      }
    }

    if (!replayGuard.consumeChallenge(result.peerId, answeredChallenge, now)) {
      rejectPendingPeer(result.peerId);
      result.reason =
          "Handshake validation rejected: challenge was already consumed.";
      results.push_back(result);
      continue;
    }

    const std::optional<p2p::PeerMetadata> parsedPeer =
        p2p::PeerHandshakeManager::peerMetadataFromHello(envelope, now);
    if (!parsedPeer.has_value()) {
      rejectPendingPeer(result.peerId);
      result.registered = false;
      result.alreadyKnown = false;
      result.reason = "Could not parse PeerMetadata from PEER_HELLO payload.";
      results.push_back(result);
      continue;
    }

    const bool wasAlreadyKnown =
        gossip.peerRegistry().contains(envelope.senderNodeId());
    const p2p::PeerRegistryResult regResult = gossip.registerPeer(*parsedPeer);

    if (!regResult.success()) {
      rejectPendingPeer(result.peerId);
      result.registered = false;
      result.alreadyKnown = false;
      result.reason = "Peer registration rejected: " + regResult.reason();
      results.push_back(result);
      continue;
    }

    if (authenticatedTransport != nullptr &&
        !authenticatedTransport->establishInboundSession(
            gossip.config().localNodeId(), result.peerId, *inboundSessionSecret,
            now)) {
      rejectPendingPeer(result.peerId);
      result.registered = false;
      result.alreadyKnown = false;
      result.reason =
          "Peer registration rejected: inbound secure session failed.";
      results.push_back(result);
      continue;
    }

    if (wasAlreadyKnown) {
      gossip.peerRegistry().updateHeartbeat(envelope.senderNodeId(), now);
      reregisteredThisTick.insert(result.peerId);
    }

    if (authenticatedTransport == nullptr) {
      gossip.sendTo(result.peerId, p2p::NetworkMessageType::CHAIN_STATUS,
                    encodedChainStatus(localChainStatus), now);
    } else if (authenticatedTransport->activateOutboundSession(
                   gossip.config().localNodeId(), result.peerId)) {
      gossip.sendTo(result.peerId, p2p::NetworkMessageType::CHAIN_STATUS,
                    encodedChainStatus(localChainStatus), now);
    }

    result.registered = !wasAlreadyKnown;
    result.alreadyKnown = wasAlreadyKnown;
    result.reason =
        wasAlreadyKnown
            ? "Peer cryptographic identity revalidated; heartbeat updated."
            : "Peer registered and CHAIN_STATUS sent.";
    authenticatedThisTick.insert(result.peerId);
    results.push_back(result);
  }

  const auto challenges =
      gossip.drainInbox(p2p::NetworkMessageType::PEER_CHALLENGE);
  for (const auto &envelope : challenges) {
    const p2p::TransportConnectionId connectionId =
        gossip.takeConnectionIdForEnvelope(envelope);
    const std::optional<p2p::PeerChallengeMessage> challenge =
        p2p::PeerHandshakeManager::challengeFromEnvelope(gossip.config(),
                                                         envelope, now);
    if (!challenge.has_value()) {
      continue;
    }

    const auto responseKeyPair =
        p2p::PeerSessionKeyAgreement::generateEphemeralKeyPair();
    if (!responseKeyPair.has_value())
      continue;

    std::optional<std::string> outboundSessionSecret;
    if (authenticatedTransport != nullptr) {
      const p2p::PeerSessionContext context{
          gossip.config().networkId(),        gossip.config().chainId(),
          gossip.config().protocolVersion(),  challenge->challengerNodeId(),
          gossip.config().localNodeId(),      challenge->nonce(),
          challenge->ephemeralPublicKeyHex(), responseKeyPair->publicKeyHex};
      outboundSessionSecret = p2p::PeerSessionKeyAgreement::deriveSessionSecret(
          responseKeyPair->privateKeyHex, challenge->ephemeralPublicKeyHex(),
          context);
      if (!outboundSessionSecret.has_value() ||
          !authenticatedTransport->stageOutboundSession(
              gossip.config().localNodeId(), challenge->challengerNodeId(),
              *outboundSessionSecret, now)) {
        continue;
      }
    }

    const p2p::NetworkEnvelope hello =
        p2p::PeerHandshakeManager::createHelloEnvelope(
            gossip.config(), localPeer, localChainStatus,
            challenge->challengerNodeId(), challenge->nonce(),
            challenge->ephemeralPublicKeyHex(), responseKeyPair->publicKeyHex,
            nodeIdentityKey, now);
    const p2p::GossipDeliveryReport response = gossip.sendHandshakeTo(
        challenge->challengerNodeId(), p2p::NetworkMessageType::PEER_HELLO,
        hello.payload(), now, connectionId);
    if (!response.allAccepted()) {
      if (authenticatedTransport != nullptr) {
        authenticatedTransport->removeSession(gossip.config().localNodeId(),
                                              challenge->challengerNodeId());
      }
      continue;
    }

    if (authenticatedTransport != nullptr &&
        authenticatedThisTick.find(challenge->challengerNodeId()) !=
            authenticatedThisTick.end() &&
        authenticatedTransport->activateOutboundSession(
            gossip.config().localNodeId(), challenge->challengerNodeId())) {
      if (reregisteredThisTick.count(challenge->challengerNodeId()) > 0) {
        gossip.sendTo(challenge->challengerNodeId(),
                      p2p::NetworkMessageType::CHAIN_STATUS,
                      encodedChainStatus(localChainStatus), now);
      }
    }

    if (authenticatedThisTick.find(challenge->challengerNodeId()) ==
            authenticatedThisTick.end() &&
        !replayGuard.outstandingChallenge(challenge->challengerNodeId(), now)
             .has_value()) {
      (void)initiateHandshake(gossip, challenge->challengerNodeId(), now,
                              connectionId);
    }
  }

  return results;
}

p2p::GossipDeliveryReport PeerHandshakeAutoRegistrar::initiateHandshake(
    p2p::GossipMesh &gossip, const std::string &targetNodeId, std::int64_t now,
    p2p::TransportConnectionId connectionId) {
  const auto challengeMaterial =
      gossip.handshakeReplayGuard().issueChallengeMaterial(
          targetNodeId, now, gossip.config().defaultTtlSeconds());
  if (!challengeMaterial.has_value()) {
    return p2p::GossipDeliveryReport(0, 1);
  }

  try {
    const p2p::NetworkEnvelope challenge =
        p2p::PeerHandshakeManager::createChallengeEnvelope(
            gossip.config(), targetNodeId, challengeMaterial->nonce,
            challengeMaterial->ephemeralPublicKeyHex, now);
    const p2p::GossipDeliveryReport sent = gossip.sendHandshakeTo(
        targetNodeId, p2p::NetworkMessageType::PEER_CHALLENGE,
        challenge.payload(), now, connectionId);
    if (!sent.allAccepted()) {
      (void)gossip.handshakeReplayGuard().discardChallenge(
          targetNodeId, challengeMaterial->nonce);
    }
    return sent;
  } catch (const std::exception &) {
    (void)gossip.handshakeReplayGuard().discardChallenge(
        targetNodeId, challengeMaterial->nonce);
    return p2p::GossipDeliveryReport(0, 1);
  }
}

} // namespace nodo::node
