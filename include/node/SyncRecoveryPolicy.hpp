#ifndef NODO_NODE_SYNC_RECOVERY_POLICY_HPP
#define NODO_NODE_SYNC_RECOVERY_POLICY_HPP

#include "core/Blockchain.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/PersistentBlockStateSync.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nodo::node {

enum class SyncRecoveryDecisionStatus { ACCEPTED, STALE, REJECTED };

std::string
syncRecoveryDecisionStatusToString(SyncRecoveryDecisionStatus status);

/*
 * Decision returned when this node receives a BLOCK_SYNC_REQUEST.
 *
 * The policy deliberately separates three cases:
 *   - ACCEPTED: build and send a batch immediately.
 *   - STALE: the request is well-formed, but this node has no new blocks for
 *             that range. This is not a protocol failure.
 *   - REJECTED: the request is malformed, not from the claimed requester,
 *               forks away from our canonical ancestor, or asks for an unsafe
 *               range. This is recorded in SyncHealth.
 */
class SyncServeDecision {
public:
  static SyncServeDecision accepted(std::uint64_t fromHeight,
                                    std::uint64_t maxItems,
                                    std::string expectedAncestorHash);
  static SyncServeDecision stale(std::string reason);
  static SyncServeDecision rejected(std::string reason);

  SyncRecoveryDecisionStatus status() const;
  const std::string &reason() const;
  std::uint64_t fromHeight() const;
  std::uint64_t maxItems() const;
  const std::string &expectedAncestorHash() const;

  bool accepted() const;
  bool stale() const;
  bool rejected() const;

private:
  SyncServeDecision();

  SyncRecoveryDecisionStatus m_status;
  std::string m_reason;
  std::uint64_t m_fromHeight;
  std::uint64_t m_maxItems;
  std::string m_expectedAncestorHash;
};

/*
 * Decision returned when this node receives a BLOCK_SYNC_RESPONSE.
 *
 * It performs cheap envelope/checkpoint/overlap checks before the expensive
 * canonical runtime import. This makes multi-node failures diagnosable and
 * prevents a stale or conflicting peer response from being treated the same as
 * an import failure.
 */
class SyncBatchDecision {
public:
  static SyncBatchDecision accepted(std::size_t firstNewItemIndex);
  static SyncBatchDecision stale(std::string reason);
  static SyncBatchDecision rejected(std::string reason);

  SyncRecoveryDecisionStatus status() const;
  const std::string &reason() const;
  std::size_t firstNewItemIndex() const;

  bool accepted() const;
  bool stale() const;
  bool rejected() const;

private:
  SyncBatchDecision();

  SyncRecoveryDecisionStatus m_status;
  std::string m_reason;
  std::size_t m_firstNewItemIndex;
};

class SyncRecoveryPolicy {
public:
  static SyncServeDecision
  evaluateServeRequest(const NetworkBlockSyncRequest &request,
                       const std::string &envelopeSenderNodeId,
                       const core::Blockchain &localBlockchain,
                       std::uint64_t maxBatchSize);

  static SyncBatchDecision
  evaluateResponseBatch(const PersistentBlockSyncBatch &batch,
                        const std::string &envelopeSenderNodeId,
                        const PersistentSyncCheckpoint &localCheckpoint,
                        const core::Blockchain &localBlockchain);

  static bool targetReached(const core::Blockchain &localBlockchain,
                            std::uint64_t targetHeight,
                            const std::string &targetBlockHash);
};

} // namespace nodo::node

#endif
