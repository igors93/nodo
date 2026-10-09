#include "node/history/HistorySyncMessages.hpp"

#include "archive/ArchiveEncoding.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"
#include "utils/SafeScalar.hpp"

#include <stdexcept>
#include <type_traits>

namespace nodo::node {

namespace {

using Limits = HistorySyncLimits;
using serialization::CanonicalReader;
using serialization::CanonicalWriter;

void require(bool condition, const char *what) {
  if (!condition) {
    throw std::invalid_argument(std::string("History sync message: ") + what);
  }
}

void requireBytes(const std::vector<unsigned char> &bytes, std::size_t max,
                  const char *what) {
  require(!bytes.empty() && bytes.size() <= max, what);
}

void requireId(const std::string &value, const char *what) {
  require(archive::isDigestHex(value), what);
}

void requireRequest(std::uint64_t requestId) {
  require(requestId != 0, "request id must be non-zero");
}

void validate(const HistorySyncMessage &message) {
  std::visit(
      [](const auto &m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, CheckpointRequest>) {
          requireRequest(m.requestId);
        } else if constexpr (std::is_same_v<T, CheckpointResponse>) {
          requireRequest(m.requestId);
          requireBytes(m.checkpoint, Limits::kMaxCheckpointBytes, "checkpoint size");
        } else if constexpr (std::is_same_v<T, SnapshotManifestRequest>) {
          requireRequest(m.requestId);
          requireId(m.checkpointId, "checkpoint id");
        } else if constexpr (std::is_same_v<T, SnapshotManifestResponse>) {
          requireRequest(m.requestId);
          requireId(m.checkpointId, "checkpoint id");
          requireId(m.snapshotDigest, "snapshot digest");
          require(m.height > 0 && m.totalBytes > 0 && m.chunkBytes > 0 &&
                      m.chunkBytes <= Limits::kMaxChunkBytes,
                  "snapshot manifest geometry");
        } else if constexpr (std::is_same_v<T, SnapshotChunkRequest>) {
          requireRequest(m.requestId);
          requireId(m.checkpointId, "checkpoint id");
        } else if constexpr (std::is_same_v<T, SnapshotChunkResponse>) {
          requireRequest(m.requestId);
          requireBytes(m.bytes, Limits::kMaxChunkBytes, "chunk size");
          require(m.treeSize > 0 && m.chunkIndex < m.treeSize &&
                      m.proof.size() <= Limits::kMaxProofSiblings,
                  "chunk proof shape");
          for (const std::string &sibling : m.proof) {
            requireId(sibling, "chunk proof sibling");
          }
        } else if constexpr (std::is_same_v<T, HistoryRangeRequest>) {
          requireRequest(m.requestId);
          require(m.firstHeight > 0 && m.count >= 1 &&
                      m.count <= Limits::kMaxRangeBlocks,
                  "range must be 1-4 contiguous blocks after genesis");
        } else if constexpr (std::is_same_v<T, ArchiveIndexRequest>) {
          requireRequest(m.requestId);
          require(m.count >= 1 && m.count <= Limits::kMaxIndexEntries,
                  "archive index request count");
        } else if constexpr (std::is_same_v<T, ArchiveIndexResponse>) {
          requireRequest(m.requestId);
          require(!m.commitments.empty() &&
                      m.commitments.size() <= Limits::kMaxIndexEntries,
                  "archive index response count");
          for (const auto &commitment : m.commitments) {
            requireBytes(commitment, Limits::kMaxSegmentCommitmentBytes,
                         "segment commitment size");
          }
        } else if constexpr (std::is_same_v<T, ArchiveProviderAnnouncement>) {
          requireBytes(m.registration, Limits::kMaxRegistrationBytes,
                       "registration size");
        } else if constexpr (std::is_same_v<T, ArchiveSegmentsAnnouncement>) {
          require(utils::isSafeIdentifier(m.providerId, 128, "_-.:"),
                  "provider id");
          require(!m.ranges.empty() && m.ranges.size() <= Limits::kMaxSegmentRanges,
                  "segment range count");
          for (std::size_t index = 0; index < m.ranges.size(); ++index) {
            require(m.ranges[index].first <= m.ranges[index].second,
                    "segment range order");
            require(index == 0 ||
                        m.ranges[index].first > m.ranges[index - 1].second,
                    "segment ranges must ascend without overlap");
          }
          require(m.createdAt > 0, "announcement time");
          requireBytes(m.signature, Limits::kMaxSignatureBytes, "signature size");
        } else if constexpr (std::is_same_v<T, ArchivalChallengeMessage>) {
          requireBytes(m.challenge, Limits::kMaxChallengeBytes, "challenge size");
        } else if constexpr (std::is_same_v<T, ArchivalProofMessage>) {
          requireBytes(m.proof, Limits::kMaxArchivalProofBytes, "proof size");
        } else if constexpr (std::is_same_v<T, ReplicationStatusRequest>) {
          requireRequest(m.requestId);
          require(m.count >= 1 && m.count <= Limits::kMaxIndexEntries,
                  "replication request count");
        } else if constexpr (std::is_same_v<T, ReplicationStatusResponse>) {
          requireRequest(m.requestId);
          require(!m.entries.empty() &&
                      m.entries.size() <= Limits::kMaxIndexEntries,
                  "replication response count");
          for (std::size_t index = 1; index < m.entries.size(); ++index) {
            require(m.entries[index].segmentIndex >
                        m.entries[index - 1].segmentIndex,
                    "replication entries must ascend");
          }
        }
      },
      message);
}

void writeBody(CanonicalWriter &w, const HistorySyncMessage &message) {
  std::visit(
      [&w](const auto &m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, CheckpointRequest>) {
          w.writeUInt64(m.requestId);
          w.writeUInt64(m.height);
        } else if constexpr (std::is_same_v<T, CheckpointResponse>) {
          w.writeUInt64(m.requestId);
          w.writeBytes(m.checkpoint);
        } else if constexpr (std::is_same_v<T, SnapshotManifestRequest>) {
          w.writeUInt64(m.requestId);
          w.writeString(m.checkpointId);
        } else if constexpr (std::is_same_v<T, SnapshotManifestResponse>) {
          w.writeUInt64(m.requestId);
          w.writeString(m.checkpointId);
          w.writeUInt64(m.height);
          w.writeString(m.snapshotDigest);
          w.writeUInt64(m.totalBytes);
          w.writeUInt32(m.chunkBytes);
        } else if constexpr (std::is_same_v<T, SnapshotChunkRequest>) {
          w.writeUInt64(m.requestId);
          w.writeString(m.checkpointId);
          w.writeUInt64(m.chunkIndex);
        } else if constexpr (std::is_same_v<T, SnapshotChunkResponse>) {
          w.writeUInt64(m.requestId);
          w.writeUInt64(m.chunkIndex);
          w.writeUInt64(m.treeSize);
          w.writeBytes(m.bytes);
          w.writeUInt32(static_cast<std::uint32_t>(m.proof.size()));
          for (const std::string &sibling : m.proof) {
            w.writeString(sibling);
          }
        } else if constexpr (std::is_same_v<T, HistoryRangeRequest>) {
          w.writeUInt64(m.requestId);
          w.writeUInt64(m.firstHeight);
          w.writeUInt8(m.count);
        } else if constexpr (std::is_same_v<T, ArchiveIndexRequest> ||
                             std::is_same_v<T, ReplicationStatusRequest>) {
          w.writeUInt64(m.requestId);
          w.writeUInt64(m.fromSegment);
          w.writeUInt32(m.count);
        } else if constexpr (std::is_same_v<T, ArchiveIndexResponse>) {
          w.writeUInt64(m.requestId);
          w.writeUInt32(static_cast<std::uint32_t>(m.commitments.size()));
          for (const auto &commitment : m.commitments) {
            w.writeBytes(commitment);
          }
        } else if constexpr (std::is_same_v<T, ArchiveProviderAnnouncement>) {
          w.writeBytes(m.registration);
        } else if constexpr (std::is_same_v<T, ArchiveSegmentsAnnouncement>) {
          w.writeString(m.providerId);
          w.writeUInt32(static_cast<std::uint32_t>(m.ranges.size()));
          for (const auto &[from, to] : m.ranges) {
            w.writeUInt64(from);
            w.writeUInt64(to);
          }
          w.writeInt64(m.createdAt);
          w.writeBytes(m.signature);
        } else if constexpr (std::is_same_v<T, ArchivalChallengeMessage>) {
          w.writeBytes(m.challenge);
        } else if constexpr (std::is_same_v<T, ArchivalProofMessage>) {
          w.writeBytes(m.proof);
        } else if constexpr (std::is_same_v<T, ReplicationStatusResponse>) {
          w.writeUInt64(m.requestId);
          w.writeUInt32(static_cast<std::uint32_t>(m.entries.size()));
          for (const ReplicationStatusEntry &entry : m.entries) {
            w.writeUInt64(entry.segmentIndex);
            w.writeUInt32(entry.assignedReplicas);
            w.writeUInt32(entry.provenReplicas);
          }
        }
      },
      message);
}

