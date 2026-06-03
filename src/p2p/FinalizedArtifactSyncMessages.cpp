#include "p2p/FinalizedArtifactSyncMessages.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

// ---------------------------------------------------------------------------
// FinalizedArtifactAnnouncement
// ---------------------------------------------------------------------------

FinalizedArtifactAnnouncement::FinalizedArtifactAnnouncement()
    : m_height(0), m_createdAt(0) {}

FinalizedArtifactAnnouncement::FinalizedArtifactAnnouncement(
    std::string senderNodeId,
    std::string networkId,
    std::string chainId,
    std::string genesisId,
    std::uint64_t height,
    std::string blockHash,
    std::string previousBlockHash,
    std::string artifactDigest,
    std::int64_t createdAt
) : m_senderNodeId(std::move(senderNodeId)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_genesisId(std::move(genesisId)),
    m_height(height),
    m_blockHash(std::move(blockHash)),
    m_previousBlockHash(std::move(previousBlockHash)),
    m_artifactDigest(std::move(artifactDigest)),
    m_createdAt(createdAt) {}

const std::string& FinalizedArtifactAnnouncement::senderNodeId() const { return m_senderNodeId; }
const std::string& FinalizedArtifactAnnouncement::networkId() const { return m_networkId; }
const std::string& FinalizedArtifactAnnouncement::chainId() const { return m_chainId; }
const std::string& FinalizedArtifactAnnouncement::genesisId() const { return m_genesisId; }
std::uint64_t FinalizedArtifactAnnouncement::height() const { return m_height; }
const std::string& FinalizedArtifactAnnouncement::blockHash() const { return m_blockHash; }
const std::string& FinalizedArtifactAnnouncement::previousBlockHash() const { return m_previousBlockHash; }
const std::string& FinalizedArtifactAnnouncement::artifactDigest() const { return m_artifactDigest; }
std::int64_t FinalizedArtifactAnnouncement::createdAt() const { return m_createdAt; }

bool FinalizedArtifactAnnouncement::isValid() const {
    return !m_senderNodeId.empty() &&
           !m_networkId.empty() &&
           !m_chainId.empty() &&
           !m_genesisId.empty() &&
           !m_blockHash.empty() &&
           !m_previousBlockHash.empty() &&
           !m_artifactDigest.empty() &&
           m_createdAt > 0;
}

std::string FinalizedArtifactAnnouncement::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedArtifactAnnouncement{"
        << "senderNodeId=" << m_senderNodeId
        << ";networkId=" << m_networkId
        << ";chainId=" << m_chainId
        << ";genesisId=" << m_genesisId
        << ";height=" << m_height
        << ";blockHash=" << m_blockHash
        << ";previousBlockHash=" << m_previousBlockHash
        << ";artifactDigest=" << m_artifactDigest
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// FinalizedArtifactRequest
// ---------------------------------------------------------------------------

FinalizedArtifactRequest::FinalizedArtifactRequest()
    : m_height(0), m_createdAt(0) {}

FinalizedArtifactRequest::FinalizedArtifactRequest(
    std::string requesterNodeId,
    std::string networkId,
    std::string chainId,
    std::string genesisId,
    std::uint64_t height,
    std::string expectedBlockHash,
    std::string expectedArtifactDigest,
    std::int64_t createdAt
) : m_requesterNodeId(std::move(requesterNodeId)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_genesisId(std::move(genesisId)),
    m_height(height),
    m_expectedBlockHash(std::move(expectedBlockHash)),
    m_expectedArtifactDigest(std::move(expectedArtifactDigest)),
    m_createdAt(createdAt) {}

const std::string& FinalizedArtifactRequest::requesterNodeId() const { return m_requesterNodeId; }
const std::string& FinalizedArtifactRequest::networkId() const { return m_networkId; }
const std::string& FinalizedArtifactRequest::chainId() const { return m_chainId; }
const std::string& FinalizedArtifactRequest::genesisId() const { return m_genesisId; }
std::uint64_t FinalizedArtifactRequest::height() const { return m_height; }
const std::string& FinalizedArtifactRequest::expectedBlockHash() const { return m_expectedBlockHash; }
const std::string& FinalizedArtifactRequest::expectedArtifactDigest() const { return m_expectedArtifactDigest; }
std::int64_t FinalizedArtifactRequest::createdAt() const { return m_createdAt; }

bool FinalizedArtifactRequest::isValid() const {
    return !m_requesterNodeId.empty() &&
           !m_networkId.empty() &&
           !m_chainId.empty() &&
           !m_genesisId.empty() &&
           !m_expectedBlockHash.empty() &&
           !m_expectedArtifactDigest.empty() &&
           m_createdAt > 0;
}

std::string FinalizedArtifactRequest::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedArtifactRequest{"
        << "requesterNodeId=" << m_requesterNodeId
        << ";networkId=" << m_networkId
        << ";chainId=" << m_chainId
        << ";genesisId=" << m_genesisId
        << ";height=" << m_height
        << ";expectedBlockHash=" << m_expectedBlockHash
        << ";expectedArtifactDigest=" << m_expectedArtifactDigest
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// FinalizedArtifactResponse
// ---------------------------------------------------------------------------

std::string finalizedArtifactResponseStatusToString(
    FinalizedArtifactResponseStatus status
) {
    switch (status) {
        case FinalizedArtifactResponseStatus::ARTIFACT_FOUND:    return "ARTIFACT_FOUND";
        case FinalizedArtifactResponseStatus::ARTIFACT_NOT_FOUND: return "ARTIFACT_NOT_FOUND";
        case FinalizedArtifactResponseStatus::HEIGHT_MISMATCH:   return "HEIGHT_MISMATCH";
        case FinalizedArtifactResponseStatus::HASH_MISMATCH:     return "HASH_MISMATCH";
        case FinalizedArtifactResponseStatus::GENESIS_MISMATCH:  return "GENESIS_MISMATCH";
        case FinalizedArtifactResponseStatus::NETWORK_MISMATCH:  return "NETWORK_MISMATCH";
        case FinalizedArtifactResponseStatus::REQUEST_INVALID:   return "REQUEST_INVALID";
        default:                                                  return "UNKNOWN";
    }
}

FinalizedArtifactResponse::FinalizedArtifactResponse()
    : m_hasArtifact(false),
      m_height(0),
      m_status(FinalizedArtifactResponseStatus::ARTIFACT_NOT_FOUND),
      m_createdAt(0) {}

FinalizedArtifactResponse::FinalizedArtifactResponse(
    bool hasArtifact,
    std::string responderNodeId,
    std::string networkId,
    std::string chainId,
    std::string genesisId,
    std::uint64_t height,
    std::string blockHash,
    std::string artifactDigest,
    std::string rawArtifactContents,
    FinalizedArtifactResponseStatus status,
    std::string reason,
    std::int64_t createdAt
) : m_hasArtifact(hasArtifact),
    m_responderNodeId(std::move(responderNodeId)),
    m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_genesisId(std::move(genesisId)),
    m_height(height),
    m_blockHash(std::move(blockHash)),
    m_artifactDigest(std::move(artifactDigest)),
    m_rawArtifactContents(std::move(rawArtifactContents)),
    m_status(status),
    m_reason(std::move(reason)),
    m_createdAt(createdAt) {}

FinalizedArtifactResponse FinalizedArtifactResponse::withArtifact(
    std::string responderNodeId,
    std::string networkId,
    std::string chainId,
    std::string genesisId,
    std::uint64_t height,
    std::string blockHash,
    std::string artifactDigest,
    std::string rawArtifactContents,
    std::int64_t createdAt
) {
    return FinalizedArtifactResponse(
        true,
        std::move(responderNodeId),
        std::move(networkId),
        std::move(chainId),
        std::move(genesisId),
        height,
        std::move(blockHash),
        std::move(artifactDigest),
        std::move(rawArtifactContents),
        FinalizedArtifactResponseStatus::ARTIFACT_FOUND,
        "",
        createdAt
    );
}

FinalizedArtifactResponse FinalizedArtifactResponse::rejected(
    std::string responderNodeId,
    FinalizedArtifactResponseStatus status,
    std::string reason,
    std::int64_t createdAt
) {
    return FinalizedArtifactResponse(
        false,
        std::move(responderNodeId),
        "", "", "", 0, "", "", "",
        status,
        std::move(reason),
        createdAt
    );
}

bool FinalizedArtifactResponse::hasArtifact() const { return m_hasArtifact; }
const std::string& FinalizedArtifactResponse::responderNodeId() const { return m_responderNodeId; }
const std::string& FinalizedArtifactResponse::networkId() const { return m_networkId; }
const std::string& FinalizedArtifactResponse::chainId() const { return m_chainId; }
const std::string& FinalizedArtifactResponse::genesisId() const { return m_genesisId; }
std::uint64_t FinalizedArtifactResponse::height() const { return m_height; }
const std::string& FinalizedArtifactResponse::blockHash() const { return m_blockHash; }
const std::string& FinalizedArtifactResponse::artifactDigest() const { return m_artifactDigest; }
const std::string& FinalizedArtifactResponse::rawArtifactContents() const { return m_rawArtifactContents; }
FinalizedArtifactResponseStatus FinalizedArtifactResponse::status() const { return m_status; }
const std::string& FinalizedArtifactResponse::reason() const { return m_reason; }
std::int64_t FinalizedArtifactResponse::createdAt() const { return m_createdAt; }

bool FinalizedArtifactResponse::isValid() const {
    if (!m_hasArtifact) {
        return !m_responderNodeId.empty() && m_createdAt > 0;
    }
    return !m_responderNodeId.empty() &&
           !m_networkId.empty() &&
           !m_chainId.empty() &&
           !m_genesisId.empty() &&
           !m_blockHash.empty() &&
           !m_artifactDigest.empty() &&
           !m_rawArtifactContents.empty() &&
           m_status == FinalizedArtifactResponseStatus::ARTIFACT_FOUND &&
           m_createdAt > 0;
}

std::string FinalizedArtifactResponse::serialize() const {
    std::ostringstream oss;
    oss << "FinalizedArtifactResponse{"
        << "hasArtifact=" << (m_hasArtifact ? "1" : "0")
        << ";status=" << finalizedArtifactResponseStatusToString(m_status)
        << ";responderNodeId=" << m_responderNodeId
        << ";reason=" << m_reason;
    if (m_hasArtifact) {
        oss << ";networkId=" << m_networkId
            << ";chainId=" << m_chainId
            << ";genesisId=" << m_genesisId
            << ";height=" << m_height
            << ";blockHash=" << m_blockHash
            << ";artifactDigest=" << m_artifactDigest
            << ";rawContentLength=" << m_rawArtifactContents.size();
    }
    oss << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

} // namespace nodo::p2p
