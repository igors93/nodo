#ifndef NODO_NODE_CHAIN_SYNC_MESSAGES_HPP
#define NODO_NODE_CHAIN_SYNC_MESSAGES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ChainStatusMessage {
public:
    ChainStatusMessage();

    ChainStatusMessage(
        std::string networkId,
        std::string chainId,
        std::string protocolVersion,
        std::uint64_t latestHeight,
        std::string latestBlockHash,
        std::uint64_t finalizedHeight,
        std::string finalizedBlockHash
    );

    const std::string& networkId() const;
    const std::string& chainId() const;
    const std::string& protocolVersion() const;
    std::uint64_t latestHeight() const;
    const std::string& latestBlockHash() const;
    std::uint64_t finalizedHeight() const;
    const std::string& finalizedBlockHash() const;

    bool isValid() const;
    bool peerIsAheadOf(std::uint64_t localHeight) const;
    std::string serialize() const;

private:
    std::string m_networkId;
    std::string m_chainId;
    std::string m_protocolVersion;
    std::uint64_t m_latestHeight;
    std::string m_latestBlockHash;
    std::uint64_t m_finalizedHeight;
    std::string m_finalizedBlockHash;
};

class BlockLocator {
public:
    BlockLocator();

    BlockLocator(
        std::uint64_t fromHeight,
        std::uint64_t maxBlocks,
        std::vector<std::string> knownAncestorHashes
    );

    std::uint64_t fromHeight() const;
    std::uint64_t maxBlocks() const;
    const std::vector<std::string>& knownAncestorHashes() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_fromHeight;
    std::uint64_t m_maxBlocks;
    std::vector<std::string> m_knownAncestorHashes;
};

class NetworkBlockSyncRequest {
public:
    NetworkBlockSyncRequest();

    NetworkBlockSyncRequest(
        std::string requesterNodeId,
        BlockLocator locator,
        std::int64_t createdAt
    );

    const std::string& requesterNodeId() const;
    const BlockLocator& locator() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_requesterNodeId;
    BlockLocator m_locator;
    std::int64_t m_createdAt;
};

class RoundAdvanceMessage {
public:
    RoundAdvanceMessage();

    RoundAdvanceMessage(
        std::uint64_t height,
        std::uint64_t round,
        std::string proposerAddress,
        std::int64_t createdAt
    );

    std::uint64_t height() const;
    std::uint64_t round() const;
    const std::string& proposerAddress() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_height;
    std::uint64_t m_round;
    std::string m_proposerAddress;
    std::int64_t m_createdAt;
};

} // namespace nodo::node

#endif