std::uint32_t readCount(CanonicalReader &r, std::uint32_t max) {
  const std::uint32_t count = r.readUInt32();
  require(count <= max, "list length exceeds its limit");
  return count;
}

HistorySyncMessage readBody(CanonicalReader &r, HistorySyncMessageType type) {
  switch (type) {
  case HistorySyncMessageType::CHECKPOINT_REQUEST: {
    CheckpointRequest m;
    m.requestId = r.readUInt64();
    m.height = r.readUInt64();
    return m;
  }
  case HistorySyncMessageType::CHECKPOINT_RESPONSE: {
    CheckpointResponse m;
    m.requestId = r.readUInt64();
    m.checkpoint = r.readBytes();
    return m;
  }
  case HistorySyncMessageType::SNAPSHOT_MANIFEST_REQUEST: {
    SnapshotManifestRequest m;
    m.requestId = r.readUInt64();
    m.checkpointId = r.readString();
    return m;
  }
  case HistorySyncMessageType::SNAPSHOT_MANIFEST_RESPONSE: {
    SnapshotManifestResponse m;
    m.requestId = r.readUInt64();
    m.checkpointId = r.readString();
    m.height = r.readUInt64();
    m.snapshotDigest = r.readString();
    m.totalBytes = r.readUInt64();
    m.chunkBytes = r.readUInt32();
    return m;
  }
  case HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST: {
    SnapshotChunkRequest m;
    m.requestId = r.readUInt64();
    m.checkpointId = r.readString();
    m.chunkIndex = r.readUInt64();
    return m;
  }
  case HistorySyncMessageType::SNAPSHOT_CHUNK_RESPONSE: {
    SnapshotChunkResponse m;
    m.requestId = r.readUInt64();
    m.chunkIndex = r.readUInt64();
    m.treeSize = r.readUInt64();
    m.bytes = r.readBytes();
    const std::uint32_t count =
        readCount(r, static_cast<std::uint32_t>(Limits::kMaxProofSiblings));
    for (std::uint32_t index = 0; index < count; ++index) {
      m.proof.push_back(r.readString());
    }
    return m;
  }
  case HistorySyncMessageType::HISTORY_RANGE_REQUEST: {
    HistoryRangeRequest m;
    m.requestId = r.readUInt64();
    m.firstHeight = r.readUInt64();
    m.count = r.readUInt8();
    return m;
  }
  case HistorySyncMessageType::ARCHIVE_INDEX_REQUEST: {
    ArchiveIndexRequest m;
    m.requestId = r.readUInt64();
    m.fromSegment = r.readUInt64();
    m.count = r.readUInt32();
    return m;
  }
  case HistorySyncMessageType::ARCHIVE_INDEX_RESPONSE: {
    ArchiveIndexResponse m;
    m.requestId = r.readUInt64();
    const std::uint32_t count = readCount(r, Limits::kMaxIndexEntries);
    for (std::uint32_t index = 0; index < count; ++index) {
      m.commitments.push_back(r.readBytes());
    }
    return m;
  }
  case HistorySyncMessageType::ARCHIVE_PROVIDER_ANNOUNCEMENT: {
    ArchiveProviderAnnouncement m;
    m.registration = r.readBytes();
    return m;
  }
  case HistorySyncMessageType::ARCHIVE_SEGMENTS_ANNOUNCEMENT: {
    ArchiveSegmentsAnnouncement m;
    m.providerId = r.readString();
    const std::uint32_t count = readCount(r, Limits::kMaxSegmentRanges);
    for (std::uint32_t index = 0; index < count; ++index) {
      const std::uint64_t from = r.readUInt64();
      const std::uint64_t to = r.readUInt64();
      m.ranges.emplace_back(from, to);
    }
    m.createdAt = r.readInt64();
    m.signature = r.readBytes();
    return m;
  }
  case HistorySyncMessageType::ARCHIVAL_CHALLENGE: {
    ArchivalChallengeMessage m;
    m.challenge = r.readBytes();
    return m;
  }
  case HistorySyncMessageType::ARCHIVAL_PROOF: {
    ArchivalProofMessage m;
    m.proof = r.readBytes();
    return m;
  }
  case HistorySyncMessageType::REPLICATION_STATUS_REQUEST: {
    ReplicationStatusRequest m;
    m.requestId = r.readUInt64();
    m.fromSegment = r.readUInt64();
    m.count = r.readUInt32();
    return m;
  }
  case HistorySyncMessageType::REPLICATION_STATUS_RESPONSE: {
    ReplicationStatusResponse m;
    m.requestId = r.readUInt64();
    const std::uint32_t count = readCount(r, Limits::kMaxIndexEntries);
    for (std::uint32_t index = 0; index < count; ++index) {
      ReplicationStatusEntry entry;
      entry.segmentIndex = r.readUInt64();
      entry.assignedReplicas = r.readUInt32();
      entry.provenReplicas = r.readUInt32();
      m.entries.push_back(entry);
    }
    return m;
  }
  }
  throw std::invalid_argument("Unknown history sync message type.");
}

} // namespace

