#include "node/history/HistoryStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

constexpr std::uint64_t kMaxSegmentCommitmentBytes = 4096;

std::vector<unsigned char> toBytes(const std::string &text) {
  return std::vector<unsigned char>(text.begin(), text.end());
}

std::string toText(const std::vector<unsigned char> &bytes) {
  return std::string(bytes.begin(), bytes.end());
}

// Reads a regular, non-symlink file no larger than maxBytes. Returns nullopt
// when the file does not exist.
std::optional<std::vector<unsigned char>>
readBoundedFile(const std::filesystem::path &path, std::uint64_t maxBytes) {
  std::error_code ec;
  const std::filesystem::file_status status =
      std::filesystem::symlink_status(path, ec);
  if (ec || status.type() == std::filesystem::file_type::not_found) {
    return std::nullopt;
  }
  if (status.type() != std::filesystem::file_type::regular) {
    throw std::runtime_error("History path is not a regular file: " +
                             path.string());
  }
  const std::uintmax_t size = std::filesystem::file_size(path);
  if (size == 0 || size > maxBytes) {
    throw std::runtime_error("History file size is out of range: " +
                             path.string());
  }
  return toBytes(storage::AtomicFile::readTextFile(path));
}

std::vector<std::uint64_t> listNumbered(const std::filesystem::path &directory,
                                        const std::string &extension) {
  std::vector<std::uint64_t> numbers;
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec)) {
    return numbers;
  }
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file() || entry.is_symlink()) {
      continue;
    }
    const auto number =
        HistoryStore::parseNumberedFileName(entry.path(), extension);
    if (number) {
      numbers.push_back(*number);
    }
  }
  std::sort(numbers.begin(), numbers.end());
  return numbers;
}

HistoryWriteResult result(HistoryWriteStatus status, std::string reason = "") {
  return HistoryWriteResult{status, std::move(reason)};
}

HistoryWriteResult writeOnce(const std::filesystem::path &path,
                             const std::vector<unsigned char> &bytes,
                             std::uint64_t maxBytes) {
  try {
    const auto existing = readBoundedFile(path, maxBytes);
    if (existing) {
      return *existing == bytes
                 ? result(HistoryWriteStatus::ALREADY_PRESENT)
                 : result(HistoryWriteStatus::CONFLICT,
                          "a different object already exists at " +
                              path.filename().string());
    }
    std::filesystem::create_directories(path.parent_path());
    storage::AtomicFile::writeTextFile(path, toText(bytes));
    return result(HistoryWriteStatus::WRITTEN);
  } catch (const std::exception &error) {
    return result(HistoryWriteStatus::IO_ERROR, error.what());
  }
}

} // namespace

std::string historyWriteStatusToString(HistoryWriteStatus status) {
  switch (status) {
  case HistoryWriteStatus::WRITTEN:
    return "WRITTEN";
  case HistoryWriteStatus::ALREADY_PRESENT:
    return "ALREADY_PRESENT";
  case HistoryWriteStatus::CONFLICT:
    return "CONFLICT";
  case HistoryWriteStatus::IO_ERROR:
    return "IO_ERROR";
  }
  return "IO_ERROR";
}

HistoryStore::HistoryStore(NodeDataDirectoryConfig directory)
    : m_directory(std::move(directory)) {}

const NodeDataDirectoryConfig &HistoryStore::directory() const {
  return m_directory;
}

std::filesystem::path HistoryStore::checkpointPath(std::uint64_t height) const {
  return m_directory.checkpointsDirectoryPath() /
         (std::to_string(height) + ".checkpoint");
}

std::filesystem::path HistoryStore::snapshotPath(std::uint64_t height) const {
  return m_directory.checkpointSnapshotsDirectoryPath() /
         (std::to_string(height) + ".snapshot");
}

std::filesystem::path
HistoryStore::segmentCommitmentPath(std::uint64_t segmentIndex) const {
  return m_directory.archiveCommitmentsDirectoryPath() /
         (std::to_string(segmentIndex) + ".segment");
}

HistoryWriteResult
HistoryStore::saveCheckpoint(const FinalizedStateCheckpoint &checkpoint) const {
  if (!checkpoint.isStructurallyValid()) {
    return result(HistoryWriteStatus::IO_ERROR,
                  "refusing to store a structurally invalid checkpoint");
  }
  try {
    const std::optional<FinalizedStateCheckpoint> existing =
        loadCheckpoint(checkpoint.height());
    if (existing) {
      if (existing->checkpointId() == checkpoint.checkpointId()) {
        // Another valid vote subset for the same block; keep the first.
        return result(HistoryWriteStatus::ALREADY_PRESENT);
      }
      const std::filesystem::path evidence =
          m_directory.checkpointConflictsDirectoryPath() /
          (std::to_string(checkpoint.height()) + "-" +
           checkpoint.checkpointId() + ".checkpoint");
      std::filesystem::create_directories(evidence.parent_path());
      const std::vector<unsigned char> encoded = checkpoint.encode();
      storage::AtomicFile::writeTextFile(evidence, toText(encoded));
      return result(HistoryWriteStatus::CONFLICT,
                    "conflicting checkpoint at height " +
                        std::to_string(checkpoint.height()) +
                        " preserved for investigation");
    }
  } catch (const std::exception &error) {
    return result(HistoryWriteStatus::IO_ERROR, error.what());
  }
  return writeOnce(checkpointPath(checkpoint.height()), checkpoint.encode(),
                   FinalizedStateCheckpoint::kMaxEncodedBytes);
}

