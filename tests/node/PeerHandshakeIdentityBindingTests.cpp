#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "crypto/KeyPair.hpp"
#include "p2p/LoopbackTransport.hpp"
#include "p2p/PeerHandshakeManager.hpp"
#include "p2p/PeerSessionKeyAgreement.hpp"

#include <cassert>
#include <memory>
#include <string>

namespace {

using namespace nodo;

p2p::PeerMetadata peer(const std::string &nodeId,
                       const crypto::KeyPair &identity, std::int64_t now) {
  return p2p::PeerMetadata(nodeId, p2p::PeerEndpoint("127.0.0.1", 19002),
                           identity.publicKey().fingerprint(), now, now, 0,
                           false);
}

node::ChainStatusMessage chainStatus() {
  return node::ChainStatusMessage("localnet", "chain-localnet", "1", 10,
                                  "block-10", 10, "block-10");
}

p2p::NetworkEnvelope deliverHello(p2p::LoopbackTransport &remoteTransport,
                                  p2p::GossipMesh &localMesh,
                                  const p2p::GossipMeshConfig &remoteConfig,
                                  const p2p::PeerMetadata &remotePeer,
                                  const crypto::KeyPair &remoteIdentity,
                                  std::int64_t now) {
  p2p::GossipMesh remoteMesh(remoteConfig, remoteTransport);
  assert(remoteTransport
             .connect(remotePeer.nodeId(), localMesh.config().localNodeId())
             .success());
  const auto challenge =
      localMesh.handshakeReplayGuard().issueChallengeMaterial(
          remotePeer.nodeId(), now, localMesh.config().defaultTtlSeconds());
  assert(challenge.has_value());
  const auto responseKey =
      p2p::PeerSessionKeyAgreement::generateEphemeralKeyPair();
  assert(responseKey.has_value());
  const p2p::NetworkEnvelope hello =
      p2p::PeerHandshakeManager::createHelloEnvelope(
          remoteConfig, remotePeer, chainStatus(),
          localMesh.config().localNodeId(), challenge->nonce,
          challenge->ephemeralPublicKeyHex, responseKey->publicKeyHex,
          remoteIdentity, now);
  assert(remoteMesh
             .sendHandshakeTo(localMesh.config().localNodeId(),
                              p2p::NetworkMessageType::PEER_HELLO,
                              hello.payload(), now)
             .allAccepted());
  assert(localMesh.receiveAvailable(now).acceptedCount() == 1);
  return hello;
}

} // namespace

