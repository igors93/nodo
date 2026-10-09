#ifndef NODO_NODE_HISTORY_HISTORY_STORE_HPP
#define NODO_NODE_HISTORY_HISTORY_STORE_HPP

#include "archive/ArchiveSegment.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/history/FinalizedStateCheckpoint.hpp"
#include "node/history/FullProtocolStateSnapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

enum class HistoryWriteStatus { WRITTEN, ALREADY_PRESENT, CONFLICT, IO_ERROR };

std::string historyWriteStatusToString(HistoryWriteStatus status);

struct HistoryWriteResult {
  HistoryWriteStatus status = HistoryWriteStatus::IO_ERROR;
  std::string reason;

  bool stored() const {
    return status == HistoryWriteStatus::WRITTEN ||
           status == HistoryWriteStatus::ALREADY_PRESENT;
  }
};

/*
 * HistoryStore persists checkpoints, checkpoint snapshots and archive segment
 * commitments under <data-dir>/history (storage schema v2).
 *
 * Every write is atomic (temporary file plus rename). An existing object is
 * never overwritten with a different one: a second checkpoint with a
 * different id at the same height is a safety fault, so it is preserved under
 * checkpoints/conflicts/ for investigation and reported as CONFLICT. Reads
 * enforce size caps before allocating, refuse symbolic links, and decode
 * strictly. File names are derived only from heights and segment indices.
 */
class HistoryStore {
public:
  explicit HistoryStore(NodeDataDirectoryConfig directory);

  const NodeDataDirectoryConfig &directory() const;

  std::filesystem::path checkpointPath(std::uint64_t height) const;
  std::filesystem::path snapshotPath(std::uint64_t height) const;
  std::filesystem::path segmentCommitmentPath(std::uint64_t segmentIndex) const;

  HistoryWriteResult saveCheckpoint(
      const FinalizedStateCheckpoint &checkpoint) const;
  // nullopt when absent; throws std::runtime_error when present but corrupt.
  std::optional<FinalizedStateCheckpoint>
  loadCheckpoint(std::uint64_t height) const;
  std::vector<std::uint64_t> checkpointHeights() const;
  std::optional<FinalizedStateCheckpoint> latestCheckpoint() const;
  std::size_t conflictCount() const;

  HistoryWriteResult saveSnapshot(std::uint64_t height,
                                  const std::vector<unsigned char> &encoded) const;
  std::optional<std::vector<unsigned char>>
  loadSnapshotBytes(std::uint64_t height, std::uint64_t maxBytes) const;
  std::optional<FullProtocolStateSnapshot>
  loadSnapshot(std::uint64_t height, std::uint64_t maxBytes) const;
  std::vector<std::uint64_t> snapshotHeights() const;

  HistoryWriteResult saveSegmentCommitment(
      const archive::ArchiveSegmentCommitment &commitment) const;
  std::optional<archive::ArchiveSegmentCommitment>
  loadSegmentCommitment(std::uint64_t segmentIndex) const;
  // Segments 0..count-1, or nullopt if any is missing or inconsistent.
  std::optional<std::vector<archive::ArchiveSegmentCommitment>>
  loadSegmentCommitments(std::uint64_t count) const;
  // Number of commitments stored contiguously from segment zero.
  std::uint64_t contiguousSegmentCommitmentCount() const;

  // Parses "<decimal>.<extension>" with canonical decimal and no other text.
  static std::optional<std::uint64_t>
  parseNumberedFileName(const std::filesystem::path &path,
                        const std::string &extension);

private:
  NodeDataDirectoryConfig m_directory;
};

} // namespace nodo::node

#endif