std::string historySyncMessageTypeToString(HistorySyncMessageType type) {
  switch (type) {
  case HistorySyncMessageType::CHECKPOINT_REQUEST:
    return "CHECKPOINT_REQUEST";
  case HistorySyncMessageType::CHECKPOINT_RESPONSE:
    return "CHECKPOINT_RESPONSE";
  case HistorySyncMessageType::SNAPSHOT_MANIFEST_REQUEST:
    return "SNAPSHOT_MANIFEST_REQUEST";
  case HistorySyncMessageType::SNAPSHOT_MANIFEST_RESPONSE:
    return "SNAPSHOT_MANIFEST_RESPONSE";
  case HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST:
    return "SNAPSHOT_CHUNK_REQUEST";
  case HistorySyncMessageType::SNAPSHOT_CHUNK_RESPONSE:
    return "SNAPSHOT_CHUNK_RESPONSE";
  case HistorySyncMessageType::HISTORY_RANGE_REQUEST:
    return "HISTORY_RANGE_REQUEST";
  case HistorySyncMessageType::ARCHIVE_INDEX_REQUEST:
    return "ARCHIVE_INDEX_REQUEST";
  case HistorySyncMessageType::ARCHIVE_INDEX_RESPONSE:
    return "ARCHIVE_INDEX_RESPONSE";
  case HistorySyncMessageType::ARCHIVE_PROVIDER_ANNOUNCEMENT:
    return "ARCHIVE_PROVIDER_ANNOUNCEMENT";
  case HistorySyncMessageType::ARCHIVE_SEGMENTS_ANNOUNCEMENT:
    return "ARCHIVE_SEGMENTS_ANNOUNCEMENT";
  case HistorySyncMessageType::ARCHIVAL_CHALLENGE:
    return "ARCHIVAL_CHALLENGE";
  case HistorySyncMessageType::ARCHIVAL_PROOF:
    return "ARCHIVAL_PROOF";
  case HistorySyncMessageType::REPLICATION_STATUS_REQUEST:
    return "REPLICATION_STATUS_REQUEST";
  case HistorySyncMessageType::REPLICATION_STATUS_RESPONSE:
    return "REPLICATION_STATUS_RESPONSE";
  }
  return "UNKNOWN";
}

