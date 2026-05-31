#ifndef NODO_NODE_TESTNET_READINESS_HPP
#define NODO_NODE_TESTNET_READINESS_HPP

#include "config/NetworkParameters.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::uint64_t NODO_SYNC_MAX_BLOCK_WINDOW = 512;

class PeerHandshake {
public:
    PeerHandshake();

    PeerHandshake(
        std::string peerId,
        std::string chainId,
        std::string networkName,
        std::string protocolVersion,
        std::string genesisId,
        std::string storageFormatVersion,
        std::uint64_t finalizedHeight,
        std::string finalizedBlockHash,
        std::int64_t timestamp
    );

    const std::string& peerId() const;
    const std::string& chainId() const;
    const std::string& networkName() const;
    const std::string& protocolVersion() const;
    const std::string& genesisId() const;
    const std::string& storageFormatVersion() const;
    std::uint64_t finalizedHeight() const;
    const std::string& finalizedBlockHash() const;
    std::int64_t timestamp() const;

    bool isValid() const;
    std::string deterministicId() const;
    std::string serialize() const;

private:
    std::string m_peerId;
    std::string m_chainId;
    std::string m_networkName;
    std::string m_protocolVersion;
    std::string m_genesisId;
    std::string m_storageFormatVersion;
    std::uint64_t m_finalizedHeight;
    std::string m_finalizedBlockHash;
    std::int64_t m_timestamp;
};

class PeerHandshakeValidation {
public:
    PeerHandshakeValidation();

    PeerHandshakeValidation(
        std::string status,
        std::string localPeerId,
        std::string remotePeerId,
        std::string reason,
        std::string localHandshakeDigest,
        std::string remoteHandshakeDigest
    );

    const std::string& status() const;
    const std::string& localPeerId() const;
    const std::string& remotePeerId() const;
    const std::string& reason() const;
    const std::string& localHandshakeDigest() const;
    const std::string& remoteHandshakeDigest() const;

    bool accepted() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::string m_localPeerId;
    std::string m_remotePeerId;
    std::string m_reason;
    std::string m_localHandshakeDigest;
    std::string m_remoteHandshakeDigest;
};

class BlockSyncRequest {
public:
    BlockSyncRequest();

    BlockSyncRequest(
        std::string requesterPeerId,
        std::string responderPeerId,
        std::uint64_t fromHeight,
        std::uint64_t toHeight,
        std::string reason
    );

    const std::string& requesterPeerId() const;
    const std::string& responderPeerId() const;
    std::uint64_t fromHeight() const;
    std::uint64_t toHeight() const;
    const std::string& reason() const;

    std::uint64_t requestedBlockCount() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_requesterPeerId;
    std::string m_responderPeerId;
    std::uint64_t m_fromHeight;
    std::uint64_t m_toHeight;
    std::string m_reason;
};

class BlockSyncPlan {
public:
    BlockSyncPlan();

    BlockSyncPlan(
        std::string status,
        std::uint64_t localFinalizedHeight,
        std::uint64_t remoteFinalizedHeight,
        std::uint64_t nextHeight,
        std::uint64_t targetHeight,
        std::uint64_t maxWindow,
        std::string reason,
        std::string sourceHandshakeDigest
    );

    const std::string& status() const;
    std::uint64_t localFinalizedHeight() const;
    std::uint64_t remoteFinalizedHeight() const;
    std::uint64_t nextHeight() const;
    std::uint64_t targetHeight() const;
    std::uint64_t maxWindow() const;
    const std::string& reason() const;
    const std::string& sourceHandshakeDigest() const;

    bool needsSync() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::uint64_t m_localFinalizedHeight;
    std::uint64_t m_remoteFinalizedHeight;
    std::uint64_t m_nextHeight;
    std::uint64_t m_targetHeight;
    std::uint64_t m_maxWindow;
    std::string m_reason;
    std::string m_sourceHandshakeDigest;
};

class BlockAnnouncement {
public:
    BlockAnnouncement();

    BlockAnnouncement(
        std::string peerId,
        std::uint64_t blockHeight,
        std::string blockHash,
        std::string previousBlockHash,
        std::string chainId,
        std::string reason
    );

