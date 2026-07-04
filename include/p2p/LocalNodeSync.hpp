#ifndef NODO_P2P_LOCAL_NODE_SYNC_HPP
#define NODO_P2P_LOCAL_NODE_SYNC_HPP

#include "consensus/BlockFinalizer.hpp"
#include "consensus/ForkChoice.hpp"
#include "core/Blockchain.hpp"
#include "p2p/PeerMessage.hpp"

#include <cstdint>
#include <string>

namespace nodo::p2p {

enum class LocalSyncDecision { NO_ACTION, REQUEST_BLOCKS, REJECT_PEER };

std::string localSyncDecisionToString(LocalSyncDecision decision);

enum class LocalSyncRejectReason {
  NONE,
  INVALID_LOCAL_STATE,
  INVALID_PEER,
  INVALID_REMOTE_SUMMARY,
  REMOTE_BEHIND_FINALITY,
  REMOTE_CONFLICTS_WITH_FINALITY,
  REMOTE_NOT_BETTER
};

std::string localSyncRejectReasonToString(LocalSyncRejectReason reason);

class LocalSyncPlan {
public:
  LocalSyncPlan();

  static LocalSyncPlan noAction(std::string detail);

  static LocalSyncPlan requestBlocks(PeerInfo peer, std::uint64_t fromHeight,
                                     std::uint64_t toHeight,
                                     PeerMessage requestMessage,
                                     std::string detail);

  static LocalSyncPlan rejectPeer(LocalSyncRejectReason reason,
                                  std::string detail);

  LocalSyncDecision decision() const;
  LocalSyncRejectReason rejectReason() const;
  const std::string &detail() const;
  const PeerInfo &peer() const;
  std::uint64_t fromHeight() const;
  std::uint64_t toHeight() const;
  const PeerMessage &requestMessage() const;

  bool shouldRequestBlocks() const;
  bool rejected() const;

  std::string serialize() const;

private:
  LocalSyncDecision m_decision;
  LocalSyncRejectReason m_rejectReason;
  std::string m_detail;
  PeerInfo m_peer;
  std::uint64_t m_fromHeight;
  std::uint64_t m_toHeight;
  PeerMessage m_requestMessage;
};

class LocalNodeSynchronizer {
public:
  static LocalSyncPlan evaluatePeerSummary(
      const PeerInfo &localPeer, const PeerInfo &remotePeer,
      const core::Blockchain &localChain,
      const consensus::BlockFinalizationRegistry &localFinalizationRegistry,
      const consensus::ChainForkSummary &remoteSummary, std::int64_t createdAt);

  static std::uint64_t
  firstSafeRequestHeight(const consensus::ChainForkSummary &localSummary);
};

} // namespace nodo::p2p

#endif
