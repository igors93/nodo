#ifndef NODO_NODE_HISTORY_HISTORY_SYNC_MESSAGES_HPP
#define NODO_NODE_HISTORY_HISTORY_SYNC_MESSAGES_HPP

#include "core/ProtocolLimits.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nodo::node {

/*
 * Wire messages for checkpoint bootstrap, snapshot transfer, historical
 * ranges and Proof of Archival (ADR 0014). Every message is a canonical
 * binary payload with a u16 type tag; decoding is strict (exact field order,
 * bounded counts and byte lengths, no trailing bytes). Requests name data
 * only by height, segment index or 64-hex id, never by path. Embedded
 * protocol objects (checkpoints, registrations, challenges, proofs) are
 * bounded opaque bytes here and are decoded and verified by their own
 * strict codecs before use. Nothing in a message is trusted.
 */
enum class HistorySyncMessageType : std::uint16_t {
  CHECKPOINT_REQUEST = 1,
  CHECKPOINT_RESPONSE = 2,
  SNAPSHOT_MANIFEST_REQUEST = 3,
  SNAPSHOT_MANIFEST_RESPONSE = 4,
  SNAPSHOT_CHUNK_REQUEST = 5,
  SNAPSHOT_CHUNK_RESPONSE = 6,
  HISTORY_RANGE_REQUEST = 7,
  ARCHIVE_INDEX_REQUEST = 8,
  ARCHIVE_INDEX_RESPONSE = 9,
  ARCHIVE_PROVIDER_ANNOUNCEMENT = 10,
  ARCHIVE_SEGMENTS_ANNOUNCEMENT = 11,
  ARCHIVAL_CHALLENGE = 12,
  ARCHIVAL_PROOF = 13,
  REPLICATION_STATUS_REQUEST = 14,
  REPLICATION_STATUS_RESPONSE = 15
};

std::string historySyncMessageTypeToString(HistorySyncMessageType type);

struct HistorySyncLimits {
  static constexpr std::size_t kMaxMessageBytes =
      core::ProtocolLimits::MAX_NETWORK_PAYLOAD_BYTES;
  static constexpr std::size_t kMaxCheckpointBytes = 1024 * 1024 + 4096;
  static constexpr std::size_t kMaxChunkBytes = 256 * 1024;
  static constexpr std::size_t kMaxProofSiblings = 64;
  static constexpr std::uint8_t kMaxRangeBlocks = 4;
  static constexpr std::uint32_t kMaxIndexEntries = 1024;
  static constexpr std::size_t kMaxSegmentCommitmentBytes = 4096;
  static constexpr std::size_t kMaxRegistrationBytes = 4096;
  static constexpr std::size_t kMaxChallengeBytes = 8192;
  static constexpr std::size_t kMaxArchivalProofBytes = 4 * 1024 * 1024 - 1024;
  static constexpr std::uint32_t kMaxSegmentRanges = 64;
  static constexpr std::size_t kMaxSignatureBytes = 512;
};