    const std::string& peerId() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const std::string& previousBlockHash() const;
    const std::string& chainId() const;
    const std::string& reason() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_peerId;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::string m_previousBlockHash;
    std::string m_chainId;
    std::string m_reason;
};

class NodeHealthReport {
public:
    NodeHealthReport();

    NodeHealthReport(
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
    );

    const std::string& status() const;
    const std::string& peerId() const;
    std::uint64_t finalizedHeight() const;
    std::uint64_t connectedPeerCount() const;
    bool handshakeReady() const;
    bool syncReady() const;
    bool blockGossipReady() const;
    bool transactionGossipReady() const;
    const std::string& reason() const;
    const std::string& sourceSyncDigest() const;

    bool ready() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_status;
    std::string m_peerId;
    std::uint64_t m_finalizedHeight;
    std::uint64_t m_connectedPeerCount;
    bool m_handshakeReady;
    bool m_syncReady;
    bool m_blockGossipReady;
    bool m_transactionGossipReady;
    std::string m_reason;
    std::string m_sourceSyncDigest;
};

class TestnetReadiness {
public:
    static constexpr const char* HANDSHAKE_ACCEPTED_REASON =
        "TESTNET_HANDSHAKE_ACCEPTED";

    static constexpr const char* HANDSHAKE_REJECTED_REASON =
        "TESTNET_HANDSHAKE_REJECTED";

    static constexpr const char* SYNC_NOT_REQUIRED_REASON =
        "SYNC_NOT_REQUIRED";

    static constexpr const char* SYNC_REQUIRED_REASON =
        "SYNC_REQUIRED";

    static constexpr const char* SYNC_REQUEST_REASON =
        "BLOCK_SYNC_REQUEST";

    static constexpr const char* BLOCK_ANNOUNCEMENT_REASON =
        "FINALIZED_BLOCK_ANNOUNCEMENT";

    static constexpr const char* NODE_READY_REASON =
        "NODE_TESTNET_READY";

    static constexpr const char* NODE_NOT_READY_REASON =
        "NODE_TESTNET_NOT_READY";

    static PeerHandshake buildLocalHandshake(
        const config::NetworkParameters& networkParameters,
        const std::string& peerId,
        const std::string& genesisId,
        std::uint64_t finalizedHeight,
        const std::string& finalizedBlockHash,
        std::int64_t timestamp
    );

    static PeerHandshakeValidation validateHandshake(
        const PeerHandshake& localHandshake,
        const PeerHandshake& remoteHandshake
    );

    static BlockSyncPlan buildSyncPlan(
        const PeerHandshakeValidation& validation,
        const PeerHandshake& localHandshake,
        const PeerHandshake& remoteHandshake,
        std::uint64_t maxWindow = NODO_SYNC_MAX_BLOCK_WINDOW
    );

    static BlockSyncRequest buildSyncRequest(
        const PeerHandshakeValidation& validation,
        const BlockSyncPlan& syncPlan
    );

    static BlockAnnouncement buildBlockAnnouncement(
        const config::NetworkParameters& networkParameters,
        const std::string& peerId,
        std::uint64_t blockHeight,
        const std::string& blockHash,
        const std::string& previousBlockHash
    );

    static NodeHealthReport buildHealthReport(
        const PeerHandshake& localHandshake,
        const std::vector<PeerHandshakeValidation>& validations,
        const BlockSyncPlan& syncPlan,
        bool transactionGossipReady
    );

    static bool sameHandshake(
        const PeerHandshake& left,
        const PeerHandshake& right
    );

    static bool sameValidation(
        const PeerHandshakeValidation& left,
        const PeerHandshakeValidation& right
    );

    static bool sameSyncPlan(
        const BlockSyncPlan& left,
        const BlockSyncPlan& right
    );

    static bool sameSyncRequest(
        const BlockSyncRequest& left,
        const BlockSyncRequest& right
    );

    static bool sameAnnouncement(
        const BlockAnnouncement& left,
        const BlockAnnouncement& right
    );

    static bool sameHealthReport(
        const NodeHealthReport& left,
        const NodeHealthReport& right
    );
};

} // namespace nodo::node

#endif
