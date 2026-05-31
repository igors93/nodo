#include "node/TestnetReadiness.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::string boolToString(
    bool value
) {
    return value ? "true" : "false";
}

bool nonEmpty(
    const std::string& value
) {
    return !value.empty();
}

std::uint64_t boundedTargetHeight(
    std::uint64_t localFinalizedHeight,
    std::uint64_t remoteFinalizedHeight,
    std::uint64_t maxWindow
) {
    if (remoteFinalizedHeight <= localFinalizedHeight) {
        return localFinalizedHeight;
    }

    const std::uint64_t nextHeight =
        localFinalizedHeight + 1;

    const std::uint64_t windowEnd =
        nextHeight + maxWindow - 1;

    return std::min(
        remoteFinalizedHeight,
        windowEnd
    );
}

} // namespace

PeerHandshake::PeerHandshake()
    : m_peerId(),
      m_chainId(),
      m_networkName(),
      m_protocolVersion(),
      m_genesisId(),
      m_storageFormatVersion(),
      m_finalizedHeight(0),
      m_finalizedBlockHash(),
      m_timestamp(0) {}

PeerHandshake::PeerHandshake(
    std::string peerId,
    std::string chainId,
    std::string networkName,
    std::string protocolVersion,
    std::string genesisId,
    std::string storageFormatVersion,
    std::uint64_t finalizedHeight,
    std::string finalizedBlockHash,
    std::int64_t timestamp
)
    : m_peerId(std::move(peerId)),
      m_chainId(std::move(chainId)),
      m_networkName(std::move(networkName)),
      m_protocolVersion(std::move(protocolVersion)),
      m_genesisId(std::move(genesisId)),
      m_storageFormatVersion(std::move(storageFormatVersion)),
      m_finalizedHeight(finalizedHeight),
      m_finalizedBlockHash(std::move(finalizedBlockHash)),
      m_timestamp(timestamp) {}

const std::string& PeerHandshake::peerId() const {
    return m_peerId;
}

const std::string& PeerHandshake::chainId() const {
    return m_chainId;
}

const std::string& PeerHandshake::networkName() const {
    return m_networkName;
}

const std::string& PeerHandshake::protocolVersion() const {
    return m_protocolVersion;
}

const std::string& PeerHandshake::genesisId() const {
    return m_genesisId;
}

const std::string& PeerHandshake::storageFormatVersion() const {
    return m_storageFormatVersion;
}

std::uint64_t PeerHandshake::finalizedHeight() const {
    return m_finalizedHeight;
}

const std::string& PeerHandshake::finalizedBlockHash() const {
    return m_finalizedBlockHash;
}

std::int64_t PeerHandshake::timestamp() const {
    return m_timestamp;
}

bool PeerHandshake::isValid() const {
    return nonEmpty(m_peerId) &&
           nonEmpty(m_chainId) &&
           nonEmpty(m_networkName) &&
           nonEmpty(m_protocolVersion) &&
           nonEmpty(m_genesisId) &&
           nonEmpty(m_storageFormatVersion) &&
           nonEmpty(m_finalizedBlockHash) &&
           m_timestamp > 0;
}

std::string PeerHandshake::deterministicId() const {
    return serialize();
}