std::optional<FinalizedStateCheckpoint>
HistoryStore::loadCheckpoint(std::uint64_t height) const {
  const auto bytes = readBoundedFile(checkpointPath(height),
                                     FinalizedStateCheckpoint::kMaxEncodedBytes);
  if (!bytes) {
    return std::nullopt;
  }
  FinalizedStateCheckpoint checkpoint = FinalizedStateCheckpoint::decode(*bytes);
  if (checkpoint.height() != height) {
    throw std::runtime_error("Checkpoint file name does not match its height.");
  }
  return checkpoint;
}

std::vector<std::uint64_t> HistoryStore::checkpointHeights() const {
  return listNumbered(m_directory.checkpointsDirectoryPath(), ".checkpoint");
}

std::optional<FinalizedStateCheckpoint> HistoryStore::latestCheckpoint() const {
  const std::vector<std::uint64_t> heights = checkpointHeights();
  if (heights.empty()) {
    return std::nullopt;
  }
  return loadCheckpoint(heights.back());
}

std::size_t HistoryStore::conflictCount() const {
  std::size_t count = 0;
  std::error_code ec;
  const auto directory = m_directory.checkpointConflictsDirectoryPath();
  if (!std::filesystem::is_directory(directory, ec)) {
    return 0;
  }
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".checkpoint") {
      ++count;
    }
  }
  return count;
}

HistoryWriteResult
HistoryStore::saveSnapshot(std::uint64_t height,
                           const std::vector<unsigned char> &encoded) const {
  if (height == 0 || encoded.empty()) {
    return result(HistoryWriteStatus::IO_ERROR, "invalid snapshot input");
  }
  return writeOnce(snapshotPath(height), encoded,
                   config::HistoryParameters::kMaxSnapshotBytes);
}

std::optional<std::vector<unsigned char>>
HistoryStore::loadSnapshotBytes(std::uint64_t height,
                                std::uint64_t maxBytes) const {
  return readBoundedFile(snapshotPath(height), maxBytes);
}

std::optional<FullProtocolStateSnapshot>
HistoryStore::loadSnapshot(std::uint64_t height, std::uint64_t maxBytes) const {
  const auto bytes = loadSnapshotBytes(height, maxBytes);
  if (!bytes) {
    return std::nullopt;
  }
  FullProtocolStateSnapshot snapshot =
      FullProtocolStateSnapshot::decode(*bytes, maxBytes);
  if (snapshot.height() != height) {
    throw std::runtime_error("Snapshot file name does not match its height.");
  }
  return snapshot;
}

std::vector<std::uint64_t> HistoryStore::snapshotHeights() const {
  return listNumbered(m_directory.checkpointSnapshotsDirectoryPath(),
                      ".snapshot");
}

HistoryWriteResult HistoryStore::saveSegmentCommitment(
    const archive::ArchiveSegmentCommitment &commitment) const {
  if (!commitment.isValid()) {
    return result(HistoryWriteStatus::IO_ERROR,
                  "refusing to store an invalid segment commitment");
  }
  return writeOnce(segmentCommitmentPath(commitment.segmentIndex()),
                   commitment.encode(), kMaxSegmentCommitmentBytes);
}

std::optional<archive::ArchiveSegmentCommitment>
HistoryStore::loadSegmentCommitment(std::uint64_t segmentIndex) const {
  const auto bytes = readBoundedFile(segmentCommitmentPath(segmentIndex),
                                     kMaxSegmentCommitmentBytes);
  if (!bytes) {
    return std::nullopt;
  }
  archive::ArchiveSegmentCommitment commitment =
      archive::ArchiveSegmentCommitment::decode(*bytes);
  if (commitment.segmentIndex() != segmentIndex) {
    throw std::runtime_error("Segment commitment file name does not match.");
  }
  return commitment;
}

std::optional<std::vector<archive::ArchiveSegmentCommitment>>
HistoryStore::loadSegmentCommitments(std::uint64_t count) const {
  std::vector<archive::ArchiveSegmentCommitment> commitments;
  commitments.reserve(static_cast<std::size_t>(count));
  try {
    for (std::uint64_t index = 0; index < count; ++index) {
      auto commitment = loadSegmentCommitment(index);
      if (!commitment) {
        return std::nullopt;
      }
      commitments.push_back(std::move(*commitment));
    }
  } catch (const std::exception &) {
    return std::nullopt;
  }
  if (!archive::ArchiveIndex::isContiguous(commitments)) {
    return std::nullopt;
  }
  return commitments;
}

std::uint64_t HistoryStore::contiguousSegmentCommitmentCount() const {
  const std::vector<std::uint64_t> indices =
      listNumbered(m_directory.archiveCommitmentsDirectoryPath(), ".segment");
  std::uint64_t count = 0;
  for (const std::uint64_t index : indices) {
    if (index != count) {
      break;
    }
    ++count;
  }
  return count;
}

std::optional<std::uint64_t>
HistoryStore::parseNumberedFileName(const std::filesystem::path &path,
                                    const std::string &extension) {
  if (path.extension() != extension) {
    return std::nullopt;
  }
  const std::string stem = path.stem().string();
  if (stem.empty() || stem.size() > 20 ||
      stem.find_first_not_of("0123456789") != std::string::npos ||
      (stem.size() > 1 && stem.front() == '0')) {
    return std::nullopt;
  }
  try {
    std::size_t used = 0;
    const unsigned long long value = std::stoull(stem, &used);
    if (used != stem.size()) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(value);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

} // namespace nodo::node