HistorySyncMessageType HistorySyncCodec::typeOf(const HistorySyncMessage &message) {
  // The variant order mirrors the wire tags.
  return static_cast<HistorySyncMessageType>(message.index() + 1);
}

std::vector<unsigned char>
HistorySyncCodec::encode(const HistorySyncMessage &message) {
  validate(message);
  CanonicalWriter writer;
  writer.writeUInt16(static_cast<std::uint16_t>(typeOf(message)));
  writeBody(writer, message);
  require(writer.size() <= Limits::kMaxMessageBytes, "message exceeds the cap");
  return writer.bytes();
}

HistorySyncMessage
HistorySyncCodec::decode(const std::vector<unsigned char> &bytes) {
  require(!bytes.empty() && bytes.size() <= Limits::kMaxMessageBytes,
          "message size is out of range");
  CanonicalReader reader(bytes, Limits::kMaxMessageBytes);
  const std::uint16_t tag = reader.readUInt16();
  require(tag >= 1 &&
              tag <= static_cast<std::uint16_t>(
                         HistorySyncMessageType::REPLICATION_STATUS_RESPONSE),
          "unknown message type");
  HistorySyncMessage message =
      readBody(reader, static_cast<HistorySyncMessageType>(tag));
  reader.requireFullyConsumed();
  validate(message);
  require(encode(message) == bytes, "message encoding is not canonical");
  return message;
}

