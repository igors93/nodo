#include "node/FastSyncSnapshotStore.hpp"

#include "storage/AtomicFile.hpp"

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
    return true;
  } catch (...) {
    return false;
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
