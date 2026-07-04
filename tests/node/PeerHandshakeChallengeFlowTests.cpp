#include "node/ChainStatusGossipCodec.hpp"
#include "node/PeerHandshakeAutoRegistrar.hpp"

#include "crypto/KeyPair.hpp"
#include "p2p/EncryptedPeerTransport.hpp"
#include "p2p/LoopbackTransport.hpp"

#include <cassert>
#include <memory>

namespace {

using namespace nodo;

p2p::GossipMeshConfig config(const std::string &nodeId) {
  return p2p::GossipMeshConfig(nodeId, "localnet", "chain-localnet", "1",
                               "genesis-localnet", 60, 3, 100, 50);
}

p2p::PeerMetadata peer(const std::string &nodeId, std::uint16_t port,
                       const crypto::KeyPair &identity, std::int64_t now) {
  return p2p::PeerMetadata(nodeId, p2p::PeerEndpoint("127.0.0.1", port),
                           identity.publicKey().fingerprint(), now, now, 0,
                           false);
}

node::ChainStatusMessage status() {
  return node::ChainStatusMessage("localnet", "chain-localnet", "1", 10,
                                  "block-10", 10, "block-10");
}

std::vector<node::HandshakeRegistrationResult>
processHandshakes(p2p::GossipMesh &mesh, const p2p::PeerMetadata &localPeer,
                  const crypto::KeyPair &identity, std::int64_t now) {
  return node::PeerHandshakeAutoRegistrar::processInbox(
      mesh, mesh.drainInbox(p2p::NetworkMessageType::PEER_HELLO),
      mesh.drainInbox(p2p::NetworkMessageType::PEER_CHALLENGE), localPeer,
      status(), identity, now);
}

} // namespace

int main() {
  auto bus = std::make_shared<p2p::LoopbackTransportBus>();
  p2p::LoopbackTransport rawTransportA(bus);
  p2p::LoopbackTransport rawTransportB(bus);
  p2p::EncryptedPeerTransport transportA(rawTransportA);
  p2p::EncryptedPeerTransport transportB(rawTransportB);
  p2p::GossipMesh meshA(config("node-a"), transportA);
  p2p::GossipMesh meshB(config("node-b"), transportB);

  const crypto::KeyPair identityA =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-challenge-flow-a");
  const crypto::KeyPair identityB =
      crypto::KeyPair::createDeterministicEd25519KeyPair(
          "peer-challenge-flow-b");
  const p2p::PeerMetadata peerA = peer("node-a", 19001, identityA, 1000);
  const p2p::PeerMetadata peerB = peer("node-b", 19002, identityB, 1000);

  assert(
      !node::PeerHandshakeAutoRegistrar::initiateHandshake(meshA, "node-b", 999)
           .allAccepted());
  assert(!meshA.handshakeReplayGuard()
              .outstandingChallenge("node-b", 999)
              .has_value());
  assert(meshA.handshakeReplayGuard().consumedCount() == 0);

  assert(transportA.connect("node-a", "node-b").success());
  assert(transportB.connect("node-b", "node-a").success());
  assert(
      node::PeerHandshakeAutoRegistrar::initiateHandshake(meshA, "node-b", 1000)
          .allAccepted());

  assert(meshB.receiveAvailable(1000).acceptedCount() == 1);
  assert(meshB.inbox().countForType(p2p::NetworkMessageType::PEER_CHALLENGE) ==
         1);
  const auto firstStep = processHandshakes(meshB, peerB, identityB, 1000);
  assert(firstStep.empty());

  assert(meshA.receiveAvailable(1001).acceptedCount() == 2);
  const p2p::NetworkEnvelope originalHello =
      meshA.inbox()
          .messagesForType(p2p::NetworkMessageType::PEER_HELLO)
          .front();
  const auto secondStep = processHandshakes(meshA, peerA, identityA, 1001);
  assert(secondStep.size() == 1);
  assert(secondStep.front().registered);
  assert(meshA.peerRegistry().contains("node-b"));

  assert(meshB.receiveAvailable(1002).acceptedCount() == 1);
  const auto thirdStep = processHandshakes(meshB, peerB, identityB, 1002);
  assert(thirdStep.size() == 1);
  assert(thirdStep.front().registered);
  assert(meshB.peerRegistry().contains("node-a"));
  assert(transportA.hasSession("node-a", "node-b"));
  assert(transportB.hasSession("node-b", "node-a"));

  assert(meshB.flushOutbound(1002).acceptedCount() == 1);
  assert(meshA.receiveAvailable(1002).acceptedCount() == 1);
  const auto chainStatuses =
      meshA.drainInbox(p2p::NetworkMessageType::CHAIN_STATUS);
  assert(chainStatuses.size() == 1);
  const auto decodedStatus =
      node::ChainStatusGossipCodec::decode(chainStatuses.front().payload());
  assert(decodedStatus.has_value());
  assert(decodedStatus->latestHeight() == status().latestHeight());
  assert(decodedStatus->latestBlockHash() == status().latestBlockHash());

  const p2p::NetworkEnvelope replayWrappedHello(
      originalHello.networkId(), originalHello.chainId(),
      originalHello.protocolVersion(), originalHello.messageType(),
      originalHello.senderNodeId(), originalHello.createdAt(),
      originalHello.ttlSeconds() - 1, originalHello.payload());
  assert(replayWrappedHello.messageId() != originalHello.messageId());
  assert(transportB
             .send(p2p::TransportMessage("node-b", "node-a", replayWrappedHello,
                                         1003))
             .success());
  assert(meshA.receiveAvailable(1003).acceptedCount() == 1);
  const auto replayedHello = processHandshakes(meshA, peerA, identityA, 1003);
  assert(replayedHello.size() == 1);
  assert(!replayedHello.front().registered);
  assert(replayedHello.front().reason.find("already consumed") !=
         std::string::npos);
  assert(meshA.invalidMessageCountForPeer("node-b") == 1);

  const p2p::NetworkEnvelope forgedPing("localnet", "chain-localnet", "1",
                                        p2p::NetworkMessageType::PING, "node-b",
                                        1004, 60, "forged-after-handshake");
  assert(rawTransportB
             .send(p2p::TransportMessage("node-b", "node-a", forgedPing, 1004))
             .success());
  assert(meshA.receiveAvailable(1004).acceptedCount() == 0);
  assert(meshA.inbox().countForType(p2p::NetworkMessageType::PING) == 0);
  assert(transportA.rejectedFrameCount() == 1);

  return 0;
}
