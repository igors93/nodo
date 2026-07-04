#ifndef NODO_P2P_PEER_ABUSE_EVIDENCE_HPP
#define NODO_P2P_PEER_ABUSE_EVIDENCE_HPP

#include "economics/ProtocolEvidence.hpp"

#include <cstdint>
#include <string>

namespace nodo::p2p {

/*
 * PeerAbuseAction describes the action taken when abuse was detected.
 */
enum class PeerAbuseAction { REJECTED, RATE_LIMITED, QUARANTINED, BANNED };

std::string peerAbuseActionToString(PeerAbuseAction action);

/*
 * PeerAbuseEvidence is a canonical, auditable record of a peer misbehavior
 * event observed by the local node.
 *
 * Security principle:
 * This record is observational only. It does not trigger economic penalties
 * at P0. The payloadDigest links this record to the offending message so that
 * any node that possesses the original message can verify the claim.
 *
 * DoS protection: callers must enforce rate limits before creating evidence.
 * The GossipMesh creates at most one quarantine evidence record per peer.
 */
class PeerAbuseEvidence {
public:
  PeerAbuseEvidence();

  PeerAbuseEvidence(std::string evidenceId, std::string peerId,
                    std::string messageType, std::string payloadDigest,
                    std::string observerNodeId, std::uint64_t blockHeight,
                    PeerAbuseAction action, std::string reason,
                    std::int64_t observedAt);

  const std::string &evidenceId() const;
  const std::string &peerId() const;
  const std::string &messageType() const;
  const std::string &payloadDigest() const;
  const std::string &observerNodeId() const;
  std::uint64_t blockHeight() const;
  PeerAbuseAction action() const;
  const std::string &reason() const;
  std::int64_t observedAt() const;

  bool isValid() const;
  const std::string &rejectionReason() const;

  // Convert to canonical ProtocolEvidence for storage.
  economics::ProtocolEvidence toProtocolEvidence() const;

  std::string serialize() const;

private:
  std::string m_evidenceId;
  std::string m_peerId;
  std::string m_messageType;
  std::string m_payloadDigest;
  std::string m_observerNodeId;
  std::uint64_t m_blockHeight;
  PeerAbuseAction m_action;
  std::string m_reason;
  std::int64_t m_observedAt;
  bool m_valid;
  std::string m_rejectionReason;
};

} // namespace nodo::p2p

#endif
