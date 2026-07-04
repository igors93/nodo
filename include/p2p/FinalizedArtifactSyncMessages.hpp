#ifndef NODO_P2P_FINALIZED_ARTIFACT_SYNC_MESSAGES_HPP
#define NODO_P2P_FINALIZED_ARTIFACT_SYNC_MESSAGES_HPP

#include <cstdint>
#include <string>

namespace nodo::p2p {

/*
 * FinalizedArtifactAnnouncement is sent by a peer to announce that it holds
 * a finalized artifact at a given height.
 *
 * Security principle:
 * An announcement is not trusted data — it only tells the receiver that the
 * sender claims to have a finalized artifact at the given height. The receiver
 * must request and validate the full artifact before accepting it.
 */
class FinalizedArtifactAnnouncement {
public:
  FinalizedArtifactAnnouncement();

  FinalizedArtifactAnnouncement(std::string senderNodeId, std::string networkId,
                                std::string chainId, std::string genesisId,
                                std::uint64_t height, std::string blockHash,
                                std::string previousBlockHash,
                                std::string artifactDigest,
                                std::int64_t createdAt);

  const std::string &senderNodeId() const;
  const std::string &networkId() const;
  const std::string &chainId() const;
  const std::string &genesisId() const;
  std::uint64_t height() const;
  const std::string &blockHash() const;
  const std::string &previousBlockHash() const;
  const std::string &artifactDigest() const;
  std::int64_t createdAt() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_senderNodeId;
  std::string m_networkId;
  std::string m_chainId;
  std::string m_genesisId;
  std::uint64_t m_height;
  std::string m_blockHash;
  std::string m_previousBlockHash;
  std::string m_artifactDigest;
  std::int64_t m_createdAt;
};

/*
 * FinalizedArtifactRequest is sent to request a finalized artifact by height
 * and block hash from a peer that announced it.
 *
 * Security principle:
 * The request must include the expected block hash so the responder cannot
 * substitute a different artifact for the same height. The requester must
 * validate the artifact digest and content after receiving the response.
 */
class FinalizedArtifactRequest {
public:
  FinalizedArtifactRequest();

  FinalizedArtifactRequest(std::string requesterNodeId, std::string networkId,
                           std::string chainId, std::string genesisId,
                           std::uint64_t height, std::string expectedBlockHash,
                           std::string expectedArtifactDigest,
                           std::int64_t createdAt);

  const std::string &requesterNodeId() const;
  const std::string &networkId() const;
  const std::string &chainId() const;
  const std::string &genesisId() const;
  std::uint64_t height() const;
  const std::string &expectedBlockHash() const;
  const std::string &expectedArtifactDigest() const;
  std::int64_t createdAt() const;

  bool isValid() const;
  std::string serialize() const;

private:
  std::string m_requesterNodeId;
  std::string m_networkId;
  std::string m_chainId;
  std::string m_genesisId;
  std::uint64_t m_height;
  std::string m_expectedBlockHash;
  std::string m_expectedArtifactDigest;
  std::int64_t m_createdAt;
};

enum class FinalizedArtifactResponseStatus {
  ARTIFACT_FOUND,
  ARTIFACT_NOT_FOUND,
  HEIGHT_MISMATCH,
  HASH_MISMATCH,
  GENESIS_MISMATCH,
  NETWORK_MISMATCH,
  REQUEST_INVALID
};

std::string
finalizedArtifactResponseStatusToString(FinalizedArtifactResponseStatus status);

/*
 * FinalizedArtifactResponse carries the raw artifact content in response to a
 * FinalizedArtifactRequest, or a rejection with a precise reason.
 *
 * Security principle:
 * The receiver must not trust the artifact content without running the full
 * LocalArtifactImportService validation pipeline. A response carrying an
 * artifact is not an acceptance — it is candidate input for validation.
 */
class FinalizedArtifactResponse {
public:
  FinalizedArtifactResponse();

  static FinalizedArtifactResponse
  withArtifact(std::string responderNodeId, std::string networkId,
               std::string chainId, std::string genesisId, std::uint64_t height,
               std::string blockHash, std::string artifactDigest,
               std::string rawArtifactContents, std::int64_t createdAt);

  static FinalizedArtifactResponse
  rejected(std::string responderNodeId, FinalizedArtifactResponseStatus status,
           std::string reason, std::int64_t createdAt);

  bool hasArtifact() const;
  const std::string &responderNodeId() const;
  const std::string &networkId() const;
  const std::string &chainId() const;
  const std::string &genesisId() const;
  std::uint64_t height() const;
  const std::string &blockHash() const;
  const std::string &artifactDigest() const;
  const std::string &rawArtifactContents() const;
  FinalizedArtifactResponseStatus status() const;
  const std::string &reason() const;
  std::int64_t createdAt() const;

  bool isValid() const;
  std::string serialize() const;

private:
  FinalizedArtifactResponse(bool hasArtifact, std::string responderNodeId,
                            std::string networkId, std::string chainId,
                            std::string genesisId, std::uint64_t height,
                            std::string blockHash, std::string artifactDigest,
                            std::string rawArtifactContents,
                            FinalizedArtifactResponseStatus status,
                            std::string reason, std::int64_t createdAt);

  bool m_hasArtifact;
  std::string m_responderNodeId;
  std::string m_networkId;
  std::string m_chainId;
  std::string m_genesisId;
  std::uint64_t m_height;
  std::string m_blockHash;
  std::string m_artifactDigest;
  std::string m_rawArtifactContents;
  FinalizedArtifactResponseStatus m_status;
  std::string m_reason;
  std::int64_t m_createdAt;
};

} // namespace nodo::p2p

#endif
