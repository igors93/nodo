#include "p2p/LocalNodeSync.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

std::string localSyncDecisionToString(
    LocalSyncDecision decision
) {
    switch (decision) {
        case LocalSyncDecision::NO_ACTION:
            return "NO_ACTION";
        case LocalSyncDecision::REQUEST_BLOCKS:
            return "REQUEST_BLOCKS";
        case LocalSyncDecision::REJECT_PEER:
            return "REJECT_PEER";
        default:
            return "REJECT_PEER";
    }
}

std::string localSyncRejectReasonToString(
    LocalSyncRejectReason reason
) {
    switch (reason) {
        case LocalSyncRejectReason::NONE:
            return "NONE";
        case LocalSyncRejectReason::INVALID_LOCAL_STATE:
            return "INVALID_LOCAL_STATE";
        case LocalSyncRejectReason::INVALID_PEER:
            return "INVALID_PEER";
        case LocalSyncRejectReason::INVALID_REMOTE_SUMMARY:
            return "INVALID_REMOTE_SUMMARY";
        case LocalSyncRejectReason::REMOTE_BEHIND_FINALITY:
            return "REMOTE_BEHIND_FINALITY";
        case LocalSyncRejectReason::REMOTE_CONFLICTS_WITH_FINALITY:
            return "REMOTE_CONFLICTS_WITH_FINALITY";
        case LocalSyncRejectReason::REMOTE_NOT_BETTER:
            return "REMOTE_NOT_BETTER";
        default:
            return "REMOTE_NOT_BETTER";
    }
}

LocalSyncPlan::LocalSyncPlan()
    : m_decision(LocalSyncDecision::REJECT_PEER),
      m_rejectReason(LocalSyncRejectReason::INVALID_LOCAL_STATE),
      m_detail("Uninitialized local sync plan."),
      m_peer(),
      m_fromHeight(0),
      m_toHeight(0),
      m_requestMessage() {}

LocalSyncPlan LocalSyncPlan::noAction(
    std::string detail
) {
    LocalSyncPlan plan;
    plan.m_decision = LocalSyncDecision::NO_ACTION;
    plan.m_rejectReason = LocalSyncRejectReason::NONE;
    plan.m_detail = std::move(detail);
    return plan;
}

LocalSyncPlan LocalSyncPlan::requestBlocks(
    PeerInfo peer,
    std::uint64_t fromHeight,
    std::uint64_t toHeight,
    PeerMessage requestMessage,
    std::string detail
) {
    LocalSyncPlan plan;
    plan.m_decision = LocalSyncDecision::REQUEST_BLOCKS;
    plan.m_rejectReason = LocalSyncRejectReason::NONE;
    plan.m_peer = std::move(peer);
    plan.m_fromHeight = fromHeight;
    plan.m_toHeight = toHeight;
    plan.m_requestMessage = std::move(requestMessage);
    plan.m_detail = std::move(detail);
    return plan;
}

LocalSyncPlan LocalSyncPlan::rejectPeer(
    LocalSyncRejectReason reason,
    std::string detail
) {
    LocalSyncPlan plan;
    plan.m_decision = LocalSyncDecision::REJECT_PEER;
    plan.m_rejectReason = reason;
    plan.m_detail = std::move(detail);
    return plan;
}

LocalSyncDecision LocalSyncPlan::decision() const {
    return m_decision;
}

LocalSyncRejectReason LocalSyncPlan::rejectReason() const {
    return m_rejectReason;
}

const std::string& LocalSyncPlan::detail() const {
    return m_detail;
}

const PeerInfo& LocalSyncPlan::peer() const {
    return m_peer;
}

std::uint64_t LocalSyncPlan::fromHeight() const {
    return m_fromHeight;
}

std::uint64_t LocalSyncPlan::toHeight() const {
    return m_toHeight;
}

const PeerMessage& LocalSyncPlan::requestMessage() const {
    return m_requestMessage;
}

bool LocalSyncPlan::shouldRequestBlocks() const {
    return m_decision == LocalSyncDecision::REQUEST_BLOCKS &&
           m_requestMessage.isValid();
}

bool LocalSyncPlan::rejected() const {
    return m_decision == LocalSyncDecision::REJECT_PEER;
}

std::string LocalSyncPlan::serialize() const {
    std::ostringstream oss;

    oss << "LocalSyncPlan{"
        << "decision=" << localSyncDecisionToString(m_decision)
        << ";rejectReason=" << localSyncRejectReasonToString(m_rejectReason)
        << ";detail=" << m_detail
        << ";fromHeight=" << m_fromHeight
        << ";toHeight=" << m_toHeight
        << ";peer=" << (m_peer.isValid() ? m_peer.serialize() : "NONE")
        << ";requestMessage=" << (m_requestMessage.isValid() ? m_requestMessage.serialize() : "NONE")
        << "}";

    return oss.str();
}

