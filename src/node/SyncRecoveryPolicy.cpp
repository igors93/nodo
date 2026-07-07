#include "node/SyncRecoveryPolicy.hpp"

#include <algorithm>
#include <utility>

namespace nodo::node {

std::string
syncRecoveryDecisionStatusToString(SyncRecoveryDecisionStatus status) {
  switch (status) {
  case SyncRecoveryDecisionStatus::ACCEPTED:
    return "ACCEPTED";
  case SyncRecoveryDecisionStatus::STALE:
    return "STALE";
  case SyncRecoveryDecisionStatus::REJECTED:
    return "REJECTED";
  default:
    return "REJECTED";
  }
}

SyncServeDecision::SyncServeDecision()
    : m_status(SyncRecoveryDecisionStatus::REJECTED),
      m_reason("Uninitialized sync serve decision."), m_fromHeight(0),
      m_maxItems(0), m_expectedAncestorHash() {}

SyncServeDecision
SyncServeDecision::accepted(std::uint64_t fromHeight, std::uint64_t maxItems,
                            std::string expectedAncestorHash) {
  SyncServeDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::ACCEPTED;
  decision.m_reason = "BLOCK_SYNC_REQUEST can be served.";
  decision.m_fromHeight = fromHeight;
  decision.m_maxItems = maxItems;
  decision.m_expectedAncestorHash = std::move(expectedAncestorHash);
  return decision;
}

SyncServeDecision SyncServeDecision::stale(std::string reason) {
  SyncServeDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::STALE;
  decision.m_reason = std::move(reason);
  return decision;
}

SyncServeDecision SyncServeDecision::rejected(std::string reason) {
  SyncServeDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::REJECTED;
  decision.m_reason = std::move(reason);
  return decision;
}

SyncRecoveryDecisionStatus SyncServeDecision::status() const {
  return m_status;
}
const std::string &SyncServeDecision::reason() const { return m_reason; }
std::uint64_t SyncServeDecision::fromHeight() const { return m_fromHeight; }
std::uint64_t SyncServeDecision::maxItems() const { return m_maxItems; }
const std::string &SyncServeDecision::expectedAncestorHash() const {
  return m_expectedAncestorHash;
}
bool SyncServeDecision::accepted() const {
  return m_status == SyncRecoveryDecisionStatus::ACCEPTED;
}
bool SyncServeDecision::stale() const {
  return m_status == SyncRecoveryDecisionStatus::STALE;
}
bool SyncServeDecision::rejected() const {
  return m_status == SyncRecoveryDecisionStatus::REJECTED;
}

SyncBatchDecision::SyncBatchDecision()
    : m_status(SyncRecoveryDecisionStatus::REJECTED),
      m_reason("Uninitialized sync batch decision."), m_firstNewItemIndex(0) {}

SyncBatchDecision SyncBatchDecision::accepted(std::size_t firstNewItemIndex) {
  SyncBatchDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::ACCEPTED;
  decision.m_reason = "BLOCK_SYNC_RESPONSE can be imported.";
  decision.m_firstNewItemIndex = firstNewItemIndex;
  return decision;
}

SyncBatchDecision SyncBatchDecision::stale(std::string reason) {
  SyncBatchDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::STALE;
  decision.m_reason = std::move(reason);
  return decision;
}

SyncBatchDecision SyncBatchDecision::rejected(std::string reason) {
  SyncBatchDecision decision;
  decision.m_status = SyncRecoveryDecisionStatus::REJECTED;
  decision.m_reason = std::move(reason);
  return decision;
}

SyncRecoveryDecisionStatus SyncBatchDecision::status() const {
  return m_status;
}
const std::string &SyncBatchDecision::reason() const { return m_reason; }
std::size_t SyncBatchDecision::firstNewItemIndex() const {
  return m_firstNewItemIndex;
}
bool SyncBatchDecision::accepted() const {
  return m_status == SyncRecoveryDecisionStatus::ACCEPTED;
}
bool SyncBatchDecision::stale() const {
  return m_status == SyncRecoveryDecisionStatus::STALE;
}
bool SyncBatchDecision::rejected() const {
  return m_status == SyncRecoveryDecisionStatus::REJECTED;
}

SyncServeDecision SyncRecoveryPolicy::evaluateServeRequest(
    const NetworkBlockSyncRequest &request,
    const std::string &envelopeSenderNodeId,
    const core::Blockchain &localBlockchain, std::uint64_t maxBatchSize) {
  if (!request.isValid()) {
    return SyncServeDecision::rejected(
        "BLOCK_SYNC_REQUEST is structurally invalid.");
  }

  if (request.requesterNodeId() != envelopeSenderNodeId) {
    return SyncServeDecision::rejected(
        "BLOCK_SYNC_REQUEST requesterNodeId does not match the envelope "
        "sender.");
  }

  if (localBlockchain.empty()) {
    return SyncServeDecision::stale(
        "Responder has no canonical blocks available for sync.");
  }

  const std::uint64_t fromHeight = request.locator().fromHeight();
  const std::uint64_t localTipHeight = localBlockchain.latestBlock().index();
  if (fromHeight == 0) {
    return SyncServeDecision::rejected(
        "BLOCK_SYNC_REQUEST fromHeight must be greater than zero.");
  }

  if (fromHeight > localTipHeight) {
    return SyncServeDecision::stale(
        "Responder is not ahead at the requested sync height.");
  }

  if (fromHeight >= localBlockchain.blocks().size()) {
    return SyncServeDecision::stale(
        "Requested sync height is not present in the responder chain yet.");
  }

  const core::Block &ancestor =
      localBlockchain.blocks()[static_cast<std::size_t>(fromHeight - 1)];
  const std::string &expectedAncestorHash = ancestor.hash();
  const auto &knownAncestors = request.locator().knownAncestorHashes();
  if (std::find(knownAncestors.begin(), knownAncestors.end(),
                expectedAncestorHash) == knownAncestors.end()) {
    return SyncServeDecision::rejected(
        "BLOCK_SYNC_REQUEST does not contain this responder's canonical "
        "ancestor hash at height " +
        std::to_string(fromHeight - 1) + ".");
  }

  if (maxBatchSize == 0 ||
      maxBatchSize > NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH) {
    maxBatchSize = NODO_PERSISTENT_SYNC_MAX_BLOCK_BATCH;
  }

  const std::uint64_t available = localTipHeight - fromHeight + 1;
  const std::uint64_t requested =
      std::min<std::uint64_t>(request.locator().maxBlocks(), maxBatchSize);
  const std::uint64_t maxItems = std::min(available, requested);
  if (maxItems == 0) {
    return SyncServeDecision::stale(
        "BLOCK_SYNC_REQUEST has no serveable block window.");
  }

  return SyncServeDecision::accepted(fromHeight, maxItems,
                                     expectedAncestorHash);
}

SyncBatchDecision SyncRecoveryPolicy::evaluateResponseBatch(
    const PersistentBlockSyncBatch &batch,
    const std::string &envelopeSenderNodeId,
    const PersistentSyncCheckpoint &localCheckpoint,
    const core::Blockchain &localBlockchain) {
  if (!localCheckpoint.isValid()) {
    return SyncBatchDecision::rejected(
        "Local persistent sync checkpoint is invalid.");
  }

  if (!batch.isValid()) {
    return SyncBatchDecision::rejected(
        "BLOCK_SYNC_RESPONSE batch is structurally invalid.");
  }

  if (batch.sourcePeerId() != envelopeSenderNodeId) {
    return SyncBatchDecision::rejected(
        "BLOCK_SYNC_RESPONSE sourcePeerId does not match the envelope "
        "sender.");
  }

  if (localBlockchain.empty()) {
    return SyncBatchDecision::rejected(
        "Local blockchain is empty while applying a sync response.");
  }

  const core::Block &localTip = localBlockchain.latestBlock();
  if (localTip.index() != localCheckpoint.finalizedHeight() ||
      localTip.hash() != localCheckpoint.finalizedBlockHash()) {
    return SyncBatchDecision::rejected(
        "Local persistent sync checkpoint does not match the canonical tip.");
  }

  if (batch.fromHeight() > localTip.index() + 1) {
    return SyncBatchDecision::rejected(
        "BLOCK_SYNC_RESPONSE starts after the next expected local height.");
  }

  const auto &localBlocks = localBlockchain.blocks();
  std::size_t firstNewItemIndex = 0;
  for (; firstNewItemIndex < batch.items().size(); ++firstNewItemIndex) {
    const PersistentBlockSyncItem &item = batch.items()[firstNewItemIndex];
    if (item.height() > localTip.index()) {
      break;
    }

    if (item.height() >= localBlocks.size()) {
      return SyncBatchDecision::rejected(
          "BLOCK_SYNC_RESPONSE overlaps a height that is not present in the "
          "local canonical chain.");
    }

    const core::Block &localBlock =
        localBlocks[static_cast<std::size_t>(item.height())];
    if (localBlock.hash() != item.blockHash()) {
      return SyncBatchDecision::rejected(
          "BLOCK_SYNC_RESPONSE conflicts with the local canonical chain at "
          "already-finalized height " +
          std::to_string(item.height()) + ".");
    }
  }

  if (firstNewItemIndex == batch.items().size()) {
    return SyncBatchDecision::stale(
        "BLOCK_SYNC_RESPONSE contains only blocks already finalized locally.");
  }

  const PersistentBlockSyncItem &firstNew = batch.items()[firstNewItemIndex];
  if (firstNew.height() != localTip.index() + 1) {
    return SyncBatchDecision::rejected(
        "BLOCK_SYNC_RESPONSE first new block is not the next local height.");
  }

  if (firstNew.previousBlockHash() != localTip.hash()) {
    return SyncBatchDecision::rejected(
        "BLOCK_SYNC_RESPONSE first new block does not connect to the local "
        "canonical tip.");
  }

  return SyncBatchDecision::accepted(firstNewItemIndex);
}

bool SyncRecoveryPolicy::targetReached(const core::Blockchain &localBlockchain,
                                       std::uint64_t targetHeight,
                                       const std::string &targetBlockHash) {
  if (targetHeight == 0 || localBlockchain.empty()) {
    return true;
  }

  if (localBlockchain.latestBlock().index() < targetHeight) {
    return false;
  }

  if (targetBlockHash.empty()) {
    return true;
  }

  if (targetHeight >= localBlockchain.blocks().size()) {
    return false;
  }

  return localBlockchain.blocks()[static_cast<std::size_t>(targetHeight)]
             .hash() == targetBlockHash;
}

} // namespace nodo::node
