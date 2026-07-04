#ifndef NODO_P2P_PEER_REGISTRY_HPP
#define NODO_P2P_PEER_REGISTRY_HPP

#include "p2p/Peer.hpp"

#include <map>
#include <string>
#include <vector>

namespace nodo::p2p {

enum class PeerRegistryStatus { REGISTERED, UPDATED, REJECTED, NOT_FOUND };

std::string peerRegistryStatusToString(PeerRegistryStatus status);

class PeerRegistryResult {
public:
  PeerRegistryResult();
  PeerRegistryResult(PeerRegistryStatus status, std::string reason);

  PeerRegistryStatus status() const;
  const std::string &reason() const;
  bool success() const;

private:
  PeerRegistryStatus m_status;
  std::string m_reason;
};

class PeerRegistry {
public:
  PeerRegistry();

  PeerRegistryResult registerPeer(PeerMetadata peer);
  PeerRegistryResult updateHeartbeat(const std::string &nodeId,
                                     std::int64_t seenAt);
  PeerRegistryResult adjustScore(const std::string &nodeId, std::int32_t delta,
                                 std::string reason);
  PeerRegistryResult quarantinePeer(const std::string &nodeId,
                                    std::string reason);
  PeerRegistryResult banPeer(const std::string &nodeId,
                             std::int64_t bannedUntil, std::string reason);
  PeerRegistryResult liftPeerPenalty(const std::string &nodeId,
                                     std::int64_t seenAt);
  std::size_t liftExpiredPeerPenalties(std::int64_t now);

  bool contains(const std::string &nodeId) const;
  bool containsIdentityKey(const std::string &identityKey) const;
  const PeerMetadata *peer(const std::string &nodeId) const;
  const PeerMetadata *peerByIdentityKey(const std::string &identityKey) const;
  std::vector<PeerMetadata> activePeers() const;
  std::vector<PeerMetadata> activePeersAt(std::int64_t now) const;
  std::vector<PeerMetadata> allPeers() const;
  std::size_t size() const;
  bool isValid() const;
  std::string serialize() const;

private:
  std::map<std::string, PeerMetadata> m_peersByNodeId;
  std::map<std::string, std::string> m_nodeIdByIdentityKey;
};

} // namespace nodo::p2p

#endif