struct CheckpointRequest {
  std::uint64_t requestId = 0;
  std::uint64_t height = 0; // 0 asks for the latest checkpoint
};
struct CheckpointResponse {
  std::uint64_t requestId = 0;
  std::vector<unsigned char> checkpoint;
};
struct SnapshotManifestRequest {
  std::uint64_t requestId = 0;
  std::string checkpointId;
};
struct SnapshotManifestResponse {
  std::uint64_t requestId = 0;
  std::string checkpointId;
  std::uint64_t height = 0;
  std::string snapshotDigest;
  std::uint64_t totalBytes = 0;
  std::uint32_t chunkBytes = 0;
};
struct SnapshotChunkRequest {
  std::uint64_t requestId = 0;
  std::string checkpointId;
  std::uint64_t chunkIndex = 0;
};
struct SnapshotChunkResponse {
  std::uint64_t requestId = 0;
  std::uint64_t chunkIndex = 0;
  std::uint64_t treeSize = 0;
  std::vector<unsigned char> bytes;
  std::vector<std::string> proof; // sibling digests, hex
};
struct HistoryRangeRequest {
  std::uint64_t requestId = 0;
  std::uint64_t firstHeight = 0;
  std::uint8_t count = 0;
};
struct ArchiveIndexRequest {
  std::uint64_t requestId = 0;
  std::uint64_t fromSegment = 0;
  std::uint32_t count = 0;
};
struct ArchiveIndexResponse {
  std::uint64_t requestId = 0;
  std::vector<std::vector<unsigned char>> commitments;
};
struct ArchiveProviderAnnouncement {
  std::vector<unsigned char> registration;
};
struct ArchiveSegmentsAnnouncement {
  std::string providerId;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
  std::int64_t createdAt = 0;
  std::vector<unsigned char> signature;
};
struct ArchivalChallengeMessage {
  std::vector<unsigned char> challenge;
};
struct ArchivalProofMessage {
  std::vector<unsigned char> proof;
};
struct ReplicationStatusRequest {
  std::uint64_t requestId = 0;
  std::uint64_t fromSegment = 0;
  std::uint32_t count = 0;
};
struct ReplicationStatusEntry {
  std::uint64_t segmentIndex = 0;
  std::uint32_t assignedReplicas = 0;
  std::uint32_t provenReplicas = 0;
};
struct ReplicationStatusResponse {
  std::uint64_t requestId = 0;
  std::vector<ReplicationStatusEntry> entries;
};

using HistorySyncMessage =
    std::variant<CheckpointRequest, CheckpointResponse, SnapshotManifestRequest,
                 SnapshotManifestResponse, SnapshotChunkRequest,
                 SnapshotChunkResponse, HistoryRangeRequest,
                 ArchiveIndexRequest, ArchiveIndexResponse,
                 ArchiveProviderAnnouncement, ArchiveSegmentsAnnouncement,
                 ArchivalChallengeMessage, ArchivalProofMessage,
                 ReplicationStatusRequest, ReplicationStatusResponse>;

class HistorySyncCodec {
public:
  static HistorySyncMessageType typeOf(const HistorySyncMessage &message);
  // Throws std::invalid_argument for any message outside its limits.
  static std::vector<unsigned char> encode(const HistorySyncMessage &message);
  static HistorySyncMessage decode(const std::vector<unsigned char> &bytes);
  // The response type a request expects; nullopt for non-requests.
  static std::optional<HistorySyncMessageType>
  expectedResponse(HistorySyncMessageType request);
};

/*
 * Per-peer request accounting. Responses are accepted only for a request
 * this node opened (same peer, id and expected type) and before it expires;
 * anything else is unsolicited. Serving is charged by response bytes in a
 * sliding window, which bounds amplification: a tiny request cannot make
 * this node send unbounded data. Tracked peers and outstanding requests are
 * capped, so a flood cannot grow memory without bound.
 */
class HistoryRequestGuard {
public:
  struct Limits {
    std::size_t maxOutstandingPerPeer = 8;
    std::size_t maxTrackedPeers = 4096;
    std::uint64_t maxServedBytesPerWindow = 64ULL * 1024 * 1024;
    std::int64_t windowSeconds = 60;
    std::int64_t requestTimeoutSeconds = 30;
  };

  HistoryRequestGuard();
  explicit HistoryRequestGuard(Limits limits);

  std::optional<std::uint64_t> openRequest(const std::string &peerId,
                                           HistorySyncMessageType requestType,
                                           std::int64_t now);
  bool acceptResponse(const std::string &peerId, std::uint64_t requestId,
                      HistorySyncMessageType responseType, std::int64_t now);
  bool admitServe(const std::string &peerId, std::uint64_t responseBytes,
                  std::int64_t now);
  std::size_t expire(std::int64_t now);
  std::size_t outstanding(const std::string &peerId) const;

private:
  struct Outstanding {
    HistorySyncMessageType expected;
    std::int64_t openedAt;
  };
  struct PeerState {
    std::map<std::uint64_t, Outstanding> requests;
    std::int64_t windowStart = 0;
    std::uint64_t servedBytes = 0;
  };

  Limits m_limits;
  std::uint64_t m_nextRequestId = 1;
  std::map<std::string, PeerState> m_peers;
};

} // namespace nodo::node

#endif