int main() {
  auto bus = std::make_shared<p2p::LoopbackTransportBus>();
  p2p::LoopbackTransport localTransport(bus);
  p2p::LoopbackTransport remoteTransport(bus);
  p2p::GossipMesh localMesh(p2p::GossipMeshConfig("node-local", "localnet",
                                                  "chain-localnet", "1",
                                                  "genesis-localnet", 60, 3),
                            localTransport);
  const crypto::KeyPair localIdentity =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-identity-binding-local");
  const p2p::PeerMetadata localPeer = peer("node-local", localIdentity, 1000);

  const p2p::GossipMeshConfig originalConfig("node-remote", "localnet",
                                             "chain-localnet", "1",
                                             "genesis-localnet", 60, 3);
  const crypto::KeyPair originalIdentity =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-identity-binding-original");
  const crypto::KeyPair attackerIdentity =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-identity-binding-attacker");
  const p2p::PeerMetadata original =
      peer("node-remote", originalIdentity, 1000);
  const p2p::NetworkEnvelope originalHello =
      deliverHello(remoteTransport, localMesh, originalConfig, original,
                   originalIdentity, 1000);
  const auto registered = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1000);
  assert(registered.size() == 1);
  assert(registered[0].registered);

  const p2p::NetworkEnvelope replayWrappedHello(
      originalHello.networkId(), originalHello.chainId(),
      originalHello.protocolVersion(), originalHello.messageType(),
      originalHello.senderNodeId(), originalHello.createdAt(),
      originalHello.ttlSeconds() - 1, originalHello.payload());
  assert(remoteTransport
             .send(p2p::TransportMessage("node-remote", "node-local",
                                         replayWrappedHello, 1000))
             .success());
  assert(localMesh.receiveAvailable(1000).acceptedCount() == 1);
  const auto replayed = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1000);
  assert(replayed.size() == 1);
  assert(!replayed[0].registered);
  assert(replayed[0].reason.find("already consumed") != std::string::npos);
  // Replaying a consumed challenge must feed the same misbehavior-tracking
  // pipeline used for other P2P abuse (rate limiting, invalid gossip), not
  // just fail silently.
  assert(localMesh.invalidMessageCountForPeer("node-remote") == 1);

  const p2p::PeerMetadata refreshed =
      peer("node-remote", originalIdentity, 1001);
  deliverHello(remoteTransport, localMesh, originalConfig, refreshed,
               originalIdentity, 1001);
  const auto refreshedResult = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1001);
  assert(refreshedResult.size() == 1);
  assert(!refreshedResult[0].registered);
  assert(refreshedResult[0].alreadyKnown);

  const p2p::PeerMetadata takeover =
      peer("node-remote", attackerIdentity, 1002);
  deliverHello(remoteTransport, localMesh, originalConfig, takeover,
               attackerIdentity, 1002);
  const auto takeoverResult = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1002);
  assert(takeoverResult.size() == 1);
  assert(!takeoverResult[0].registered);
  assert(!takeoverResult[0].alreadyKnown);
  assert(localMesh.peerRegistry().peer("node-remote") != nullptr);
  assert(localMesh.peerRegistry().peer("node-remote")->publicKeyFingerprint() ==
         originalIdentity.publicKey().fingerprint());

  const p2p::GossipMeshConfig aliasConfig("node-remote-rotated", "localnet",
                                          "chain-localnet", "1",
                                          "genesis-localnet", 60, 3);
  const p2p::PeerMetadata alias =
      peer("node-remote-rotated", originalIdentity, 1003);
  deliverHello(remoteTransport, localMesh, aliasConfig, alias, originalIdentity,
               1003);
  const auto aliasResult = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1003);
  assert(aliasResult.size() == 1);
  assert(!aliasResult[0].registered);
  assert(!localMesh.peerRegistry().contains("node-remote-rotated"));
  assert(localMesh.peerRegistry().size() == 1);

  const p2p::NetworkEnvelope bypassAttempt(
      "localnet", "chain-localnet", "1", p2p::NetworkMessageType::PING,
      "node-remote-rotated", 1004, 60, "ping");
  assert(remoteTransport
             .send(p2p::TransportMessage("node-remote-rotated", "node-local",
                                         bypassAttempt, 1004))
             .success());
  const p2p::GossipDeliveryReport bypassDelivery =
      localMesh.receiveAvailable(1004);
  assert(bypassDelivery.acceptedCount() == 0);
  assert(bypassDelivery.rejectedCount() == 1);
  assert(localMesh.inbox().countForType(p2p::NetworkMessageType::PING) == 0);

  // Tampered signature: the hello payload is mutated after signing, so
  // Ed25519 verification fails. Validation must reject it and the
  // rejection must feed the misbehavior-tracking pipeline, matching the
  // evidentiary trail already recorded for other P2P abuse.
  const std::string forgerNodeId = "node-forger";
  const crypto::KeyPair forgerIdentity =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-identity-binding-forger");
  const p2p::PeerMetadata forgerPeer = peer(forgerNodeId, forgerIdentity, 1005);
  const p2p::GossipMeshConfig forgerConfig(forgerNodeId, "localnet",
                                           "chain-localnet", "1",
                                           "genesis-localnet", 60, 3);
  const auto forgerChallenge =
      localMesh.handshakeReplayGuard().issueChallengeMaterial(
          forgerNodeId, 1005, localMesh.config().defaultTtlSeconds());
  assert(forgerChallenge.has_value());
  const auto forgerResponseKey =
      p2p::PeerSessionKeyAgreement::generateEphemeralKeyPair();
  assert(forgerResponseKey.has_value());
  const p2p::NetworkEnvelope forgerHello =
      p2p::PeerHandshakeManager::createHelloEnvelope(
          forgerConfig, forgerPeer, chainStatus(),
          localMesh.config().localNodeId(), forgerChallenge->nonce,
          forgerChallenge->ephemeralPublicKeyHex,
          forgerResponseKey->publicKeyHex, forgerIdentity, 1005);

  std::string tamperedPayload = forgerHello.payload();
  const std::size_t portField = tamperedPayload.find("port=19002");
  assert(portField != std::string::npos);
  tamperedPayload.replace(portField, std::string("port=19002").size(),
                          "port=19099");
  const p2p::NetworkEnvelope tamperedHello(
      forgerHello.networkId(), forgerHello.chainId(),
      forgerHello.protocolVersion(), forgerHello.messageType(),
      forgerHello.senderNodeId(), forgerHello.createdAt(),
      forgerHello.ttlSeconds(), tamperedPayload);

  assert(remoteTransport.connect(forgerNodeId, "node-local").success());
  assert(remoteTransport
             .send(p2p::TransportMessage(forgerNodeId, "node-local",
                                         tamperedHello, 1005))
             .success());
  assert(localMesh.receiveAvailable(1005).acceptedCount() == 1);
  const auto forgedResult = node::PeerHandshakeAutoRegistrar::processInbox(
      localMesh, localPeer, chainStatus(), localIdentity, 1005);
  assert(forgedResult.size() == 1);
  assert(!forgedResult[0].registered);
  assert(forgedResult[0].reason.find("Handshake validation rejected") !=
         std::string::npos);
  assert(localMesh.invalidMessageCountForPeer(forgerNodeId) == 1);
  assert(!localMesh.peerRegistry().contains(forgerNodeId));

  return 0;
}