std::optional<HistorySyncMessageType>
HistorySyncCodec::expectedResponse(HistorySyncMessageType request) {
  switch (request) {
  case HistorySyncMessageType::CHECKPOINT_REQUEST:
    return HistorySyncMessageType::CHECKPOINT_RESPONSE;
  case HistorySyncMessageType::SNAPSHOT_MANIFEST_REQUEST:
    return HistorySyncMessageType::SNAPSHOT_MANIFEST_RESPONSE;
  case HistorySyncMessageType::SNAPSHOT_CHUNK_REQUEST:
    return HistorySyncMessageType::SNAPSHOT_CHUNK_RESPONSE;
  case HistorySyncMessageType::ARCHIVE_INDEX_REQUEST:
    return HistorySyncMessageType::ARCHIVE_INDEX_RESPONSE;
  case HistorySyncMessageType::REPLICATION_STATUS_REQUEST:
    return HistorySyncMessageType::REPLICATION_STATUS_RESPONSE;
  default:
    return std::nullopt;
  }
}

HistoryRequestGuard::HistoryRequestGuard() : HistoryRequestGuard(Limits{}) {}

HistoryRequestGuard::HistoryRequestGuard(Limits limits)
    : m_limits(limits), m_peers() {}

std::optional<std::uint64_t>
HistoryRequestGuard::openRequest(const std::string &peerId,
                                 HistorySyncMessageType requestType,
                                 std::int64_t now) {
  const auto expected = HistorySyncCodec::expectedResponse(requestType);
  if (!expected || peerId.empty()) {
    return std::nullopt;
  }
  if (m_peers.count(peerId) == 0 && m_peers.size() >= m_limits.maxTrackedPeers) {
    return std::nullopt;
  }
  PeerState &peer = m_peers[peerId];
  if (peer.requests.size() >= m_limits.maxOutstandingPerPeer) {
    return std::nullopt;
  }
  const std::uint64_t id = m_nextRequestId++;
  peer.requests.emplace(id, Outstanding{*expected, now});
  return id;
}

bool HistoryRequestGuard::acceptResponse(const std::string &peerId,
                                         std::uint64_t requestId,
                                         HistorySyncMessageType responseType,
                                         std::int64_t now) {
  const auto peer = m_peers.find(peerId);
  if (peer == m_peers.end()) {
    return false;
  }
  const auto request = peer->second.requests.find(requestId);
  if (request == peer->second.requests.end() ||
      request->second.expected != responseType ||
      now - request->second.openedAt > m_limits.requestTimeoutSeconds) {
    return false;
  }
  peer->second.requests.erase(request);
  return true;
}

bool HistoryRequestGuard::admitServe(const std::string &peerId,
                                     std::uint64_t responseBytes,
                                     std::int64_t now) {
  if (peerId.empty() || responseBytes > m_limits.maxServedBytesPerWindow) {
    return false;
  }
  if (m_peers.count(peerId) == 0 && m_peers.size() >= m_limits.maxTrackedPeers) {
    return false;
  }
  PeerState &peer = m_peers[peerId];
  if (now - peer.windowStart >= m_limits.windowSeconds) {
    peer.windowStart = now;
    peer.servedBytes = 0;
  }
  if (peer.servedBytes + responseBytes > m_limits.maxServedBytesPerWindow) {
    return false;
  }
  peer.servedBytes += responseBytes;
  return true;
}

std::size_t HistoryRequestGuard::expire(std::int64_t now) {
  std::size_t expired = 0;
  for (auto peer = m_peers.begin(); peer != m_peers.end();) {
    for (auto request = peer->second.requests.begin();
         request != peer->second.requests.end();) {
      if (now - request->second.openedAt > m_limits.requestTimeoutSeconds) {
        request = peer->second.requests.erase(request);
        ++expired;
      } else {
        ++request;
      }
    }
    if (peer->second.requests.empty() &&
        now - peer->second.windowStart >= m_limits.windowSeconds) {
      peer = m_peers.erase(peer);
    } else {
      ++peer;
    }
  }
  return expired;
}

std::size_t HistoryRequestGuard::outstanding(const std::string &peerId) const {
  const auto peer = m_peers.find(peerId);
  return peer == m_peers.end() ? 0 : peer->second.requests.size();
}

} // namespace nodo::node