LocalSyncPlan LocalNodeSynchronizer::evaluatePeerSummary(
    const PeerInfo& localPeer,
    const PeerInfo& remotePeer,
    const core::Blockchain& localChain,
    const consensus::BlockFinalizationRegistry& localFinalizationRegistry,
    const consensus::ChainForkSummary& remoteSummary,
    std::int64_t createdAt
) {
    if (!localPeer.isValid() ||
        !remotePeer.isValid()) {
        return LocalSyncPlan::rejectPeer(
            LocalSyncRejectReason::INVALID_PEER,
            "Local or remote peer metadata is invalid."
        );
    }

    if (createdAt <= 0 ||
        localChain.empty() ||
        !localChain.isValid() ||
        !localFinalizationRegistry.isValid()) {
        return LocalSyncPlan::rejectPeer(
            LocalSyncRejectReason::INVALID_LOCAL_STATE,
            "Local sync state is invalid."
        );
    }

    if (!remoteSummary.isValid()) {
        return LocalSyncPlan::rejectPeer(
            LocalSyncRejectReason::INVALID_REMOTE_SUMMARY,
            "Remote chain summary is invalid."
        );
    }

    const consensus::ChainForkSummary localSummary =
        consensus::ForkChoicePolicy::summarizeChain(
            localChain,
            localFinalizationRegistry
        );

    if (!localSummary.isValid()) {
        return LocalSyncPlan::rejectPeer(
            LocalSyncRejectReason::INVALID_LOCAL_STATE,
            "Local chain summary is invalid."
        );
    }

    if (localSummary.hasFinalizedCheckpoint()) {
        const std::uint64_t localFinalizedHeight =
            localSummary.finalizedCheckpoint().blockIndex();

        if (remoteSummary.latestBlockIndex() < localFinalizedHeight) {
            return LocalSyncPlan::rejectPeer(
                LocalSyncRejectReason::REMOTE_BEHIND_FINALITY,
                "Remote chain is behind local finalized checkpoint."
            );
        }

        if (remoteSummary.hasFinalizedCheckpoint() &&
            remoteSummary.finalizedCheckpoint().blockIndex() == localFinalizedHeight &&
            remoteSummary.finalizedCheckpoint().blockHash() !=
                localSummary.finalizedCheckpoint().blockHash()) {
            return LocalSyncPlan::rejectPeer(
                LocalSyncRejectReason::REMOTE_CONFLICTS_WITH_FINALITY,
                "Remote finalized checkpoint conflicts with local finality."
            );
        }
    }

    const bool remoteHasHigherFinality =
        remoteSummary.hasFinalizedCheckpoint() &&
        (!localSummary.hasFinalizedCheckpoint() ||
         remoteSummary.finalizedCheckpoint().blockIndex() >
            localSummary.finalizedCheckpoint().blockIndex());

    const bool remoteIsLonger =
        remoteSummary.chainSize() > localSummary.chainSize();

    if (!remoteHasHigherFinality &&
        !remoteIsLonger) {
        return LocalSyncPlan::noAction(
            "Remote chain summary is not better than local summary."
        );
    }

    const std::uint64_t fromHeight =
        firstSafeRequestHeight(localSummary);

    const std::uint64_t toHeight =
        remoteSummary.latestBlockIndex();

    if (fromHeight > toHeight) {
        return LocalSyncPlan::noAction(
            "Remote summary does not contain requestable blocks."
        );
    }

    PeerMessage request =
        PeerMessageFactory::syncRequest(
            localPeer,
            remotePeer.peerId(),
            localSummary,
            fromHeight,
            toHeight,
            createdAt
        );

    return LocalSyncPlan::requestBlocks(
        remotePeer,
        fromHeight,
        toHeight,
        request,
        remoteHasHigherFinality
            ? "Remote peer has higher finalized checkpoint."
            : "Remote peer has a longer chain."
    );
}

std::uint64_t LocalNodeSynchronizer::firstSafeRequestHeight(
    const consensus::ChainForkSummary& localSummary
) {
    if (!localSummary.isValid()) {
        return 0;
    }

    if (localSummary.hasFinalizedCheckpoint()) {
        return localSummary.finalizedCheckpoint().blockIndex() + 1;
    }

    /*
     * Keep genesis anchored locally. If there is no finality yet, sync from the
     * first non-genesis block.
     */
    return 1;
}

} // namespace nodo::p2p
