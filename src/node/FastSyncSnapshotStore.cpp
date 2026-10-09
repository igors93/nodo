#include "node/FastSyncSnapshotStore.hpp"

#include "storage/AtomicFile.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace nodo::node {

namespace {

bool parseU64Strict(const std::string &value, std::uint64_t &out) {
  if (value.empty()) {
    return false;
  }
  for (const char c : value) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  try {
    std::size_t used = 0;
    const unsigned long long parsed = std::stoull(value, &used);
    if (used != value.size()) {
      return false;
    }
    out = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace

FastSyncSnapshotStore::FastSyncSnapshotStore(
    std::filesystem::path directoryPath)
    : m_directoryPath(std::move(directoryPath)) {}

const std::filesystem::path &FastSyncSnapshotStore::directoryPath() const {
  return m_directoryPath;
}

std::filesystem::path
FastSyncSnapshotStore::snapshotPath(std::uint64_t height) const {
  return m_directoryPath / (std::to_string(height) + ".fastsnap");
}

std::filesystem::path FastSyncSnapshotStore::latestPointerPath() const {
  return m_directoryPath / "latest";
}

bool FastSyncSnapshotStore::save(const FastSyncSnapshot &snapshot) const {
  if (!snapshot.isValid()) {
    return false;
  }
  try {
    std::filesystem::create_directories(m_directoryPath);
    storage::AtomicFile::writeTextFile(snapshotPath(snapshot.blockHeight()),
                                       snapshot.serialize());
    storage::AtomicFile::writeTextFile(
        latestPointerPath(), std::to_string(snapshot.blockHeight()) + "\n");
    pruneSupersededSnapshots(snapshot.blockHeight());
    return true;
  } catch (...) {
    return false;
  }
}

void FastSyncSnapshotStore::pruneSupersededSnapshots(
    std::uint64_t keptHeight) const {
  // Legacy fast-sync snapshots are a per-block cache of derived state, not
  // history: reload never reads them and finalized state checkpoints are the
  // durable snapshots (ADR 0014). Keeping every one grew storage with every
  // block, so only the snapshot just written and the newest one survive.
  std::vector<std::uint64_t> heights;
  for (const auto &entry :
       std::filesystem::directory_iterator(m_directoryPath)) {
    std::uint64_t height = 0;
    if (entry.is_regular_file() && !entry.is_symlink() &&
        entry.path().extension() == ".fastsnap" &&
        parseU64Strict(entry.path().stem().string(), height) &&
        std::to_string(height) == entry.path().stem().string()) {
      heights.push_back(height);
    }
  }
  if (heights.empty()) {
    return;
  }
  const std::uint64_t newest =
      *std::max_element(heights.begin(), heights.end());
  for (const std::uint64_t height : heights) {
    if (height != keptHeight && height != newest) {
      std::error_code ec;
      std::filesystem::remove(snapshotPath(height), ec);
    }
  }
}

std::optional<FastSyncSnapshot>
FastSyncSnapshotStore::load(std::uint64_t height) const {
  const std::filesystem::path path = snapshotPath(height);
  if (height == 0 || !std::filesystem::exists(path)) {
    return std::nullopt;
  }
  try {
    return FastSyncSnapshot::deserialize(
        storage::AtomicFile::readTextFile(path));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<FastSyncSnapshot> FastSyncSnapshotStore::loadLatest() const {
  const std::uint64_t height = latestHeight();
  if (height == 0) {
    return std::nullopt;
  }
  return load(height);
}

bool FastSyncSnapshotStore::exists(std::uint64_t height) const {
  return height != 0 && std::filesystem::exists(snapshotPath(height));
}

std::uint64_t FastSyncSnapshotStore::latestHeight() const {
  if (!std::filesystem::exists(latestPointerPath())) {
    return 0;
  }
  try {
    std::string value = storage::AtomicFile::readTextFile(latestPointerPath());
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
      value.pop_back();
    }
    std::uint64_t parsed = 0;
    if (!parseU64Strict(value, parsed)) {
      return 0;
    }
    return parsed;
  } catch (...) {
    return 0;
  }
}

} // namespace nodo::node
