#ifndef NODO_NODE_FAST_SYNC_SNAPSHOT_STORE_HPP
#define NODO_NODE_FAST_SYNC_SNAPSHOT_STORE_HPP

#include "node/FastSyncSnapshot.hpp"

#include <filesystem>
#include <optional>

namespace nodo::node {

class FastSyncSnapshotStore {
public:
  explicit FastSyncSnapshotStore(std::filesystem::path directoryPath);

  const std::filesystem::path &directoryPath() const;
  std::filesystem::path snapshotPath(std::uint64_t height) const;
  std::filesystem::path latestPointerPath() const;

  bool save(const FastSyncSnapshot &snapshot) const;
  std::optional<FastSyncSnapshot> load(std::uint64_t height) const;
  std::optional<FastSyncSnapshot> loadLatest() const;
  bool exists(std::uint64_t height) const;
  std::uint64_t latestHeight() const;

private:
  std::filesystem::path m_directoryPath;
};

} // namespace nodo::node

#endif