std::string PeerHandshake::serialize() const {
    std::ostringstream oss;

    oss << "PeerHandshake{"
        << "peerId=" << m_peerId
        << ";chainId=" << m_chainId
        << ";networkName=" << m_networkName
        << ";protocolVersion=" << m_protocolVersion
        << ";genesisId=" << m_genesisId
        << ";storageFormatVersion=" << m_storageFormatVersion
        << ";finalizedHeight=" << m_finalizedHeight
        << ";finalizedBlockHash=" << m_finalizedBlockHash
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

PeerHandshakeValidation::PeerHandshakeValidation()
    : m_status("REJECTED"),
      m_localPeerId(),
      m_remotePeerId(),
      m_reason(TestnetReadiness::HANDSHAKE_REJECTED_REASON),
      m_localHandshakeDigest(),
      m_remoteHandshakeDigest() {}

PeerHandshakeValidation::PeerHandshakeValidation(
    std::string status,
    std::string localPeerId,
    std::string remotePeerId,
    std::string reason,
    std::string localHandshakeDigest,
    std::string remoteHandshakeDigest
)
    : m_status(std::move(status)),
      m_localPeerId(std::move(localPeerId)),
      m_remotePeerId(std::move(remotePeerId)),
      m_reason(std::move(reason)),
      m_localHandshakeDigest(std::move(localHandshakeDigest)),
      m_remoteHandshakeDigest(std::move(remoteHandshakeDigest)) {}

const std::string& PeerHandshakeValidation::status() const {
    return m_status;
}

const std::string& PeerHandshakeValidation::localPeerId() const {
    return m_localPeerId;
}

const std::string& PeerHandshakeValidation::remotePeerId() const {
    return m_remotePeerId;
}

const std::string& PeerHandshakeValidation::reason() const {
    return m_reason;
}

const std::string& PeerHandshakeValidation::localHandshakeDigest() const {
    return m_localHandshakeDigest;
}

const std::string& PeerHandshakeValidation::remoteHandshakeDigest() const {
    return m_remoteHandshakeDigest;
}

bool PeerHandshakeValidation::accepted() const {
    return m_status == "ACCEPTED" && isValid();
}

bool PeerHandshakeValidation::isValid() const {
    if (m_status != "ACCEPTED" && m_status != "REJECTED") {
        return false;
    }

    if (m_localPeerId.empty() ||
        m_remotePeerId.empty() ||
        m_reason.empty() ||
        m_localHandshakeDigest.empty() ||
        m_remoteHandshakeDigest.empty()) {
        return false;
    }

    if (m_status == "ACCEPTED") {
        return m_reason == TestnetReadiness::HANDSHAKE_ACCEPTED_REASON;
    }

    return m_reason == TestnetReadiness::HANDSHAKE_REJECTED_REASON;
}

std::string PeerHandshakeValidation::serialize() const {
    std::ostringstream oss;

    oss << "PeerHandshakeValidation{"
        << "status=" << m_status
        << ";localPeerId=" << m_localPeerId
        << ";remotePeerId=" << m_remotePeerId
        << ";reason=" << m_reason
        << ";localHandshakeDigest=" << m_localHandshakeDigest
        << ";remoteHandshakeDigest=" << m_remoteHandshakeDigest
        << "}";

    return oss.str();
}

BlockSyncRequest::BlockSyncRequest()
    : m_requesterPeerId(),
      m_responderPeerId(),
      m_fromHeight(0),
      m_toHeight(0),
      m_reason() {}

BlockSyncRequest::BlockSyncRequest(
    std::string requesterPeerId,
    std::string responderPeerId,
    std::uint64_t fromHeight,
    std::uint64_t toHeight,
    std::string reason
)
    : m_requesterPeerId(std::move(requesterPeerId)),
      m_responderPeerId(std::move(responderPeerId)),
      m_fromHeight(fromHeight),
      m_toHeight(toHeight),
      m_reason(std::move(reason)) {}

const std::string& BlockSyncRequest::requesterPeerId() const {
    return m_requesterPeerId;
}

const std::string& BlockSyncRequest::responderPeerId() const {
    return m_responderPeerId;
}

std::uint64_t BlockSyncRequest::fromHeight() const {
    return m_fromHeight;
}

std::uint64_t BlockSyncRequest::toHeight() const {
    return m_toHeight;
}

const std::string& BlockSyncRequest::reason() const {
    return m_reason;
}

std::uint64_t BlockSyncRequest::requestedBlockCount() const {
    if (m_toHeight < m_fromHeight) {
        return 0;
    }

    return m_toHeight - m_fromHeight + 1;
}

bool BlockSyncRequest::isValid() const {
    return !m_requesterPeerId.empty() &&
           !m_responderPeerId.empty() &&
           m_requesterPeerId != m_responderPeerId &&
           m_fromHeight > 0 &&
           m_toHeight >= m_fromHeight &&
           requestedBlockCount() <= NODO_SYNC_MAX_BLOCK_WINDOW &&
           m_reason == TestnetReadiness::SYNC_REQUEST_REASON;
}

std::string BlockSyncRequest::serialize() const {
    std::ostringstream oss;

    oss << "BlockSyncRequest{"
        << "requesterPeerId=" << m_requesterPeerId
        << ";responderPeerId=" << m_responderPeerId
        << ";fromHeight=" << m_fromHeight
        << ";toHeight=" << m_toHeight
        << ";requestedBlockCount=" << requestedBlockCount()
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

BlockSyncPlan::BlockSyncPlan()
    : m_status("IN_SYNC"),
      m_localFinalizedHeight(0),
      m_remoteFinalizedHeight(0),
      m_nextHeight(0),
      m_targetHeight(0),
      m_maxWindow(NODO_SYNC_MAX_BLOCK_WINDOW),
      m_reason(TestnetReadiness::SYNC_NOT_REQUIRED_REASON),
      m_sourceHandshakeDigest() {}

BlockSyncPlan::BlockSyncPlan(
    std::string status,
    std::uint64_t localFinalizedHeight,
    std::uint64_t remoteFinalizedHeight,
    std::uint64_t nextHeight,
    std::uint64_t targetHeight,
    std::uint64_t maxWindow,
    std::string reason,
    std::string sourceHandshakeDigest
)
    : m_status(std::move(status)),
      m_localFinalizedHeight(localFinalizedHeight),
      m_remoteFinalizedHeight(remoteFinalizedHeight),
      m_nextHeight(nextHeight),
      m_targetHeight(targetHeight),
      m_maxWindow(maxWindow),
      m_reason(std::move(reason)),
      m_sourceHandshakeDigest(std::move(sourceHandshakeDigest)) {}

const std::string& BlockSyncPlan::status() const {
    return m_status;
}

std::uint64_t BlockSyncPlan::localFinalizedHeight() const {
    return m_localFinalizedHeight;
}

std::uint64_t BlockSyncPlan::remoteFinalizedHeight() const {
    return m_remoteFinalizedHeight;
}

std::uint64_t BlockSyncPlan::nextHeight() const {
    return m_nextHeight;
}

std::uint64_t BlockSyncPlan::targetHeight() const {
    return m_targetHeight;
}

std::uint64_t BlockSyncPlan::maxWindow() const {
    return m_maxWindow;
}

const std::string& BlockSyncPlan::reason() const {
    return m_reason;
}

const std::string& BlockSyncPlan::sourceHandshakeDigest() const {
    return m_sourceHandshakeDigest;
}

bool BlockSyncPlan::needsSync() const {
    return m_status == "SYNC_REQUIRED" && isValid();
}

bool BlockSyncPlan::isValid() const {
    if (m_status != "IN_SYNC" && m_status != "SYNC_REQUIRED") {
        return false;
    }

    if (m_maxWindow == 0 ||
        m_maxWindow > NODO_SYNC_MAX_BLOCK_WINDOW ||
        m_sourceHandshakeDigest.empty()) {
        return false;
    }

    if (m_status == "IN_SYNC") {
        return m_remoteFinalizedHeight <= m_localFinalizedHeight &&
               m_nextHeight == 0 &&
               m_targetHeight == 0 &&
               m_reason == TestnetReadiness::SYNC_NOT_REQUIRED_REASON;
    }

    return m_remoteFinalizedHeight > m_localFinalizedHeight &&
           m_nextHeight == m_localFinalizedHeight + 1 &&
           m_targetHeight >= m_nextHeight &&
           m_targetHeight <= m_remoteFinalizedHeight &&
           (m_targetHeight - m_nextHeight + 1) <= m_maxWindow &&
           m_reason == TestnetReadiness::SYNC_REQUIRED_REASON;
}

std::string BlockSyncPlan::serialize() const {
    std::ostringstream oss;

    oss << "BlockSyncPlan{"
        << "status=" << m_status
        << ";localFinalizedHeight=" << m_localFinalizedHeight
        << ";remoteFinalizedHeight=" << m_remoteFinalizedHeight
        << ";nextHeight=" << m_nextHeight
        << ";targetHeight=" << m_targetHeight
        << ";maxWindow=" << m_maxWindow
        << ";reason=" << m_reason
        << ";sourceHandshakeDigest=" << m_sourceHandshakeDigest
        << "}";

    return oss.str();
}

BlockAnnouncement::BlockAnnouncement()
    : m_peerId(),
      m_blockHeight(0),
      m_blockHash(),
      m_previousBlockHash(),
      m_chainId(),
      m_reason() {}

BlockAnnouncement::BlockAnnouncement(
    std::string peerId,
    std::uint64_t blockHeight,
    std::string blockHash,
    std::string previousBlockHash,
    std::string chainId,
    std::string reason
)
    : m_peerId(std::move(peerId)),
      m_blockHeight(blockHeight),
      m_blockHash(std::move(blockHash)),
      m_previousBlockHash(std::move(previousBlockHash)),
      m_chainId(std::move(chainId)),
      m_reason(std::move(reason)) {}

const std::string& BlockAnnouncement::peerId() const {
    return m_peerId;
}

std::uint64_t BlockAnnouncement::blockHeight() const {
    return m_blockHeight;
}

const std::string& BlockAnnouncement::blockHash() const {
    return m_blockHash;
}

const std::string& BlockAnnouncement::previousBlockHash() const {
    return m_previousBlockHash;
}

const std::string& BlockAnnouncement::chainId() const {
    return m_chainId;
}

const std::string& BlockAnnouncement::reason() const {
    return m_reason;
}

bool BlockAnnouncement::isValid() const {
    return !m_peerId.empty() &&
           m_blockHeight > 0 &&
           !m_blockHash.empty() &&
           !m_previousBlockHash.empty() &&
           !m_chainId.empty() &&
           m_reason == TestnetReadiness::BLOCK_ANNOUNCEMENT_REASON;
}

std::string BlockAnnouncement::serialize() const {
    std::ostringstream oss;

    oss << "BlockAnnouncement{"
        << "peerId=" << m_peerId
        << ";blockHeight=" << m_blockHeight
        << ";blockHash=" << m_blockHash
        << ";previousBlockHash=" << m_previousBlockHash
        << ";chainId=" << m_chainId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

NodeHealthReport::NodeHealthReport()
    : m_status("NOT_READY"),
      m_peerId(),
      m_finalizedHeight(0),
      m_connectedPeerCount(0),
      m_handshakeReady(false),
      m_syncReady(false),
      m_blockGossipReady(false),
      m_transactionGossipReady(false),
      m_reason(TestnetReadiness::NODE_NOT_READY_REASON),
      m_sourceSyncDigest() {}

NodeHealthReport::NodeHealthReport(
    std::string status,
    std::string peerId,
    std::uint64_t finalizedHeight,
    std::uint64_t connectedPeerCount,
    bool handshakeReady,
    bool syncReady,
    bool blockGossipReady,
    bool transactionGossipReady,
    std::string reason,
    std::string sourceSyncDigest
)
    : m_status(std::move(status)),
      m_peerId(std::move(peerId)),
      m_finalizedHeight(finalizedHeight),
      m_connectedPeerCount(connectedPeerCount),
      m_handshakeReady(handshakeReady),
      m_syncReady(syncReady),
      m_blockGossipReady(blockGossipReady),
      m_transactionGossipReady(transactionGossipReady),
      m_reason(std::move(reason)),
      m_sourceSyncDigest(std::move(sourceSyncDigest)) {}

const std::string& NodeHealthReport::status() const {
    return m_status;
}

const std::string& NodeHealthReport::peerId() const {
    return m_peerId;
}

std::uint64_t NodeHealthReport::finalizedHeight() const {
    return m_finalizedHeight;
}

std::uint64_t NodeHealthReport::connectedPeerCount() const {
    return m_connectedPeerCount;
}

bool NodeHealthReport::handshakeReady() const {
    return m_handshakeReady;
}

bool NodeHealthReport::syncReady() const {
    return m_syncReady;
}

bool NodeHealthReport::blockGossipReady() const {
    return m_blockGossipReady;
}

bool NodeHealthReport::transactionGossipReady() const {
    return m_transactionGossipReady;
}

const std::string& NodeHealthReport::reason() const {
    return m_reason;
}

const std::string& NodeHealthReport::sourceSyncDigest() const {
    return m_sourceSyncDigest;
}

bool NodeHealthReport::ready() const {
    return m_status == "READY" && isValid();
}

bool NodeHealthReport::isValid() const {
    if (m_status != "READY" && m_status != "NOT_READY") {
        return false;
    }

    if (m_peerId.empty() ||
        m_reason.empty() ||
        m_sourceSyncDigest.empty()) {
        return false;
    }

    if (m_status == "READY") {
        return m_handshakeReady &&
               m_syncReady &&
               m_blockGossipReady &&
               m_transactionGossipReady &&
               m_reason == TestnetReadiness::NODE_READY_REASON;
    }

    return m_reason == TestnetReadiness::NODE_NOT_READY_REASON;
}

std::string NodeHealthReport::serialize() const {
    std::ostringstream oss;

    oss << "NodeHealthReport{"
        << "status=" << m_status
        << ";peerId=" << m_peerId
        << ";finalizedHeight=" << m_finalizedHeight
        << ";connectedPeerCount=" << m_connectedPeerCount
        << ";handshakeReady=" << boolToString(m_handshakeReady)
        << ";syncReady=" << boolToString(m_syncReady)
        << ";blockGossipReady=" << boolToString(m_blockGossipReady)
        << ";transactionGossipReady=" << boolToString(m_transactionGossipReady)
        << ";reason=" << m_reason
        << ";sourceSyncDigest=" << m_sourceSyncDigest
        << "}";

    return oss.str();
}

PeerHandshake TestnetReadiness::buildLocalHandshake(
    const config::NetworkParameters& networkParameters,
    const std::string& peerId,
    const std::string& genesisId,
    std::uint64_t finalizedHeight,
    const std::string& finalizedBlockHash,
    std::int64_t timestamp
) {
    if (!networkParameters.isValid()) {
        throw std::invalid_argument("Cannot build handshake from invalid network parameters.");
    }

    PeerHandshake handshake(
        peerId,
        networkParameters.chainId(),
        networkParameters.networkName(),
        networkParameters.protocolVersion(),
        genesisId,
        networkParameters.storageFormatVersion(),
        finalizedHeight,
        finalizedBlockHash,
        timestamp
    );

    if (!handshake.isValid()) {
        throw std::invalid_argument("Built peer handshake is invalid.");
    }

    return handshake;
}

PeerHandshakeValidation TestnetReadiness::validateHandshake(
    const PeerHandshake& localHandshake,
    const PeerHandshake& remoteHandshake
) {
    const bool accepted =
        localHandshake.isValid() &&
        remoteHandshake.isValid() &&
        localHandshake.peerId() != remoteHandshake.peerId() &&
        localHandshake.chainId() == remoteHandshake.chainId() &&
        localHandshake.networkName() == remoteHandshake.networkName() &&
        localHandshake.protocolVersion() == remoteHandshake.protocolVersion() &&
        localHandshake.genesisId() == remoteHandshake.genesisId() &&
        localHandshake.storageFormatVersion() == remoteHandshake.storageFormatVersion();

    return PeerHandshakeValidation(
        accepted ? "ACCEPTED" : "REJECTED",
        localHandshake.peerId().empty() ? "UNKNOWN_LOCAL_PEER" : localHandshake.peerId(),
        remoteHandshake.peerId().empty() ? "UNKNOWN_REMOTE_PEER" : remoteHandshake.peerId(),
        accepted ? HANDSHAKE_ACCEPTED_REASON : HANDSHAKE_REJECTED_REASON,
        localHandshake.isValid() ? localHandshake.deterministicId() : "INVALID_LOCAL_HANDSHAKE",
        remoteHandshake.isValid() ? remoteHandshake.deterministicId() : "INVALID_REMOTE_HANDSHAKE"
    );
}

BlockSyncPlan TestnetReadiness::buildSyncPlan(
    const PeerHandshakeValidation& validation,
    const PeerHandshake& localHandshake,
    const PeerHandshake& remoteHandshake,
    std::uint64_t maxWindow
) {
    if (!validation.isValid()) {
        throw std::invalid_argument("Cannot build sync plan from invalid handshake validation.");
    }

    if (maxWindow == 0 || maxWindow > NODO_SYNC_MAX_BLOCK_WINDOW) {
        throw std::invalid_argument("Invalid block sync window.");
    }

    const std::string sourceDigest =
        validation.serialize();

    if (!validation.accepted() ||
        remoteHandshake.finalizedHeight() <= localHandshake.finalizedHeight()) {
        return BlockSyncPlan(
            "IN_SYNC",
            localHandshake.finalizedHeight(),
            remoteHandshake.finalizedHeight(),
            0,
            0,
            maxWindow,
            SYNC_NOT_REQUIRED_REASON,
            sourceDigest
        );
    }

    return BlockSyncPlan(
        "SYNC_REQUIRED",
        localHandshake.finalizedHeight(),
        remoteHandshake.finalizedHeight(),
        localHandshake.finalizedHeight() + 1,
        boundedTargetHeight(
            localHandshake.finalizedHeight(),
            remoteHandshake.finalizedHeight(),
            maxWindow
        ),
        maxWindow,
        SYNC_REQUIRED_REASON,
        sourceDigest
    );
}

BlockSyncRequest TestnetReadiness::buildSyncRequest(
    const PeerHandshakeValidation& validation,
    const BlockSyncPlan& syncPlan
) {
    if (!validation.accepted() || !syncPlan.needsSync()) {
        throw std::invalid_argument("Cannot build block sync request without an accepted handshake and required sync plan.");
    }

    BlockSyncRequest request(
        validation.localPeerId(),
        validation.remotePeerId(),
        syncPlan.nextHeight(),
        syncPlan.targetHeight(),
        SYNC_REQUEST_REASON
    );

    if (!request.isValid()) {
        throw std::invalid_argument("Built block sync request is invalid.");
    }

    return request;
}

BlockAnnouncement TestnetReadiness::buildBlockAnnouncement(
    const config::NetworkParameters& networkParameters,
    const std::string& peerId,
    std::uint64_t blockHeight,
    const std::string& blockHash,
    const std::string& previousBlockHash
) {
    if (!networkParameters.isValid()) {
        throw std::invalid_argument("Cannot build block announcement from invalid network parameters.");
    }

    BlockAnnouncement announcement(
        peerId,
        blockHeight,
        blockHash,
        previousBlockHash,
        networkParameters.chainId(),
        BLOCK_ANNOUNCEMENT_REASON
    );

    if (!announcement.isValid()) {
        throw std::invalid_argument("Built block announcement is invalid.");
    }

    return announcement;
}

NodeHealthReport TestnetReadiness::buildHealthReport(
    const PeerHandshake& localHandshake,
    const std::vector<PeerHandshakeValidation>& validations,
    const BlockSyncPlan& syncPlan,
    bool transactionGossipReady
) {
    if (!localHandshake.isValid() || !syncPlan.isValid()) {
        throw std::invalid_argument("Cannot build health report from invalid local handshake or sync plan.");
    }

    std::uint64_t acceptedPeers = 0;
    for (const PeerHandshakeValidation& validation : validations) {
        if (validation.accepted()) {
            ++acceptedPeers;
        }
    }

    const bool handshakeReady =
        acceptedPeers > 0;

    const bool syncReady =
        !syncPlan.needsSync();

    const bool blockGossipReady =
        handshakeReady && localHandshake.finalizedHeight() > 0;

    const bool ready =
        handshakeReady &&
        syncReady &&
        blockGossipReady &&
        transactionGossipReady;

    return NodeHealthReport(
        ready ? "READY" : "NOT_READY",
        localHandshake.peerId(),
        localHandshake.finalizedHeight(),
        acceptedPeers,
        handshakeReady,
        syncReady,
        blockGossipReady,
        transactionGossipReady,
        ready ? NODE_READY_REASON : NODE_NOT_READY_REASON,
        syncPlan.serialize()
    );
}

bool TestnetReadiness::sameHandshake(
    const PeerHandshake& left,
    const PeerHandshake& right
) {
    return left.serialize() == right.serialize();
}

bool TestnetReadiness::sameValidation(
    const PeerHandshakeValidation& left,
    const PeerHandshakeValidation& right
) {
    return left.serialize() == right.serialize();
}

bool TestnetReadiness::sameSyncPlan(
    const BlockSyncPlan& left,
    const BlockSyncPlan& right
) {
    return left.serialize() == right.serialize();
}

bool TestnetReadiness::sameSyncRequest(
    const BlockSyncRequest& left,
    const BlockSyncRequest& right
) {
    return left.serialize() == right.serialize();
}

bool TestnetReadiness::sameAnnouncement(
    const BlockAnnouncement& left,
    const BlockAnnouncement& right
) {
    return left.serialize() == right.serialize();
}

bool TestnetReadiness::sameHealthReport(
    const NodeHealthReport& left,
    const NodeHealthReport& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
