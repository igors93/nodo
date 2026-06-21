#include "node/ChainSyncMessages.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string& value) {
    if (value.empty() || value.size() > 200) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

ChainStatusMessage::ChainStatusMessage()
    : m_networkId(""),
      m_chainId(""),
      m_protocolVersion(""),
      m_latestHeight(0),
      m_latestBlockHash(""),
      m_finalizedHeight(0),
      m_finalizedBlockHash("") {}

ChainStatusMessage::ChainStatusMessage(
    std::string networkId,
    std::string chainId,
    std::string protocolVersion,
    std::uint64_t latestHeight,
    std::string latestBlockHash,
    std::uint64_t finalizedHeight,
    std::string finalizedBlockHash
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_protocolVersion(std::move(protocolVersion)),
    m_latestHeight(latestHeight),
    m_latestBlockHash(std::move(latestBlockHash)),
    m_finalizedHeight(finalizedHeight),
    m_finalizedBlockHash(std::move(finalizedBlockHash)) {}

const std::string& ChainStatusMessage::networkId() const { return m_networkId; }
const std::string& ChainStatusMessage::chainId() const { return m_chainId; }
const std::string& ChainStatusMessage::protocolVersion() const { return m_protocolVersion; }
std::uint64_t ChainStatusMessage::latestHeight() const { return m_latestHeight; }
const std::string& ChainStatusMessage::latestBlockHash() const { return m_latestBlockHash; }
std::uint64_t ChainStatusMessage::finalizedHeight() const { return m_finalizedHeight; }
const std::string& ChainStatusMessage::finalizedBlockHash() const { return m_finalizedBlockHash; }

bool ChainStatusMessage::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_protocolVersion) &&
           isSafeScalar(m_latestBlockHash) &&
           isSafeScalar(m_finalizedBlockHash) &&
           m_finalizedHeight <= m_latestHeight;
}

bool ChainStatusMessage::peerIsAheadOf(std::uint64_t localHeight) const {
    return isValid() && m_latestHeight > localHeight;
}

std::string ChainStatusMessage::serialize() const {
    std::ostringstream output;
    output << "ChainStatusMessage{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";protocolVersion=" << m_protocolVersion
           << ";latestHeight=" << m_latestHeight
           << ";latestBlockHash=" << m_latestBlockHash
           << ";finalizedHeight=" << m_finalizedHeight
           << ";finalizedBlockHash=" << m_finalizedBlockHash
           << "}";
    return output.str();
}

BlockLocator::BlockLocator()
    : m_fromHeight(0),
      m_maxBlocks(0),
      m_knownAncestorHashes() {}

BlockLocator::BlockLocator(
    std::uint64_t fromHeight,
    std::uint64_t maxBlocks,
    std::vector<std::string> knownAncestorHashes
) : m_fromHeight(fromHeight),
    m_maxBlocks(maxBlocks),
    m_knownAncestorHashes(std::move(knownAncestorHashes)) {}

std::uint64_t BlockLocator::fromHeight() const { return m_fromHeight; }
std::uint64_t BlockLocator::maxBlocks() const { return m_maxBlocks; }
const std::vector<std::string>& BlockLocator::knownAncestorHashes() const { return m_knownAncestorHashes; }

bool BlockLocator::isValid() const {
    if (m_maxBlocks == 0 || m_maxBlocks > 1024 || m_knownAncestorHashes.empty()) {
        return false;
    }

    for (const auto& hash : m_knownAncestorHashes) {
        if (!isSafeScalar(hash)) {
            return false;
        }
    }

    return true;
}

std::string BlockLocator::serialize() const {
    std::ostringstream output;
    output << "BlockLocator{fromHeight=" << m_fromHeight
           << ";maxBlocks=" << m_maxBlocks
           << ";knownAncestorHashes=[";
    for (std::size_t index = 0; index < m_knownAncestorHashes.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << m_knownAncestorHashes[index];
    }
    output << "]}";
    return output.str();
}

NetworkBlockSyncRequest::NetworkBlockSyncRequest()
    : m_requesterNodeId(""),
      m_locator(),
      m_createdAt(0) {}

NetworkBlockSyncRequest::NetworkBlockSyncRequest(
    std::string requesterNodeId,
    BlockLocator locator,
    std::int64_t createdAt
) : m_requesterNodeId(std::move(requesterNodeId)),
    m_locator(std::move(locator)),
    m_createdAt(createdAt) {}

const std::string& NetworkBlockSyncRequest::requesterNodeId() const { return m_requesterNodeId; }
const BlockLocator& NetworkBlockSyncRequest::locator() const { return m_locator; }
std::int64_t NetworkBlockSyncRequest::createdAt() const { return m_createdAt; }

bool NetworkBlockSyncRequest::isValid() const {
    return isSafeScalar(m_requesterNodeId) &&
           m_locator.isValid() &&
           m_createdAt > 0;
}

std::string NetworkBlockSyncRequest::serialize() const {
    std::ostringstream output;
    output << "NetworkBlockSyncRequest{"
           << "requesterNodeId=" << m_requesterNodeId
           << ";locator=" << m_locator.serialize()
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

RoundAdvanceMessage::RoundAdvanceMessage()
    : m_height(0),
      m_round(0),
      m_proposerAddress(""),
      m_createdAt(0) {}

RoundAdvanceMessage::RoundAdvanceMessage(
    std::uint64_t height,
    std::uint64_t round,
    std::string proposerAddress,
    std::int64_t createdAt
) : m_height(height),
    m_round(round),
    m_proposerAddress(std::move(proposerAddress)),
    m_createdAt(createdAt) {}

std::uint64_t RoundAdvanceMessage::height() const { return m_height; }
std::uint64_t RoundAdvanceMessage::round() const { return m_round; }
const std::string& RoundAdvanceMessage::proposerAddress() const { return m_proposerAddress; }
std::int64_t RoundAdvanceMessage::createdAt() const { return m_createdAt; }

bool RoundAdvanceMessage::isValid() const {
    return m_height > 0 &&
           m_round > 0 &&
           isSafeScalar(m_proposerAddress) &&
           m_createdAt > 0;
}

std::string RoundAdvanceMessage::serialize() const {
    std::ostringstream output;
    output << "RoundAdvanceMessage{"
           << "height=" << m_height
           << ";round=" << m_round
           << ";proposerAddress=" << m_proposerAddress
           << ";createdAt=" << m_createdAt
           << "}";
    return output.str();
}

} // namespace nodo::node
